// 2026.4.6. 최종 주석 추가


/*
 * ============================================================================
 * 스마트 텃밭 종합 제어 시스템 (Raspberry Pi Pico 2 W)
 * ============================================================================
 *
 * [핀 연결 가이드 - Pin Mapping]
 * 1. DHT22 (온습도 센서)
 * - VCC : 3V3
 * - GND : GND
 * - DAT : GP13
 *
 * 2. 서보 모터 (차양막 제어)
 * - VCC : 5V (VBUS)
 * - GND : GND
 * - PWM : GP16
 *
 * 3. 조도 센서 (LDR)
 * - VCC : 3V3
 * - GND : GND
 * - AO  : GP26 (ADC0)
 *
 * 4. 토양 수분 센서
 * - VCC : 3V3
 * - GND : GND
 * - AO  : GP27 (ADC1)
 *
 * 5. 수위 센서 (Water Level)
 * - VCC : 3V3
 * - GND : GND
 * - AO  : GP28 (ADC2)
 *
 * 6. 워터펌프 & L9110S 모터 드라이버
 * - 외부 전원(+) : L9110S VCC (18650 배터리 또는 별도 5V 어댑터 권장)
 * - 외부 전원(-) : L9110S GND (반드시 Pico GND와 공통 접지할 것)
 * - IN1 : GP14
 * - IN2 : GP15
 * - OA/OB : 워터 펌프 연결
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// lwIP MQTT 헤더
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"

// --- MQTT 브로커 설정 ---
#define MQTT_BROKER_IP "221.145.210.230"  // MQTT 브로커(서버)의 IP 주소
#define MQTT_PORT 1883
#define MQTT_TOPIC_PUB "sensor/data"
#define MQTT_TOPIC_SUB "sensor/control"// 피코가 명령을 받는 토픽

// --- 핀 설정 ---
#define DHT_PIN 13
#define SERVO_PIN 16
#define LDR_ADC_GPIO 26
#define LDR_ADC_INPUT 0
#define SOIL_ADC_GPIO 27
#define SOIL_ADC_INPUT 1
#define WATER_SENSOR_ADC_GPIO 28
#define WATER_SENSOR_ADC_INPUT 2
#define L9110S_IN1_GPIO 14
#define L9110S_IN2_GPIO 15

// --- 센서 및 구동기 설정값 ---
#define MAX_TIMESTEPS 85
#define SERVO_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 2000
#define SERVO_MAX_US 1400 //875
#define SOIL_ADC_MAX 4095
#define SOIL_DRY_THRESHOLD_VALUE 1100
#define DRY_SENSITIVITY_DIVISOR 2
#define WATER_DOSE_ML 100
#define PUMP_FLOW_RATE_ML_PER_MIN 600
#define SENSOR_RAW_DRY 0
#define SENSOR_RAW_WET 3200
#define ADC_AVG_SAMPLES 16
#define CHECK_INTERVAL_MS 60000

// --- 전역 변수 (상태 저장용) ---
typedef enum { SHADE_IDLE = 0, SHADE_MOVED = 1 } shade_state_t;
typedef struct { float humidity; float temperature; bool valid; } dht_result;

static shade_state_t g_shade_state = SHADE_IDLE;
static uint g_pwm_slice, g_pwm_wrap;
static dht_result g_current_dht = {0.0f, 0.0f, false};


static uint16_t g_current_servo_us = 2000; //  시작 위치를 2000으로 변경

// 종합 알림을 위해 각 센서의 최근 값을 기억하는 변수들
static int32_t g_current_water_percent = 0;
static uint16_t g_current_light_raw = 0;
static uint16_t g_current_soil_raw = 0;
static uint16_t g_dry_threshold_value = 1u;

// 시간 관리 타이머 변수들
static uint64_t next_dht_publish_ms = 0;
static uint64_t next_light_check_ms = 0;
static uint64_t next_soil_check_ms = 0;
static uint64_t next_waterlevel_check_ms = 0;
static uint64_t next_network_check_ms = 0;

// 네트워크 및 제어 관리 변수
static mqtt_client_t *mqtt_client = NULL;
static bool mqtt_connecting = false; 

// 웹 원격 제어 수신용 깃발
static bool g_cmd_pump_on = false;
static bool g_cmd_shade_close = false;
static bool g_cmd_shade_open = false;

/**
 * 시스템 부팅 이후 경과된 시간을 밀리초(ms) 단위로 반환합니다.
 */
