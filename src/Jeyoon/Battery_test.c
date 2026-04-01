#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

/*
 * ============================================================================
 * 핀 연결 방법 (Pin Connection Guide)
 * ============================================================================
 * 
 * [서보모터 (Servo Motor)]
 * - GPIO 16 (PWM): 서모터의 신호 핀에 연결
 * - VCC (3.3V/5V): 외부 전원 또는 Pico의 VBUS (5V)에 연결
 * - GND: Pico의 GND와 연결
 * - PWM 신호: 50Hz, 1ms~2ms 펄스 폭 사용
 *   - 1000us (1ms): 반시계방향 최대
 *   - 1500us (1.5ms): 정지 (Neutral)
 *   - 2000us (2ms): 시계방향 최대
 * 
 * [워터펌프 (Water Pump)]
 * L9110S 모터 드라이버 사용:
 * - GPIO 15 (L9110S A-IA / IN1): Pico → L9110S의 IN1 핀
 * - GPIO 14 (L9110S A-IB / IN2): Pico → L9110S의 IN2 핀
 * - L9110S OA/OB: 워터펌프의 두 선에 연결
 * - L9110S VCC: 외부 전원 (+) 또는 Pico VBUS (5V)
 * - L9110S GND: 외부 전원 (-)과 Pico GND 공유 (공통 GND)
 * 
 * 펌프 동작:
 * - IN1=1, IN2=0: 정방향 (Forward)
 * - IN1=0, IN2=1: 역방향 (Backward)
 * - IN1=0, IN2=0: 정지 (Stop)
 * - IN1=1, IN2=1: 정지 (Stop)
 * 
 * 
 * 
 * ============================================================================
 */

// ============================================================================
// 서보모터 설정 (Servo Motor Configuration)
// ============================================================================
#define SERVO_PIN 16
#define SERVO_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_STOP_US 1500      // 정지 (Neutral)
#define SERVO_CW_US 2000        // 시계방향 최대 (Forward)
#define SERVO_CCW_US 1000       // 반시계방향 최대 (Backward)

// ============================================================================
// 워터펌프 설정 (Water Pump Configuration)
// ============================================================================
#define PUMP_IN1_GPIO 15        // L9110S IN1
#define PUMP_IN2_GPIO 14        // L9110S IN2
#define PUMP_FORWARD_IN1_LEVEL 1
#define PUMP_FORWARD_IN2_LEVEL 0

// ============================================================================
// 타이밍 설정 (Timing Configuration)
// ============================================================================
#define RUN_TIME_MS 10000       // 10초 가동 (Run for 10 seconds)
#define STOP_TIME_MS 5000       // 5초 정지 (Stop for 5 seconds)
#define LED_BLINK_INTERVAL_MS 500
#define WAIT_TICK_MS 50

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

// Global variables for servo PWM
static uint g_pwm_slice = 0;
static uint16_t g_pwm_wrap = 0;
static bool g_led_uses_cyw43 = false;

/**
 * 서보모터 초기화 함수
 * Initialize servo motor PWM
 */
void servo_init() {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    
    // PWM 설정: 50Hz (20ms 주기)
    pwm_set_clkdiv(g_pwm_slice, 64.0f);
    g_pwm_wrap = 20000;  // 1us 단위로 20ms
    pwm_set_wrap(g_pwm_slice, g_pwm_wrap);
    pwm_set_enabled(g_pwm_slice, true);
}

/**
 * 서보모터 제어 함수
 * Control servo motor position
 * @param pulse_us: 펄스 폭 (microseconds)
 */
void servo_set_pulse(uint16_t pulse_us) {
    uint16_t level = pulse_us;  // 1us = 1 clock
    pwm_set_gpio_level(SERVO_PIN, level);
}

/**
 * 워터펌프 초기화 함수
 * Initialize water pump GPIO pins
 */
