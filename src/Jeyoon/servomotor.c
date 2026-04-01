#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"

#define SERVO_PIN 16// 서보모터의 PWM 신호를 출력할 GPIO 핀 번호입니다. GPIO 16을 사용합니다.
#define SERVO_PERIOD_US 20000// 서보모터의 PWM 신호의 주기를 20ms로 설정합니다. 
#define SERVO_HZ 50// 20ms 주기는 50Hz에 해당합니다.

#define MQTT_HOST "221.145.210.230"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC "shade/state"

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// Continuous-rotation style control (adjust by hardware test).
#define SERVO_STOP_US 1500// 이 문구는 서보모터 움직임을 멈추기 위한 펄스 폭을 나타냅니다. 이를 기준으로 
#define SERVO_CW_US 2000// 1번 각도 조절 높일수록 시계방향으로 회전, 낮출수록 반시계방향으로 회전. 1500이 중립점 

#define SERVO_CCW_US 1000 //2번 각도조절 높일수록 시계방향으로 회전, 낮출수록 반시계방향으로 회전. 1500이 중립점
// 딱 180도를 회전시키려면 SERVO_CW_US와 SERVO_CCW_US의 차이가 1000us가 되도록 조정해야 합니다.
// 각도1 도를 올릴때마다 

#define MOVE_120_MS 420// 120도 이동에 필요한 시간입니다. 이 값은 서보모터의 속도에 따라 조정해야 할 수 있습니다.

typedef enum {
	SHADE_CLOSED = 0,
	SHADE_OPEN = 1,
} shade_state_t;

static uint g_pwm_slice = 0;
static uint16_t g_pwm_wrap = 0;
static mqtt_client_t *g_mqtt_client = NULL;
static volatile bool g_mqtt_connected = false;
static bool g_wifi_ready = false;

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
	(void)client;
	(void)arg;
	if (status == MQTT_CONNECT_ACCEPTED) {
		g_mqtt_connected = true;
		printf("[MQTT] Connected to %s:%d\n", MQTT_HOST, MQTT_BROKER_PORT);
	} else {
		g_mqtt_connected = false;
		printf("[MQTT] Connect failed. status=%d\n", status);
	}
}

static void mqtt_pub_request_cb(void *arg, err_t err) {
	(void)arg;
	if (err != ERR_OK) {
		printf("[MQTT] Publish error. err=%d\n", err);
	} else {
		printf("[MQTT] Publish success.\n");
	}
}

static bool mqtt_try_connect(void) {
	ip_addr_t broker_ip;
	struct mqtt_connect_client_info_t ci = {0};
	err_t err;

	if (!ipaddr_aton(MQTT_HOST, &broker_ip)) {
		printf("[MQTT] Invalid broker host: %s\n", MQTT_HOST);
		return false;
	}

	g_mqtt_client = mqtt_client_new();
	if (!g_mqtt_client) {
		printf("[MQTT] Client allocation failed.\n");
		return false;
	}

	ci.client_id = "pico_shade_client";
	ci.client_user = NULL;
	ci.client_pass = NULL;
	ci.keep_alive = 60;

	printf("[MQTT] Connecting to %s:%d ...\n", MQTT_HOST, MQTT_BROKER_PORT);
	err = mqtt_client_connect(g_mqtt_client, &broker_ip, MQTT_BROKER_PORT, mqtt_connection_cb, NULL, &ci);
	if (err != ERR_OK) {
		printf("[MQTT] Connect call failed. err=%d\n", err);
		return false;
	}

	for (int i = 0; i < 100; ++i) {
		if (g_mqtt_connected) {
			return true;
		}
		sleep_ms(50);
	}

	printf("[MQTT] Connect timeout.\n");
	return false;
}

static void mqtt_publish_state(const char *message) {
	if (!g_wifi_ready) {
		printf("[MQTT] Skip publish (Wi-Fi not ready): %s\n", message);
		return;
	}

	if (!g_mqtt_connected || !g_mqtt_client) {
		printf("[MQTT] Skip publish (MQTT not connected): %s\n", message);
		return;
	}

	err_t err = mqtt_publish(g_mqtt_client,
			MQTT_TOPIC,
			message,
			(u16_t)strlen(message),
			0,
			0,
			mqtt_pub_request_cb,
			NULL);
	if (err != ERR_OK) {
		printf("[MQTT] Publish call failed. err=%d\n", err);
	}
}

static void servo_write_pulse_us(uint16_t pulse_us) {
	uint32_t level = ((uint32_t)pulse_us * (g_pwm_wrap + 1u)) / SERVO_PERIOD_US;
	pwm_set_gpio_level(SERVO_PIN, (uint16_t)level);
}