static uint64_t now_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

// ============================================================================
// MQTT 통신 콜백 함수들
// ============================================================================

/**
 * MQTT 메시지 발행(Publish) 요청이 완료되었을 때 호출되는 콜백 함수입니다.
 */
static void mqtt_pub_request_cb(void *arg, err_t result) {
    if (result != ERR_OK) {
        printf("MQTT Publish failed: %d\n", result);
    }
}

/**
 * MQTT 토픽 구독(Subscribe) 요청 결과가 돌아왔을 때 호출되는 콜백 함수입니다.
 */
static void mqtt_sub_request_cb(void *arg, err_t result) {
    // 필요 시 처리
}

/**
 * 구독 중인 토픽으로부터 실제 데이터(Payload)가 도착했을 때 실행되는 함수입니다.
 * 수신된 명령(pump_on, shade_close 등)에 따라 전역 제어 플래그를 설정합니다.
 */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char payload[64];
    uint16_t copy_len = (len < sizeof(payload) - 1) ? len : sizeof(payload) - 1;
    memcpy(payload, data, copy_len);
    payload[copy_len] = '\0';
    
    printf("[MQTT] 원격 명령 수신: %s\n", payload);
    
    if (strcmp(payload, "pump_on") == 0) g_cmd_pump_on = true;
    else if (strcmp(payload, "shade_close") == 0) g_cmd_shade_close = true;
    else if (strcmp(payload, "shade_open") == 0) g_cmd_shade_open = true;
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    // 새로운 메시지 수신이 시작될 때의 처리 (현재는 비워둠)
}

/**
 * MQTT 브로커와의 연결 상태가 변경될 때(성공/실패 등) 호출되는 콜백 함수입니다.
 * 연결 성공 시 제어 토픽(MQTT_TOPIC_SUB)을 구독합니다.
 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    mqtt_connecting = false;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("[MQTT] 브로커 연결 성공!\n");
        // 연결 성공 시 원격 제어 명령 토픽을 구독합니다.
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);
        mqtt_subscribe(client, MQTT_TOPIC_SUB, 0, mqtt_sub_request_cb, NULL);
    } else {
        printf("[MQTT] 연결 실패 (상태 코드: %d)\n", status);
    }
}

/**
 * 설정된 브로커 IP와 포트를 사용하여 MQTT 서버에 접속을 시도합니다.
 * 비동기 방식으로 동작하며 결과는 mqtt_connection_cb를 통해 전달됩니다.
 */
void connect_to_mqtt_broker() {
    // 연결 시도 중이거나 클라이언트 객체가 없으면 무시 (메모리 꼬임 방지)
    if (mqtt_connecting || mqtt_client == NULL) return;

    ip_addr_t broker_ip;
    ipaddr_aton(MQTT_BROKER_IP, &broker_ip);

    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = "pico2w_smartfarm";
    ci.keep_alive = 60; 

    mqtt_connecting = true; 
    err_t err = mqtt_client_connect(mqtt_client, &broker_ip, MQTT_PORT, mqtt_connection_cb, NULL, &ci);
    if (err != ERR_OK) {
        printf("[MQTT] 연결 요청 에러: %d\n", err);
        mqtt_connecting = false; 
    }
}

/**
 * 현재 센서 데이터(온도, 습도, 조도, 토양수분, 수위)와 상태 정보를 JSON 형식으로 구성하여 
 * 지정된 MQTT 토픽으로 발행합니다.
 */
void publish_mqtt_json(const char* event_type) {
    char json_payload[256];
    
    snprintf(json_payload, sizeof(json_payload),
             "{\"event\": \"%s\", \"temp\": %.1f, \"humi\": %.1f, \"light\": %d, \"shade\": %d, \"water\": %ld, \"soil\": %d}",
             event_type, 
             g_current_dht.valid ? g_current_dht.temperature : 0.0,
             g_current_dht.valid ? g_current_dht.humidity : 0.0,
             g_current_light_raw,
             g_shade_state,
             g_current_water_percent,
             g_current_soil_raw);
             
    printf("\n[MQTT 전송]\n%s\n", json_payload);

    if (mqtt_client != NULL && mqtt_client_is_connected(mqtt_client)) {
        err_t err = mqtt_publish(mqtt_client, MQTT_TOPIC_PUB, json_payload, strlen(json_payload), 0, 0, mqtt_pub_request_cb, NULL);
        if (err != ERR_OK) {
            printf("발행 요청 에러: %d\n", err);
        }
    }
}

