# include <stdio.h>
# include <stdint.h>

# include "pico/stdlib.h"
# include "hardware/adc.h"

// Wiring (typical analog water level sensor):
// - Sensor VCC -> Pico 3V3
// - Sensor GND -> Pico GND
// - Sensor AO  -> Pico GP26 (ADC0)
# define WATER_SENSOR_ADC_GPIO 26
# define WATER_SENSOR_ADC_INPUT (WATER_SENSOR_ADC_GPIO - 26)

# if (WATER_SENSOR_ADC_GPIO < 26) || (WATER_SENSOR_ADC_GPIO > 29)
# error "WATER_SENSOR_ADC_GPIO must be 26~29 for ADC input"
# endif

// Adjust these two values after calibration on your sensor.
// SENSOR_RAW_DRY: ADC value when sensor is dry.
// SENSOR_RAW_WET: ADC value at the maximum water depth you want to measure.
# define SENSOR_RAW_DRY 0
# define SENSOR_RAW_WET 3200

// Maximum depth represented by SENSOR_RAW_WET (in millimeters).
# define SENSOR_MAX_DEPTH_MM 100

// Number of ADC samples to average each cycle.
# define ADC_AVG_SAMPLES 16

static uint16_t read_adc_average(void) {
	uint32_t sum = 0;

	for (int i = 0; i < ADC_AVG_SAMPLES; ++i) {
		sum += adc_read();
		sleep_ms(2);
        printf("ADC sample %2d: %4u\n", i + 1, (unsigned)sum / (i + 1));
	}

	return (uint16_t)(sum / ADC_AVG_SAMPLES);
}

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

int main(void) {
	stdio_init_all();
	sleep_ms(2000);  // Allow USB serial monitor to attach.

	adc_init();
	adc_gpio_init(WATER_SENSOR_ADC_GPIO);
	adc_select_input(WATER_SENSOR_ADC_INPUT);

	printf("Water depth monitor started.\n");
	printf("ADC mapping: GP%d -> ADC%d\n", WATER_SENSOR_ADC_GPIO, WATER_SENSOR_ADC_INPUT);
	printf("Calib: dry=%d, wet=%d, max_depth=%d mm\n",
		   SENSOR_RAW_DRY,
		   SENSOR_RAW_WET,
		   SENSOR_MAX_DEPTH_MM);
	printf("Tip: if raw changes but depth is fixed, tune SENSOR_RAW_DRY/SENSOR_RAW_WET.\n");

	bool has_prev = false;
	uint16_t prev_raw = 0;
	uint32_t stale_count = 0;

	while (true) {
		const uint16_t raw = read_adc_average();
		const int32_t voltage_mv = ((int32_t)raw * 3300) / 4095;

		if (has_prev) {
			const uint16_t diff = (raw > prev_raw) ? (raw - prev_raw) : (prev_raw - raw);
			if (diff <= 1) {
				stale_count++;
			} else {
				stale_count = 0;
			}

			if (stale_count == 10 || (stale_count > 0 && (stale_count % 30) == 0)) {
				printf("[WARN] ADC raw almost unchanged. Check AO wiring pin and calibration range.\n");
			}
		}
		prev_raw = raw;
		has_prev = true;

		int32_t level_permille = 0;
		if (SENSOR_RAW_WET > SENSOR_RAW_DRY) {
			level_permille = ((int32_t)raw - SENSOR_RAW_DRY) * 1000 /
							 (SENSOR_RAW_WET - SENSOR_RAW_DRY);
			level_permille = clamp_i32(level_permille, 0, 1000);
		}

		const int32_t depth_mm = (level_permille * SENSOR_MAX_DEPTH_MM) / 1000;

		printf("raw=%4u | voltage=%4ld mV | level=%3ld.%01ld%% | depth=%2ld.%01ld cm\n",
			   raw,
			   (long)voltage_mv,
			   (long)(level_permille / 10),
			   (long)(level_permille % 10),
			   (long)(depth_mm / 10),
			   (long)(depth_mm % 10));

		sleep_ms(1000);
	}
}
//브레드 보드와 피코보드 연결 후 센서와 연결하는 방법
//1. 피코보드 와 브레드 보드에 장착하기
//2. 센서의 VCC 핀을 피코보드의 3V3 핀에 연결하기
//3. 센서의 GND 핀을 피코보드의 GND 핀에 연결하기
//4. 센서의 AO 핀을 피코보드의 GP26 핀에 연결하기 (ADC0 입력으로 사용)
//5. 피코보드에 코드를 업로드하고 시리얼 모니터를 통해 수위 측정 결과를 확인하기
//이 코드는 수위 센서에서 아날로그 값을 읽어와서 수위에 따른 전압과 퍼센트, 그리고 수심을 계산하여 출력하는 코드입니다.
//센서의 건조 상태와 최대 수심에 따른 ADC 값을 기준으로 수위를 계산하며, 1초마다 측정 결과를 시리얼 모니터에 출력합니다.
