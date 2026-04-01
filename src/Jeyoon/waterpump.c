#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

// L9110S (motor channel A) wiring guide:
// - Pico GP15 -> L9110S A-IA (IN1)
// - Pico GP14 -> L9110S A-IB (IN2)
// - Pump wires -> L9110S OA/OB
// - L9110S VCC -> pump external power (+), GND -> external power (-)
// - Pico GND and external power GND must be common
#define L9110S_IN1_GPIO 15
#define L9110S_IN2_GPIO 14

// If pump direction is reversed, swap these two values.
#define PUMP_FORWARD_IN1_LEVEL 1
#define PUMP_FORWARD_IN2_LEVEL 0

// Calibrate this value by measurement.
// Example: if 100 ml comes out in 10 seconds, rate = 600 ml/min.
#define PUMP_FLOW_RATE_ML_PER_MIN 600

#define MAX_CMD_LEN 31
#define MAX_DOSE_ML 2000
#define AUTO_SUBMIT_IDLE_MS 800
#define RUN_STATUS_INTERVAL_MS 3000
#define IDLE_STATUS_INTERVAL_MS 10000
#define USB_LINK_CHECK_INTERVAL_MS 500
#define SERIAL_WAIT_MS 3000

typedef enum {
	PUMP_MODE_OFF = 0,
	PUMP_MODE_CONTINUOUS,
	PUMP_MODE_TIMED,
} pump_mode_t;

static pump_mode_t g_pump_mode = PUMP_MODE_OFF;
static uint64_t g_stop_at_ms = 0;
static uint64_t g_next_status_log_ms = 0;
static uint64_t g_next_idle_log_ms = 0;
static uint64_t g_next_usb_link_check_ms = 0;
static bool g_usb_prev_connected = false;

static uint64_t now_ms(void) {
	return to_ms_since_boot(get_absolute_time());
}

static bool usb_serial_connected(void) {
	return stdio_usb_connected();
}

