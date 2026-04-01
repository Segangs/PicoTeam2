# include <stdio.h>
# include "pico/stdlib.h"
# include "hardware/pwm.h"
# include "hardware/adc.h"
# include "hardware/clocks.h"

// --- 설정값 ---
# define SERVO_PIN 16
# define SERVO_HZ 50
# define SERVO_PERIOD_US 20000

// 일반 서보 기준 (모터 사양에 따라 미세 조정 필요)
# define SERVO_MIN_US 500    // 0도 (왼쪽 끝)
# define SERVO_MID_US 1500   // 90도 (중앙/오른쪽 고정 위치)
# define SERVO_MAX_US 2000   // 180도 (오른쪽 끝)

typedef enum {
    SHADE_IDLE = 0,    // 초기 상태
    SHADE_MOVED = 1,   // 이동 완료 상태
} shade_state_t;

static shade_state_t g_shade_state = SHADE_IDLE;
static uint g_pwm_slice;
static uint16_t g_pwm_wrap;

// --- 서보모터 각도 제어 함수 ---
static void servo_write_pulse_us(uint16_t pulse_us) {
    uint32_t level = ((uint32_t)pulse_us * (g_pwm_wrap + 1u)) / SERVO_PERIOD_US;
    pwm_set_gpio_level(SERVO_PIN, (uint16_t)level);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    printf("시스템 시작: 조도 감지 후 90도 고정 모드\n");

    // 1. ADC(조도 센서) 초기화
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    // 2. PWM(서보 모터) 초기화
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    g_pwm_wrap = SERVO_PERIOD_US - 1u;

    float clk_div = (float)clock_get_hz(clk_sys) / (SERVO_HZ * (g_pwm_wrap + 1));
    pwm_set_clkdiv(g_pwm_slice, clk_div);
    pwm_set_wrap(g_pwm_slice, g_pwm_wrap);
    pwm_set_enabled(g_pwm_slice, true);

    // 초기 상태: 0도 방향(또는 대기 위치)으로 시작
    servo_write_pulse_us(SERVO_MIN_US);

    while (true) {
        uint16_t raw_value = adc_read();
        printf("현재 조도 수치: %u\n", raw_value);

        // 3. 조도 값에 따라 '한 번만' 오른쪽(90도)으로 이동
        if (raw_value >= 3500) {
            if (g_shade_state != SHADE_MOVED) {
                printf("상태 변경: 밝음 -> 오른쪽(90도)으로 이동 후 고정합니다.\n");

                // 90도(중앙/오른쪽) 위치로 이동
                servo_write_pulse_us(SERVO_MAX_US);

                // 이동 후 상태를 변경하여 다시 움직이지 않도록 함
                g_shade_state = SHADE_MOVED;
            }
        }
        // 조도가 낮아졌을 때 다시 원래대로 돌아오게 하고 싶다면 아래 주석을 해제하세요.
        else {
                printf("상태 변경: 어두움 -> 원래 위치(0도)로 복귀.\n");
                servo_write_pulse_us(SERVO_MID_US);
                g_shade_state = SHADE_IDLE;
            }

        sleep_ms(500);
    }

    return 0;
}