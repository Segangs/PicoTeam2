#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

// --- 설정값 ---
#define SERVO_PIN 16
#define SERVO_HZ 50
#define SERVO_PERIOD_US 20000

#define SERVO_STOP_US 1500
#define SERVO_CW_US 2000   // 열기
#define SERVO_CCW_US 1000  // 닫기
#define MOVE_TIME_MS 420

#ifndef MQTT_SERVER
#define MQTT_SERVER "221.145.210.230"
#endif
#define MQTT_TOPIC "sensor/data"

// Wi-Fi 정보 (CMake에서 정의하지 않았을 경우 대비)
#ifndef WIFI_SSID
#define WIFI_SSID "bindsoft"
#define WIFI_PASSWORD "bindsoft24"
#endif

// --- MQTT 상태 구조체 (Example 참고) ---
typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    uint16_t last_raw_value;
} MQTT_CLIENT_DATA_T;

typedef enum {
    SHADE_CLOSED = 0,
    SHADE_OPEN = 1,
} shade_state_t;

static shade_state_t g_shade_state = SHADE_CLOSED;
static uint g_pwm_slice;
static uint16_t g_pwm_wrap;

// --- 서보모터 제어 함수 ---
static void servo_write_pulse_us(uint16_t pulse_us) {
    uint32_t level = ((uint32_t)pulse_us * (g_pwm_wrap + 1u)) / SERVO_PERIOD_US;
    pwm_set_gpio_level(SERVO_PIN, (uint16_t)level);
}

static void servo_rotate_for_ms(uint16_t pulse_us, uint32_t duration_ms) {
    servo_write_pulse_us(pulse_us);
    sleep_ms(duration_ms);
    servo_write_pulse_us(SERVO_STOP_US);
}

// --- MQTT 콜백 함수들 ---
static void pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) printf("Publish failed %d\n", err);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT Connected!\n");
        state->connect_done = true;
    } else {
        printf("MQTT Disconnected or failed: %d\n", status);
        state->connect_done = false;
    }
}

// 메시지 전송 유틸리티
static void publish_shade_status(MQTT_CLIENT_DATA_T *state, const char* msg) {
    if (state->connect_done && mqtt_client_is_connected(state->mqtt_client_inst)) {
        cyw43_arch_lwip_begin();
        mqtt_publish(state->mqtt_client_inst, MQTT_TOPIC, msg, strlen(msg), 1, 0, pub_request_cb, state);
        cyw43_arch_lwip_end();
    }
}

// DNS 결과 콜백
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        printf("DNS Found IP: %s\n", ipaddr_ntoa(ipaddr));
        
        // 클라이언트 시작
        state->mqtt_client_inst = mqtt_client_new();
        cyw43_arch_lwip_begin();
        mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, 1883, mqtt_connection_cb, state, &state->mqtt_client_info);
        cyw43_arch_lwip_end();
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // 1. ADC 및 PWM 초기화
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    g_pwm_wrap = SERVO_PERIOD_US - 1u;
    pwm_set_clkdiv(g_pwm_slice, (float)clock_get_hz(clk_sys) / (50.0f * SERVO_PERIOD_US));
    pwm_set_wrap(g_pwm_slice, g_pwm_wrap);
    pwm_set_enabled(g_pwm_slice, true);
    servo_write_pulse_us(SERVO_STOP_US);

    // 2. Wi-Fi 초기화
    if (cyw43_arch_init()) {
        printf("Wi-Fi Init Failed\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Wi-Fi Connect Failed\n");
        return -1;
    }
    printf("Wi-Fi Connected!\n");

    // 3. MQTT 상태 초기화
    static MQTT_CLIENT_DATA_T mqtt_state;
    memset(&mqtt_state, 0, sizeof(mqtt_state));
    mqtt_state.mqtt_client_info.client_id = "PicoShade01";
    mqtt_state.mqtt_client_info.keep_alive = 60;

    // 4. DNS를 통한 브로커 주소 획득 및 연결 시작
    cyw43_arch_lwip_begin();
    dns_gethostbyname(MQTT_SERVER, &mqtt_state.mqtt_server_address, dns_found, &mqtt_state);
    cyw43_arch_lwip_end();

    // 5. 메인 루프
    while (true) {
        // 필수: 백그라운드 네트워크 처리
        cyw43_arch_poll();

        uint16_t raw_value = adc_read();
        
        if (raw_value >= 3500) {
            if (g_shade_state != SHADE_CLOSED) {
                printf("조도(%u): 차양막을 닫습니다.\n", raw_value);
                servo_rotate_for_ms(SERVO_CCW_US, MOVE_TIME_MS);
                g_shade_state = SHADE_CLOSED;
                publish_shade_status(&mqtt_state, "차양막을 닫았습니다.");
            }
        } else {
            if (g_shade_state != SHADE_OPEN) {
                printf("조도(%u): 차양막을 열었습니다.\n", raw_value);
                servo_rotate_for_ms(SERVO_CW_US, MOVE_TIME_MS);
                g_shade_state = SHADE_OPEN;
                publish_shade_status(&mqtt_state, "차양막을 열었습니다.");
            }
        }

        sleep_ms(1000);
    }

    return 0;
}