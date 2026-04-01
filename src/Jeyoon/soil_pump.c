# include <stdbool.h>
# include <stdint.h>
# include <stdio.h>

# include "hardware/adc.h"
# include "pico/stdio_usb.h"
# include "pico/stdlib.h"

// Soil sensor (analog)
# define SOIL_ADC_GPIO 26
# define SOIL_ADC_INPUT 0
# define SOIL_ADC_MAX 4095

// 변환값 기준: 마를수록 낮고, 습할수록 높게 보이도록 반전 처리
// soil_value = SOIL_ADC_MAX - raw
# define SOIL_DRY_THRESHOLD_VALUE 1100
// 민감도 완화 계수: 2이면 약 2배 덜 민감(더 건조할 때만 급수)
# define DRY_SENSITIVITY_DIVISOR 2

// Pump driver (L9110S channel A)
# define L9110S_IN1_GPIO 15
# define L9110S_IN2_GPIO 14
# define PUMP_FORWARD_IN1_LEVEL 1
# define PUMP_FORWARD_IN2_LEVEL 0

// Watering policy
# define WATER_DOSE_ML 100
# define REPEAT_INTERVAL_MS 10000

// Pump flow calibration: ml/min
// 예) 600이면 100ml에 약 10초 동작
# define PUMP_FLOW_RATE_ML_PER_MIN 600

# define SERIAL_WAIT_MS 3000
# define SENSOR_LOG_INTERVAL_MS 1000
# define DRY_WAIT_LOG_INTERVAL_MS 1000

static uint64_t now_ms(void) {
	return to_ms_since_boot(get_absolute_time());
}

static bool usb_serial_connected(void) {
	return stdio_usb_connected();
}

