#include <stdio.h>
#include <stdint.h>

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#define SERVO_PIN 16
#define SERVO_HZ 50
#define SERVO_PERIOD_US 20000

// Positional servo pulses (tune with real hardware).
#define SERVO_HOME_US 1500
// Approximate 150-degree move from HOME (center 1500us). Tune on hardware.
#define SERVO_CCW_150_US 670

// 500us move (0 -> 90deg) takes about 1s, slower than immediate jump.
#define SERVO_MOVE_STEP_US 10
#define SERVO_MOVE_STEP_DELAY_MS 20

#define USB_WAIT_MS 8000

typedef enum {
	POS_HOME = 0,
	POS_CCW_150 = 1,
} servo_pos_t;

static uint g_pwm_slice;
static uint16_t g_pwm_wrap;
static uint16_t g_current_pulse_us = SERVO_HOME_US;

static void wait_for_usb_console(uint32_t timeout_ms) {
	absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
	while (!time_reached(deadline)) {
		if (stdio_usb_connected()) {
			return;
		}
		sleep_ms(100);
	}
}

static void servo_pwm_init(void) {
	gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
	g_pwm_slice = pwm_gpio_to_slice_num(SERVO_PIN);

	uint32_t sys_hz = clock_get_hz(clk_sys);
	float clk_div = (float)sys_hz / (float)(SERVO_HZ * SERVO_PERIOD_US);
	g_pwm_wrap = SERVO_PERIOD_US - 1u;

	pwm_set_clkdiv(g_pwm_slice, clk_div);
	pwm_set_wrap(g_pwm_slice, g_pwm_wrap);
	pwm_set_enabled(g_pwm_slice, true);
}

static void servo_write_pulse_us(uint16_t pulse_us) {
	uint32_t level = ((uint32_t)pulse_us * (g_pwm_wrap + 1u)) / SERVO_PERIOD_US;
	pwm_set_gpio_level(SERVO_PIN, (uint16_t)level);
}

static void servo_set_position_us(uint16_t pulse_us) {
	servo_write_pulse_us(pulse_us);
	g_current_pulse_us = pulse_us;
}

static void servo_move_slow_to(uint16_t target_us) {
	int32_t current = (int32_t)g_current_pulse_us;
	int32_t target = (int32_t)target_us;

	if (current == target) {
		return;
	}

	int32_t step = (target > current) ? SERVO_MOVE_STEP_US : -SERVO_MOVE_STEP_US;
	while (current != target) {
		current += step;
		if ((step > 0 && current > target) || (step < 0 && current < target)) {
			current = target;
		}

		servo_write_pulse_us((uint16_t)current);
		sleep_ms(SERVO_MOVE_STEP_DELAY_MS);
	}

	g_current_pulse_us = (uint16_t)target;
}

int main(void) {
	stdio_init_all();
	wait_for_usb_console(USB_WAIT_MS);
	sleep_ms(500);

	servo_pwm_init();
	servo_set_position_us(SERVO_HOME_US);

	servo_pos_t pos = POS_HOME;

	printf("Pico 2W 서보 시리얼 제어 준비 완료.\n");
	printf("입력 1: 반시계 방향 150도 이동\n");
	printf("입력 2: 시계 방향 150도 이동(원위치 복귀)\n");
	printf("규칙: 1 입력 후 2로 복귀하기 전까지 1은 무시됩니다.\n");
	printf("서보는 마지막 위치를 유지합니다.\n");

	while (true) {
		int ch = getchar_timeout_us(1000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
			continue;
		}

		if (ch == '1') {
			if (pos == POS_CCW_150) {
				printf("무시됨: 이미 반시계 150도 위치입니다. 먼저 2를 눌러 원위치로 복귀하세요.\n");
				continue;
			}

			printf("동작: 반시계 방향 150도 이동 중...\n");
			servo_move_slow_to(SERVO_CCW_150_US);
			pos = POS_CCW_150;
			printf("완료: 반시계 150도 위치 유지 중.\n");
			continue;
		}

		if (ch == '2') {
			if (pos == POS_HOME) {
				printf("무시됨: 이미 원위치입니다. 먼저 1을 입력하세요.\n");
				continue;
			}

			printf("동작: 시계 방향 150도 이동(원위치 복귀) 중...\n");
			servo_move_slow_to(SERVO_HOME_US);
			pos = POS_HOME;
			printf("완료: 원위치 유지 중.\n");
			continue;
		}

		printf("잘못된 입력 '%c' 입니다. 1 또는 2만 입력하세요.\n", (char)ch);
	}
}
//핀연결방법
// - 서보모터 신호선 -> GP16
// - 서보모터 전원선 -> 5V 또는 3.3V (모터 사양에 따라 다름)
// - 서보모터 GND -> Pico GND