// ============================================================================
// 하드웨어 제어 함수
// ============================================================================

/**
 * 특정 ADC 채널로부터 지정된 횟수만큼 샘플링하여 평균값을 읽어옵니다. (노이즈 제거용)
 */
static uint16_t read_adc_average(uint channel) {
    adc_select_input(channel);
    uint32_t sum = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; ++i) { sum += adc_read(); sleep_ms(2); }
    return (uint16_t)(sum / ADC_AVG_SAMPLES);
}

/**
 * DHT22 센서와의 통신을 통해 온도와 습도를 읽어옵니다.
 * 정밀한 타이밍 제어를 위해 비트 뱅잉(Bit-banging) 방식을 사용합니다.
 */
bool read_from_dht(dht_result *result) {
    int data[5] = {0}; uint last_state = 1; uint j = 0;
    gpio_set_dir(DHT_PIN, GPIO_OUT); gpio_put(DHT_PIN, 0); sleep_ms(20);
    gpio_put(DHT_PIN, 1); sleep_us(40); gpio_set_dir(DHT_PIN, GPIO_IN);

    for (uint i = 0; i < MAX_TIMESTEPS; i++) {
        uint count = 0;
        while (gpio_get(DHT_PIN) == last_state) { count++; sleep_us(1); if (count == 255) break; }
        last_state = gpio_get(DHT_PIN); if (count == 255) break;
        if ((i >= 4) && (i % 2 == 0)) { data[j / 8] <<= 1; if (count > 40) data[j / 8] |= 1; j++; }
    }
    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
        result->humidity = (float)((data[0] << 8) + data[1]) / 10;
        result->temperature = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
        if (data[2] & 0x80) result->temperature *= -1;
        result->valid = true; return true;
    }
    result->valid = false; return false;
}

/**
 * 서보 모터의 펄스 폭(Microseconds)을 설정하여 모터의 각도를 제어합니다.
 */
static void servo_write_pulse_us(uint16_t pulse_us) {
    uint32_t level = ((uint32_t)pulse_us * (g_pwm_wrap + 1u)) / SERVO_PERIOD_US;
    pwm_set_gpio_level(SERVO_PIN, (uint16_t)level);
}

/**
 * L9110S 모터 드라이버의 입력 핀 2개를 제어하여 펌프의 작동 여부를 결정합니다.
 */
static void pump_drive_levels(bool in1, bool in2) {
    gpio_put(L9110S_IN1_GPIO, in1); gpio_put(L9110S_IN2_GPIO, in2);
}

/**
 * 입력받은 용량(ml)에 맞춰 펌프를 특정 시간 동안 작동시킵니다.
 */
static void water_once_ml(uint32_t ml) {
    const uint64_t numerator = (uint64_t)ml * 60000u;
    uint32_t ms = (uint32_t)(numerator / PUMP_FLOW_RATE_ML_PER_MIN);
    if ((numerator % PUMP_FLOW_RATE_ML_PER_MIN) != 0u) { ms += 1u; }
    if (ms == 0u) ms = 1u;
    
    pump_drive_levels(true, false); sleep_ms(ms); pump_drive_levels(false, false);
}

// ============================================================================
// MAIN
// ============================================================================

/**
 * 시스템의 메인 엔트리 포인트입니다. 하드웨어를 초기화하고 무한 루프 내에서 센서 측정 및 제어를 수행합니다.
 */