static void wait_for_serial_or_timeout(uint32_t timeout_ms) {
	uint32_t elapsed_ms = 0;
	while (!usb_serial_connected() && elapsed_ms < timeout_ms) {
		sleep_ms(10);
		elapsed_ms += 10;
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
		pump_drive_levels(false, false);
	}
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

static uint16_t soil_value_to_percent_x10(uint16_t soil_value) {
	uint32_t scaled = (uint32_t)soil_value * 1000u;
	scaled = (scaled + (SOIL_ADC_MAX / 2u)) / SOIL_ADC_MAX;
	if (scaled > 1000u) {
		scaled = 1000u;
	}
	return (uint16_t)scaled;
}

static void water_once_ml(uint32_t ml) {
	const uint32_t run_ms = ml_to_duration_ms(ml);
	pump_set(true);
	sleep_ms(run_ms);
	pump_set(false);
}

int main(void) {
	stdio_init_all();
	setvbuf(stdout, NULL, _IONBF, 0);
	wait_for_serial_or_timeout(SERIAL_WAIT_MS);
	sleep_ms(100);

	// Soil sensor ADC init
	adc_init();
	adc_gpio_init(SOIL_ADC_GPIO);
	adc_select_input(SOIL_ADC_INPUT);

	// Pump GPIO init
	gpio_init(L9110S_IN1_GPIO);
	gpio_set_dir(L9110S_IN1_GPIO, GPIO_OUT);
	gpio_init(L9110S_IN2_GPIO);
	gpio_set_dir(L9110S_IN2_GPIO, GPIO_OUT);
	pump_set(false);

	printf("Soil + Pump controller ready.\n");
	uint16_t dry_threshold_value = (uint16_t)(SOIL_DRY_THRESHOLD_VALUE / DRY_SENSITIVITY_DIVISOR);
	if (dry_threshold_value == 0u) {
		dry_threshold_value = 1u;
	}
	const uint16_t dry_threshold_percent_x10 = soil_value_to_percent_x10(dry_threshold_value);
	printf("Dry threshold(value <= %d, %u.%u%%), sensitivity_div=%d, dose=%dml, repeat=%dms\n",
			dry_threshold_value,
			dry_threshold_percent_x10 / 10u,
			dry_threshold_percent_x10 % 10u,
			DRY_SENSITIVITY_DIVISOR,
			WATER_DOSE_ML,
			REPEAT_INTERVAL_MS);

	uint64_t next_watering_allowed_ms = 0;
	uint64_t next_sensor_log_ms = 0;
	uint64_t next_dry_wait_log_ms = 0;

	while (true) {
		const uint64_t now = now_ms();
		const uint16_t soil_raw = adc_read();
		const uint16_t soil_value = (uint16_t)(SOIL_ADC_MAX - soil_raw);
		const uint16_t soil_percent_x10 = soil_value_to_percent_x10(soil_value);
		const bool is_dry = (soil_value <= dry_threshold_value);

		if (now >= next_sensor_log_ms) {
			printf("[SOIL] 토양수분 비율: %u.%u%% (임계값: %u.%u%%) | dry=%s\n",
					soil_percent_x10 / 10u,
					soil_percent_x10 % 10u,
					dry_threshold_percent_x10 / 10u,
					dry_threshold_percent_x10 % 10u,
					is_dry ? "YES" : "NO");
			next_sensor_log_ms = now + SENSOR_LOG_INTERVAL_MS;
		}

		if (is_dry) {
			if (now >= next_watering_allowed_ms) {
				printf("[PUMP] 토양 건조 감지(토양수분 %u.%u%%) -> %dml 급수 시작\n",
						soil_percent_x10 / 10u,
						soil_percent_x10 % 10u,
						WATER_DOSE_ML);
				water_once_ml(WATER_DOSE_ML);
				printf("물 줬습니다!\n");
				next_watering_allowed_ms = now_ms() + REPEAT_INTERVAL_MS;
				next_dry_wait_log_ms = now_ms() + DRY_WAIT_LOG_INTERVAL_MS;
			} else {
				if (now >= next_dry_wait_log_ms) {
					printf("[PUMP] 건조 상태 유지, 다음 급수까지 %lu ms\n",
							(unsigned long)(next_watering_allowed_ms - now));
					next_dry_wait_log_ms = now + DRY_WAIT_LOG_INTERVAL_MS;
				}
			}
		}

		sleep_ms(100);
	}
}

// -----------------------------
// 핀 연결 방법 (Pico 2W)
// -----------------------------
// 1) 토양 수분 센서 (아날로그 사용)
// - 센서 VCC  -> Pico 2W 3.3V 3.3 out이랑 en중에 하나 연결
// - 센서 GND  -> Pico 2W GND
// - 센서 AO   -> Pico 2W GP26 (ADC0)
//   (참고: 이 파일은 DO 핀은 사용하지 않습니다)
//
// 2) 워터펌프 + L9110S
// - Pico GP15 -> L9110S IN1(A-IA)
// - Pico GP14 -> L9110S IN2(A-IB)
// - 펌프 2선   -> L9110S OA/OB
// - L9110S VCC -> 외부 전원 + (펌프 정격 전압)
// - L9110S GND -> 외부 전원 GND
// - Pico GND   -> L9110S GND (반드시 공통 접지)
//
// 3) Pico 2W와 PC 연결
// - USB 케이블로 Pico 2W를 PC와 연결
// - 빌드한 UF2를 보드에 업로드
// - 시리얼 모니터를 열면 로그와 "물 줬습니다!" 메시지 확인 가능
// 측정 기준 예시(raw): 마른 흙 약 4000, 물에 담갔을 때 약 2000
// 변환값(value = 4095 - raw): 마른 흙 약 95, 물에 담갔을 때 약 2095
// 기본 임계값은 1100이며, DRY_SENSITIVITY_DIVISOR=2 적용 시 실사용 임계값은 550
// 현재 value가 실사용 임계값 이하면 건조한 것으로 간주하고 물을 줍니다.
//수치는 정상적으로 바뀌지만 같은곳에 수분공급없이 놓았는데 계속 수치가 떨어짐
// 5분정도밖에 지나지 않았음에도 60에서 20까지 떨어짐. 코드를 바꿔서 해결가능한지 :


//pico 보드로 들어가는 전원은 5V이지만
//pico 보드의 GPIO는 3.3V에서 동작하기 때문에, 펌프 드라이버(L9110S)와 펌프는 외부 전원을 사용해야 합니다.

//보드에 들어가는 전압이나 전력을 확인하고 출력하려면 멀티미터가 필요합니다. 멀티미터를 사용하여 전압과 전류를 측정할 수 있습니다.
//피코보드 자체에는 전압이나 전류를 측정하는 기능이 없으므로, 멀티미터를 사용하여 외부에서 측정해야 합니다.

//