static void log_printf(const char *fmt, ...) {
	if (!usb_serial_connected()) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

static void wait_for_serial_or_timeout(uint32_t timeout_ms) {
	uint32_t elapsed_ms = 0;
	while (!usb_serial_connected() && elapsed_ms < timeout_ms) {
		sleep_ms(10);
		elapsed_ms += 10;
	}
}

static const char *pump_mode_to_str(pump_mode_t mode) {
	switch (mode) {
	case PUMP_MODE_OFF:
		return "OFF";
	case PUMP_MODE_CONTINUOUS:
		return "CONTINUOUS";
	case PUMP_MODE_TIMED:
		return "TIMED";
	default:
		return "UNKNOWN";
	}
}


static void pump_drive_levels(bool in1, bool in2) {
	gpio_put(L9110S_IN1_GPIO, in1);
	gpio_put(L9110S_IN2_GPIO, in2);
}

static void pump_set(bool on) {
	if (on) {
		pump_drive_levels(PUMP_FORWARD_IN1_LEVEL != 0, PUMP_FORWARD_IN2_LEVEL != 0);
	} else {
		// Coast/stop
		pump_drive_levels(false, false);
	}
}

static bool pump_is_running(void) {
	return g_pump_mode != PUMP_MODE_OFF;
}

static void report_status(const char *tag) {
	const uint64_t now = now_ms();
	uint64_t remain_ms = 0;

	if (g_pump_mode == PUMP_MODE_TIMED && g_stop_at_ms > now) {
		remain_ms = g_stop_at_ms - now;
	}

	log_printf("[STATUS] %s | mode=%s | running=%s | remain_ms=%lu | IN1=%d IN2=%d\n",
		   tag,
		   pump_mode_to_str(g_pump_mode),
		   pump_is_running() ? "YES" : "NO",
		   (unsigned long)remain_ms,
		   gpio_get(L9110S_IN1_GPIO),
		   gpio_get(L9110S_IN2_GPIO));
}

static void pump_off(const char *reason) {
	pump_set(false);
	g_pump_mode = PUMP_MODE_OFF;
	g_stop_at_ms = 0;
	g_next_status_log_ms = 0;
	if (reason) {
		log_printf("[PUMP] OFF (%s)\n", reason);
	} else {
		log_printf("[PUMP] OFF\n");
	}
	report_status("state-changed");
}

static uint32_t ml_to_duration_ms(uint32_t ml) {
	const uint64_t numerator = (uint64_t)ml * 60000u;
	uint32_t ms = (uint32_t)(numerator / PUMP_FLOW_RATE_ML_PER_MIN);
	if ((numerator % PUMP_FLOW_RATE_ML_PER_MIN) != 0u) {
		ms += 1u;
	}
	if (ms == 0u) {
		ms = 1u;
	}
	return ms;
}

static void start_continuous(void) {
	pump_set(true);
	g_pump_mode = PUMP_MODE_CONTINUOUS;
	g_stop_at_ms = 0;
	g_next_status_log_ms = now_ms() + RUN_STATUS_INTERVAL_MS;
	log_printf("[PUMP] ON (continuous mode)\n");
	report_status("state-changed");
}

static void start_timed_dose(uint32_t ml) {
	const uint32_t duration_ms = ml_to_duration_ms(ml);

	pump_set(true);
	g_pump_mode = PUMP_MODE_TIMED;
	g_stop_at_ms = now_ms() + duration_ms;
	g_next_status_log_ms = now_ms() + RUN_STATUS_INTERVAL_MS;

	log_printf("[PUMP] Dose start: %lu ml, run %lu ms\n",
		   (unsigned long)ml,
		   (unsigned long)duration_ms);
	report_status("state-changed");
}

static bool parse_positive_ml(const char *cmd, uint32_t *out_ml) {
	char *end_ptr = NULL;
	errno = 0;
	long value = strtol(cmd, &end_ptr, 10);

	if (cmd[0] == '\0' || end_ptr == cmd || *end_ptr != '\0' || errno != 0) {
		return false;
	}
	if (value <= 0 || value > MAX_DOSE_ML) {
		return false;
	}

	*out_ml = (uint32_t)value;
	return true;
}

static void handle_command(const char *cmd) {
	if (strcmp(cmd, "a") == 0 || strcmp(cmd, "A") == 0) {
		start_continuous();
		return;
	}

	if (strcmp(cmd, "b") == 0 || strcmp(cmd, "B") == 0) {
		pump_off("manual stop");
		return;
	}

	if (strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0) {
		report_status("manual-query");
		return;
	}

	uint32_t ml = 0;
	if (parse_positive_ml(cmd, &ml)) {
		start_timed_dose(ml);
		return;
	}

	log_printf("[CMD] Invalid input: '%s'\n", cmd);
	log_printf("[CMD] Use number(1~%d), 'a' (continuous ON), 'b' (OFF), 's' (status).\n", MAX_DOSE_ML);
}

int main(void) {
	stdio_init_all();
	setvbuf(stdout, NULL, _IONBF, 0);
	wait_for_serial_or_timeout(SERIAL_WAIT_MS);
	sleep_ms(100);

	gpio_init(L9110S_IN1_GPIO);
	gpio_set_dir(L9110S_IN1_GPIO, GPIO_OUT);
	gpio_init(L9110S_IN2_GPIO);
	gpio_set_dir(L9110S_IN2_GPIO, GPIO_OUT);
	g_usb_prev_connected = usb_serial_connected();
	g_next_usb_link_check_ms = now_ms() + USB_LINK_CHECK_INTERVAL_MS;
	pump_off("boot");

	log_printf("Water pump controller ready.\n");
	log_printf("L9110S pins: IN1=GP%d, IN2=GP%d, flow=%d ml/min\n",
		   L9110S_IN1_GPIO,
		   L9110S_IN2_GPIO,
		   PUMP_FLOW_RATE_ML_PER_MIN);
	log_printf("Commands: number=ml dose (ex: 10), a=continuous ON, b=OFF, s=status\n");
	log_printf("Tip: a/b/s are executed immediately. Number command works with Enter or auto-submit after %.1fs.\n",
		   AUTO_SUBMIT_IDLE_MS / 1000.0f);
	g_next_idle_log_ms = now_ms() + IDLE_STATUS_INTERVAL_MS;

	char cmd_buf[MAX_CMD_LEN + 1];
	uint32_t cmd_len = 0;
	uint64_t last_input_ms = 0;

	while (true) {
		const uint64_t now = now_ms();

		if (now >= g_next_usb_link_check_ms) {
			const bool usb_now_connected = usb_serial_connected();
			if (usb_now_connected && !g_usb_prev_connected) {
				log_printf("[USB] Serial monitor reconnected.\n");
				report_status("usb-reconnect");
			}
			g_usb_prev_connected = usb_now_connected;
			g_next_usb_link_check_ms = now + USB_LINK_CHECK_INTERVAL_MS;
		}

		if (g_pump_mode == PUMP_MODE_TIMED && now >= g_stop_at_ms) {
			pump_off("timed dose complete");
		}

		if (pump_is_running() && g_next_status_log_ms > 0 && now >= g_next_status_log_ms) {
			report_status("periodic");
			g_next_status_log_ms = now + RUN_STATUS_INTERVAL_MS;
		}

		if (!pump_is_running() && now >= g_next_idle_log_ms) {
			report_status("idle-heartbeat");
			g_next_idle_log_ms = now + IDLE_STATUS_INTERVAL_MS;
		}

		const int ch = getchar_timeout_us(1000);
		if (ch == PICO_ERROR_TIMEOUT) {
			if (cmd_len > 0 && (now - last_input_ms) >= AUTO_SUBMIT_IDLE_MS) {
				cmd_buf[cmd_len] = '\0';
				log_printf("[CMD] Auto-submit: %s\n", cmd_buf);
				handle_command(cmd_buf);
				cmd_len = 0;
			}
			sleep_ms(1);
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			if (cmd_len > 0) {
				cmd_buf[cmd_len] = '\0';
				log_printf("[CMD] Submit: %s\n", cmd_buf);
				handle_command(cmd_buf);
				cmd_len = 0;
			}
			continue;
		}

		if (ch == 0x08 || ch == 0x7F) {
			if (cmd_len > 0) {
				cmd_len--;
			}
			continue;
		}

		if (isprint(ch) && cmd_len < MAX_CMD_LEN) {
			if (cmd_len == 0 && (ch == 'a' || ch == 'A' || ch == 'b' || ch == 'B' || ch == 's' || ch == 'S')) {
				char one_cmd[2] = {(char)ch, '\0'};
				log_printf("[CMD] Instant: %s\n", one_cmd);
				handle_command(one_cmd);
				continue;
			}

			cmd_buf[cmd_len++] = (char)ch;
			last_input_ms = now;
		}
	}
}

// 이코드는 물펌프를 제어하는 프로그램입니다. 
// 사용자는 명령어를 통해 펌프를 켜거나 끌 수 있으며, 특정 양의 물을 펌핑하도록 설정할 수도 있습니다.
// 주요 기능:
// 1. 펌프 제어: 'a' 명령어로 연속적으로 펌프를 켜고, 'b' 명령어로 펌프를 끌 수 있습니다.
// 2. 타이머 기능: 사용자가 ml 단위로 양을 입력하면, 해당 양에 맞는 시간 동안 펌프가 작동합니다. 
// 3. 상태 보고: 's' 명령어로 현재 펌프의 상태를 확인할 수 있습니다. 또한, 펌프가 작동 중일 때 주기적으로 상태를 로그로 출력합니다.

//핀 연결 방법
// L9110S VCC → 외부 전원 + (예: 5V)
// L9110S GND → 외부 전원 GND
// Pico GND → L9110S GND (반드시 공통)
// Pico GP15 → L9110S a-1a (IN1)
// Pico GP14 → L9110S a-1b (IN2)
// 펌프 빨강 → L9110S A_OUT+ (출력 한 쪽)
// 펌프 검정 → L9110S A_OUT- (출력 다른 쪽)