int main() {
    stdio_init_all();
    sleep_ms(3000); 

    printf("AIoT 스마트 텃밭 시스템 부팅 중...\n");

    // 1. Wi-Fi 칩셋(CYW43) 초기화
    if (cyw43_arch_init()) {
        printf("[Error] Wi-Fi 칩셋 초기화 실패\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    // 부팅 시 MQTT 클라이언트 단 1회 할당 (메모리 꼬임 방지)
    mqtt_client = mqtt_client_new();
    if (mqtt_client == NULL) {
        printf("[Error] 초기 MQTT 클라이언트 할당 실패!\n");
    }

    // 2. 하드웨어 초기화
    adc_init(); adc_gpio_init(LDR_ADC_GPIO); adc_gpio_init(SOIL_ADC_GPIO); adc_gpio_init(WATER_SENSOR_ADC_GPIO);
    gpio_init(DHT_PIN);
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    g_pwm_wrap = SERVO_PERIOD_US - 1u;
    pwm_set_clkdiv(g_pwm_slice, (float)clock_get_hz(clk_sys) / (SERVO_HZ * (g_pwm_wrap + 1)));
    pwm_set_wrap(g_pwm_slice, g_pwm_wrap); pwm_set_enabled(g_pwm_slice, true);
    servo_write_pulse_us(SERVO_MIN_US);
    
    gpio_init(L9110S_IN1_GPIO); gpio_set_dir(L9110S_IN1_GPIO, GPIO_OUT);
    gpio_init(L9110S_IN2_GPIO); gpio_set_dir(L9110S_IN2_GPIO, GPIO_OUT);
    pump_drive_levels(false, false);

    g_dry_threshold_value = SOIL_DRY_THRESHOLD_VALUE / DRY_SENSITIVITY_DIVISOR;
    if (g_dry_threshold_value == 0u) g_dry_threshold_value = 1u;

    uint64_t init_time = now_ms();
    next_dht_publish_ms = init_time + 10000; 
    next_light_check_ms = init_time;
    next_soil_check_ms = init_time;
    next_waterlevel_check_ms = init_time;
    next_network_check_ms = init_time;


    // main 함수 내부 하드웨어 초기화 구간
    pwm_set_wrap(g_pwm_slice, g_pwm_wrap); 
    pwm_set_enabled(g_pwm_slice, true);
    
    // 부팅 시 초기 위치를 2000us(열림)로 설정
    servo_write_pulse_us(SERVO_MIN_US);

    printf("통합 제어 루프 진입\n");

    while (true) {
        uint64_t current_time = now_ms();

        // ---------------------------------------------------------
        // [0] 웹 원격 명령 실행 처리 (가장 우선순위가 높음)
        // ---------------------------------------------------------
        if (g_cmd_pump_on) {
            printf("[Remote] 원격 명령 실행: 펌프 가동\n");
            water_once_ml(WATER_DOSE_ML);
            publish_mqtt_json("pump_on");
            g_cmd_pump_on = false;
        }
        if (g_cmd_shade_close) {
            printf("[Remote] 원격 명령 실행: 차양막 닫기\n");
            servo_write_pulse_us(SERVO_MAX_US); 
            g_shade_state = SHADE_MOVED;
            publish_mqtt_json("shade_close");
            g_cmd_shade_close = false;
        }
        if (g_cmd_shade_open) {
            printf("[Remote] 원격 명령 실행: 차양막 열기\n");
            servo_write_pulse_us(SERVO_MIN_US); 
            g_shade_state = SHADE_IDLE;
            publish_mqtt_json("shade_open");
            g_cmd_shade_open = false;
        }

        // ---------------------------------------------------------
        // [1] 네트워크 모니터링 및 자동 재연결 (10초 주기)
        // ---------------------------------------------------------
        if (current_time >= next_network_check_ms) {
            int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            
            if (link_status <= 0) {
                printf("[WiFi] Wi-Fi 연결 끊김. 백그라운드 재연결 시도 중...\n");
                mqtt_connecting = false; 
                
                if (mqtt_client != NULL && mqtt_client_is_connected(mqtt_client)) {
                    mqtt_disconnect(mqtt_client); // 이전 연결 흔적 삭제
                }
                cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
            } 
            else if (link_status == CYW43_LINK_UP) {
                if (mqtt_client != NULL && !mqtt_client_is_connected(mqtt_client)) {
                    connect_to_mqtt_broker(); 
                }
            } 
            else {
                printf("[WiFi] Wi-Fi 연결 진행 중... (상태 코드: %d)\n", link_status);
                mqtt_connecting = false;
            }
            next_network_check_ms = current_time + 10000;
        }

        // ---------------------------------------------------------
        // [2] 종합 정기 알림 (1분 주기)
        // ---------------------------------------------------------
        read_from_dht(&g_current_dht); 
        if (current_time >= next_dht_publish_ms) {
            publish_mqtt_json("regular");
            next_dht_publish_ms = current_time + CHECK_INTERVAL_MS;
        }

        // ---------------------------------------------------------
        // [3] 수위 센서 모니터링 (1분 주기)
        // ---------------------------------------------------------
        if (current_time >= next_waterlevel_check_ms) {
            uint16_t water_raw = read_adc_average(WATER_SENSOR_ADC_INPUT);
            int32_t level_permille = 0;
            if (SENSOR_RAW_WET > SENSOR_RAW_DRY) {
                level_permille = ((int32_t)water_raw - SENSOR_RAW_DRY) * 1000 / (SENSOR_RAW_WET - SENSOR_RAW_DRY);
                if(level_permille < 0) level_permille = 0;
                if(level_permille > 1000) level_permille = 1000;
            }
            int32_t level_percent = level_permille / 10;
            g_current_water_percent = level_percent; 
            
            if (level_percent <= 50) {
                publish_mqtt_json("water_low"); 
            }
            next_waterlevel_check_ms = current_time + CHECK_INTERVAL_MS;
        }

        // // ---------------------------------------------------------
        // // [4] 조도 센서 모니터링 (1분 주기)
        // // ---------------------------------------------------------
        // if (current_time >= next_light_check_ms) {
        //     uint16_t light_raw = read_adc_average(LDR_ADC_INPUT);
        //     g_current_light_raw = light_raw; 
            
        //     if (light_raw >= 3850) {
        //         if (g_shade_state != SHADE_MOVED) {
        //             servo_write_pulse_us(SERVO_MAX_US); 
        //             g_shade_state = SHADE_MOVED;
        //             publish_mqtt_json("shade_close");
        //         }
        //     } else {
        //         if (g_shade_state != SHADE_IDLE) {
        //             servo_write_pulse_us(SERVO_MIN_US); 
        //             g_shade_state = SHADE_IDLE;
        //             publish_mqtt_json("shade_open");
        //         }
        //     }
        //     next_light_check_ms = current_time + CHECK_INTERVAL_MS;
        // }
        // ---------------------------------------------------------
        // [4] 조도 센서 모니터링 (1분 주기)
        // ---------------------------------------------------------
        if (current_time >= next_light_check_ms) {
            uint16_t light_raw = read_adc_average(LDR_ADC_INPUT);
            g_current_light_raw = light_raw; 
            
            // 1. 소프트웨어 민감도 증폭 (3500~4095 범위를 0~100%로 변환)
            int32_t light_percent = 0;
            if (light_raw > 4095) light_percent = 100;
            else if (light_raw < 3500) light_percent = 0;
            else {
                light_percent = ((light_raw - 3500) * 100) / (4095 - 3500);
            }

         
            // 2. 증폭된 퍼센트를 기준으로 제어 (예: 광량이 75% 이상일 때 작동)
            if (light_percent >= 75) {
                if (g_shade_state != SHADE_MOVED) {
                    servo_write_pulse_us(SERVO_MAX_US); 
                    g_shade_state = SHADE_MOVED;
                    publish_mqtt_json("shade_close");
                }
            } else {
                if (g_shade_state != SHADE_IDLE) {
                    servo_write_pulse_us(SERVO_MIN_US); 
                    g_shade_state = SHADE_IDLE;
                    publish_mqtt_json("shade_open");
                }
            }
            next_light_check_ms = current_time + CHECK_INTERVAL_MS;
        }

        // ---------------------------------------------------------
        // [5] 토양 수분 모니터링 (1분 주기)
        // ---------------------------------------------------------
        if (current_time >= next_soil_check_ms) {
            uint16_t soil_raw = read_adc_average(SOIL_ADC_INPUT);
            g_current_soil_raw = soil_raw; 
            
            uint16_t soil_value = SOIL_ADC_MAX - soil_raw;
            if (soil_value <= g_dry_threshold_value) {
                water_once_ml(WATER_DOSE_ML);
                publish_mqtt_json("pump_on");
                // 물을 준 후 1분 뒤 재검사
                current_time = now_ms(); 
                next_soil_check_ms = current_time + CHECK_INTERVAL_MS;
            } else {
                next_soil_check_ms = current_time + CHECK_INTERVAL_MS;
            }
        }

        sleep_ms(100); 
    }
    return 0;
}