void pump_init() {
    gpio_init(PUMP_IN1_GPIO);
    gpio_init(PUMP_IN2_GPIO);
    gpio_set_dir(PUMP_IN1_GPIO, GPIO_OUT);
    gpio_set_dir(PUMP_IN2_GPIO, GPIO_OUT);
    
    // 초기 상태: 정지
    gpio_put(PUMP_IN1_GPIO, 0);
    gpio_put(PUMP_IN2_GPIO, 0);
}

/**
 * 워터펌프 정방향 (순방향) 시작
 * Start water pump in forward direction
 */
void pump_forward() {
    gpio_put(PUMP_IN1_GPIO, PUMP_FORWARD_IN1_LEVEL);
    gpio_put(PUMP_IN2_GPIO, PUMP_FORWARD_IN2_LEVEL);
}

/**
 * 워터펌프 정지
 * Stop water pump
 */
void pump_stop() {
    gpio_put(PUMP_IN1_GPIO, 0);
    gpio_put(PUMP_IN2_GPIO, 0);
}

void board_led_write(bool on) {
#if defined(CYW43_WL_GPIO_LED_PIN)
    if (g_led_uses_cyw43) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
        return;
    }
#endif
    gpio_put(PICO_DEFAULT_LED_PIN, on ? 1 : 0);
}

/**
 * 보드 내장 LED 초기화
 * Initialize onboard LED for heartbeat blinking
 */
void board_led_init() {
#if defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init() == 0) {
        g_led_uses_cyw43 = true;
        board_led_write(false);
        return;
    }
#endif
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    board_led_write(false);
}

/**
 * 대기 중에도 LED를 주기적으로 깜빡여 전원 인가 상태를 표시
 * Blink LED while waiting so power status is always visible
 */
void wait_with_led_blink(uint32_t wait_ms, bool *led_state, uint32_t *blink_elapsed_ms) {
    uint32_t elapsed = 0;

    while (elapsed < wait_ms) {
        uint32_t step = WAIT_TICK_MS;
        if ((wait_ms - elapsed) < WAIT_TICK_MS) {
            step = wait_ms - elapsed;
        }

        sleep_ms(step);
        elapsed += step;
        *blink_elapsed_ms += step;

        if (*blink_elapsed_ms >= LED_BLINK_INTERVAL_MS) {
            *blink_elapsed_ms = 0;
            *led_state = !(*led_state);
            board_led_write(*led_state);
        }
    }
}

/**
 * 메인 함수
 * Main function: Run servo and pump in a cycle
 * - 10초: 가동 (ON)
 * - 5초: 정지 (OFF)
 * - 무한반복 (Infinite loop)
 */
int main() {
    // USB 출력 초기화
    stdio_init_all();
    sleep_ms(2000);
    
    printf("=== Battery Test: Servo + Pump Control ===\n");
    printf("Mode: 10 seconds ON, 5 seconds OFF (Infinite loop)\n\n");
    
    // 서보모터 및 워터펌프 초기화
    servo_init();
    pump_init();
    board_led_init();

    bool led_state = false;
    uint32_t blink_elapsed_ms = 0;
    
    printf("Servo and Pump initialized successfully!\n");
    printf("Starting cycle...\n\n");
    
    // 무한 반복 루프
    while (true) {
        // ========== 10초 가동 (RUN) ==========
        printf("[ON] Running for %d ms...\n", RUN_TIME_MS);
        
        // 서보모터: 정방향으로 회전
        servo_set_pulse(SERVO_CW_US);
        
        // 워터펌프: 정방향으로 작동
        pump_forward();
        
        // 10초 대기 (LED는 계속 점멸)
        wait_with_led_blink(RUN_TIME_MS, &led_state, &blink_elapsed_ms);
        
        // ========== 5초 정지 (STOP) ==========
        printf("[OFF] Stopping for %d ms...\n", STOP_TIME_MS);
        
        // 서보모터: 정지
        servo_set_pulse(SERVO_STOP_US);
        
        // 워터펌프: 정지
        pump_stop();
        
        // 5초 대기 (LED는 계속 점멸)
        wait_with_led_blink(STOP_TIME_MS, &led_state, &blink_elapsed_ms);
        
        printf("Cycle completed. Restarting...\n\n");
    }
    
    return 0;
}
