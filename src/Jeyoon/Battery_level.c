#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

/*
 * ==========================================================================
 * 배터리/전압 측정 개요
 * ==========================================================================
 *
 * LCD만으로는 전압이나 배터리 잔량을 직접 알 수 없습니다.
 * LCD는 표시 장치이고, 실제 전압 측정은 ADC + 저항분배기(Voltage Divider)가 필요합니다.
 *
 * 이 예제는 다음과 같이 동작합니다.
 * - ADC 입력(GP26)에서 전압을 읽음
 * - I2C LCD(16x2)에 전압과 배터리 잔량을 표시
 *
 * 기본 가정:
 * - 측정 대상 전압은 0~5V 정도
 * - 저항분배기 비율은 100kΩ : 100kΩ (2:1)
 * - 실제 배터리 종류에 따라 BATTERY_EMPTY_VOLTAGE / BATTERY_FULL_VOLTAGE 를 조정
 *
 * 배터리 잔량은 1셀 리튬배터리(예: 3.0V~4.2V) 기준의 근사치입니다.
 * USB 5V 전압을 측정하면 잔량 대신 전압 상태 확인용으로 보는 것이 더 적절합니다.
 * ==========================================================================
 */

// I2C LCD 설정
#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_BAUDRATE 100000

// PCF8574 기반 1602 LCD에서 가장 흔한 주소. 다르면 0x3F로 바꿔보세요.
#define LCD_I2C_ADDR 0x27

// ADC 설정
#define BATTERY_ADC_GPIO 26
#define BATTERY_ADC_INPUT 0
#define ADC_SAMPLE_COUNT 16
#define ADC_REF_VOLTAGE 3.3f
#define ADC_MAX_VALUE 4095.0f

// 저항분배기 값: Vbat -> R_TOP -> ADC -> R_BOTTOM -> GND
#define R_TOP_OHMS 100000.0f
#define R_BOTTOM_OHMS 100000.0f

// 배터리 잔량 계산용 기준 전압
#define BATTERY_EMPTY_VOLTAGE 3.0f
#define BATTERY_FULL_VOLTAGE 4.2f

// LCD 제어 비트(PCF8574 -> HD44780)
#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

#define LCD_COLS 16
#define LCD_ROWS 2

static const float g_divider_ratio = (R_TOP_OHMS + R_BOTTOM_OHMS) / R_BOTTOM_OHMS;

static void lcd_i2c_write(uint8_t data) {
	i2c_write_blocking(I2C_PORT, LCD_I2C_ADDR, &data, 1, false);
}

static void lcd_pulse_enable(uint8_t data) {
	lcd_i2c_write(data | LCD_EN | LCD_BL);
	sleep_us(1);
	lcd_i2c_write((data & ~LCD_EN) | LCD_BL);
	sleep_us(50);
}

static void lcd_write4bits(uint8_t data) {
	lcd_i2c_write(data | LCD_BL);
	lcd_pulse_enable(data);
}

static void lcd_send(uint8_t value, uint8_t mode) {
	uint8_t high = value & 0xF0;
	uint8_t low = (value << 4) & 0xF0;

	lcd_write4bits(high | mode);
	lcd_write4bits(low | mode);
}

static void lcd_command(uint8_t value) {
	lcd_send(value, 0x00);
}

static void lcd_data(uint8_t value) {
	lcd_send(value, LCD_RS);
}

static void lcd_clear(void) {
	lcd_command(0x01);
	sleep_ms(2);
}

static void lcd_set_cursor(uint8_t col, uint8_t row) {
	static const uint8_t row_offsets[LCD_ROWS] = {0x00, 0x40};
	if (row >= LCD_ROWS) {
		row = LCD_ROWS - 1;
	}
	lcd_command(0x80 | (col + row_offsets[row]));
}

static void lcd_print_padded(const char *text) {
	size_t len = strlen(text);
	for (size_t i = 0; i < len && i < LCD_COLS; ++i) {
		lcd_data((uint8_t)text[i]);
	}
	for (size_t i = len; i < LCD_COLS; ++i) {
		lcd_data(' ');
	}
}

static void lcd_init(void) {
	i2c_init(I2C_PORT, I2C_BAUDRATE);
	gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(I2C_SDA_PIN);
	gpio_pull_up(I2C_SCL_PIN);

	sleep_ms(50);

	// HD44780 4-bit 초기화 시퀀스
	lcd_write4bits(0x30);
	sleep_ms(5);
	lcd_write4bits(0x30);
	sleep_us(150);
	lcd_write4bits(0x30);
	sleep_us(150);
	lcd_write4bits(0x20);
	sleep_us(150);

	lcd_command(0x28); // 4-bit, 2 line, 5x8 dots
	lcd_command(0x08); // display off
	lcd_clear();
	lcd_command(0x06); // entry mode set
	lcd_command(0x0C); // display on, cursor off, blink off
}

static uint16_t read_adc_average(void) {
	uint32_t sum = 0;

	for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
		sum += adc_read();
		sleep_ms(2);
	}

	return (uint16_t)(sum / ADC_SAMPLE_COUNT);
}

static float raw_to_voltage(uint16_t raw) {
	float adc_voltage = ((float)raw * ADC_REF_VOLTAGE) / ADC_MAX_VALUE;
	return adc_voltage * g_divider_ratio;
}

static int voltage_to_percent(float voltage) {
	if (voltage <= BATTERY_EMPTY_VOLTAGE) {
		return 0;
	}

	if (voltage >= BATTERY_FULL_VOLTAGE) {
		return 100;
	}

	float percent = (voltage - BATTERY_EMPTY_VOLTAGE) * 100.0f /
					(BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE);
	return (int)(percent + 0.5f);
}

int main() {
	stdio_init_all();
	sleep_ms(1500);

	adc_init();
	adc_gpio_init(BATTERY_ADC_GPIO);
	adc_select_input(BATTERY_ADC_INPUT);

	lcd_init();

	printf("=== Battery Level Monitor ===\n");
	printf("ADC pin: GP%d, I2C LCD: SDA GP%d / SCL GP%d\n", BATTERY_ADC_GPIO, I2C_SDA_PIN, I2C_SCL_PIN);
	printf("Divider ratio: %.2f\n", g_divider_ratio);

	while (true) {
		uint16_t raw = read_adc_average();
		float voltage = raw_to_voltage(raw);
		int percent = voltage_to_percent(voltage);

		char line1[17];
		char line2[17];

		snprintf(line1, sizeof(line1), "V:%1.2fV RAW:%u", voltage, raw);
		snprintf(line2, sizeof(line2), "BAT:%3d%%       ", percent);

		lcd_set_cursor(0, 0);
		lcd_print_padded(line1);
		lcd_set_cursor(0, 1);
		lcd_print_padded(line2);

		printf("raw=%4u voltage=%1.2fV battery=%3d%%\n", raw, voltage, percent);
		sleep_ms(1000);
	}

	return 0;
}
//핀 연결 방법
// - 배터리 + -> 저항 R_TOP -> ADC GP26 -> 저항 R_BOTTOM -> GND
// - I2C LCD SDA -> GP4, SCL -> GP5
// - LCD 전원은 5V 또는 3.3V (LCD 모델에 따라 다름)와 GND에 연결
// - 배터리 측정 시에는 배터리 전압이 저항분배기를 통해 ADC로 들어오도록 연결해야 합니다. 예를 들어, 1셀 리튬배터리(3.0V~4.2V)를 측정하려면 배터리 +를 R_TOP에 연결하고
//