static void servo_stop(void) {
	servo_write_pulse_us(SERVO_STOP_US);
}

static void servo_rotate_for_ms(uint16_t pulse_us, uint32_t duration_ms) {
	servo_write_pulse_us(pulse_us);
	sleep_ms(duration_ms);
	servo_stop();
}

static void servo_pwm_init(void) {
	gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
	g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);

	uint32_t sys_hz = clock_get_hz(clk_sys);
	// Keep 20,000 counts per 20ms period so duty mapping to microseconds is simple.
	float clk_div = (float)sys_hz / (float)(SERVO_HZ * SERVO_PERIOD_US);
	g_pwm_wrap = SERVO_PERIOD_US - 1u;

	pwm_set_clkdiv(g_pwm_slice, clk_div);
	pwm_set_wrap(g_pwm_slice, g_pwm_wrap);
	pwm_set_enabled(g_pwm_slice, true);
}

int main(void) {
	stdio_init_all();
	sleep_ms(1500);

	servo_pwm_init();
	shade_state_t shade_state = SHADE_CLOSED;

	// Stay stopped before any input.
	servo_stop();

	printf("[NET] Build-time Wi-Fi SSID configured: %s\n", strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 ? "NO" : "YES");
	printf("[NET] Password configured: %s\n", strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0 ? "NO" : "YES");

	if (cyw43_arch_init()) {
		printf("[NET] Wi-Fi init failed. MQTT disabled.\n");
	} else {
		cyw43_arch_enable_sta_mode();
		if ((strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) ||
			(strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0)) {
			printf("[NET] Wi-Fi credentials are not set. Use -DJY_WIFI_SSID and -DJY_WIFI_PASSWORD at configure time.\n");
		} else {
			printf("[NET] Connecting Wi-Fi SSID=%s ...\n", WIFI_SSID);
			int wifi_err = cyw43_arch_wifi_connect_timeout_ms(
					WIFI_SSID,
					WIFI_PASSWORD,
					CYW43_AUTH_WPA2_AES_PSK,
					30000);
			if (wifi_err) {
				printf("[NET] Wi-Fi connect failed. err=%d\n", wifi_err);
			} else {
				g_wifi_ready = true;
				printf("[NET] Wi-Fi connected.\n");
				if (!mqtt_try_connect()) {
					printf("[MQTT] Initial connection failed. Will keep running without publish until reconnected.\n");
				}
			}
		}
	}

	printf("Servo controller ready.\n");
	printf("Input '1': open shade, '2': close shade.\n");
	printf("Idle state is STOP (no rotation).\n");

	while (true) {
		int ch = getchar_timeout_us(1000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			continue;
		}

		if (ch == '1') {
			if (shade_state == SHADE_OPEN) {
				printf("Ignored: already OPEN.\n");
				continue;
			}

			printf("Opening shade...\n");
			servo_rotate_for_ms(SERVO_CW_US, MOVE_120_MS);
			shade_state = SHADE_OPEN;
			mqtt_publish_state("열었어용");
			printf("Shade OPEN.\n");
			continue;
		}

		if (ch == '2') {
			if (shade_state == SHADE_CLOSED) {
				printf("Ignored: already CLOSED.\n");
				continue;
			}

			printf("Closing shade...\n");
			servo_rotate_for_ms(SERVO_CCW_US, MOVE_120_MS);
			shade_state = SHADE_CLOSED;
			mqtt_publish_state("닫았어용");
			printf("Shade CLOSED.\n");
			continue;
		}

		printf("Invalid input '%c'. Use only '1' or '2'.\n", (char)ch);
	}
}

//색깔별 핀 연결방법

//빨간색 - 5V pico 2w에서는 vbus 와 vsys중 
//검은색 - GND
//노란색 - GPIO 16 (PWM 신호)
//이 코드는 서보모터를 제어하기 위한 코드입니다. GPIO 16 핀을 PWM 신호로 사용하여 서보모터의 각도를 제어합니다.
//사용자는 '1'을 입력하여 시계방향으로 120도까지 이동하고, '2'를 입력하여 반시계방향으로 0도로 이동할 수 있습니다. 
//각 이동은 부드럽게 이루어지며, 현재 위치에서 목표 위치까지 10ms 간격으로 각도를 조정합니다.

//각도조절은 SERVO_CW_US와 SERVO_CCW_US의 값을 조정하여 원하는 각도로 설정할 수 있습니다.

//브레드 보드를 이용해 연결하는 방법
//1. 서보모터의 빨간색 선을 브레드보드의 전원 레일(전원레일은 -+중 고르면 에 연결합니다.