#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"



// ==========================
// I2C 설정
// ==========================
#define I2C_PORT i2c0
#define SDA_PIN 4
#define SCL_PIN 5

// ==========================
// SHT2x 센서 주소 & 명령어
// ==========================
#define SHT2X_ADDR 0x40

#define CMD_MEASURE_TEMP 0xF3
#define CMD_MEASURE_HUMI 0xF5

// ==========================
// LCD (PCF8574) 주소
// ==========================
#define LCD_ADDR_1 0x27
#define LCD_ADDR_2 0x3F
static uint8_t LCD_ADDR = LCD_ADDR_2;

void i2c_scan() {
    bool found_lcd27 = false;
    bool found_lcd3f = false;
    bool found_sht2x = false;

    printf("I2C scan start...\n");
    for (int addr = 1; addr < 0x7F; addr++) {
        int ret = i2c_write_blocking(I2C_PORT, addr, NULL, 0, false);
        if (ret >= 0) {
            printf("I2C device found at 0x%02X\n", addr);
            if (addr == LCD_ADDR_1) found_lcd27 = true;
            if (addr == LCD_ADDR_2) found_lcd3f = true;
            if (addr == SHT2X_ADDR) found_sht2x = true;
        }
    }

    if (found_lcd3f) {
        LCD_ADDR = LCD_ADDR_2;
    } else if (found_lcd27) {
        LCD_ADDR = LCD_ADDR_1;
    }

    printf("Selected LCD address: 0x%02X\n", LCD_ADDR);
    printf("SHT2x detected: %s\n", found_sht2x ? "Yes" : "No");
}

// LCD 제어 비트
#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04

// ==========================
// LCD 함수
// ==========================

// LCD에 데이터 전송
void lcd_send_byte(uint8_t data, uint8_t mode) {
    uint8_t high = mode | (data & 0xF0) | LCD_BACKLIGHT;
    uint8_t low  = mode | ((data << 4) & 0xF0) | LCD_BACKLIGHT;

    uint8_t buf[1];

    // 상위 4비트 전송
    buf[0] = high | ENABLE;
    i2c_write_blocking(I2C_PORT, LCD_ADDR, buf, 1, false);
    sleep_us(100);
    buf[0] = high;
    i2c_write_blocking(I2C_PORT, LCD_ADDR, buf, 1, false);

    // 하위 4비트 전송
    buf[0] = low | ENABLE;
    i2c_write_blocking(I2C_PORT, LCD_ADDR, buf, 1, false);
    sleep_us(100);
    buf[0] = low;
    i2c_write_blocking(I2C_PORT, LCD_ADDR, buf, 1, false);
}

// 명령 전송
void lcd_cmd(uint8_t cmd) {
    lcd_send_byte(cmd, 0x00);
}

// 문자 전송
void lcd_char(char val) {
    lcd_send_byte(val, 0x01);
}

// 문자열 출력
void lcd_string(const char *str) {
    while (*str) {
        lcd_char(*str++);
    }
}

// LCD 초기화
void lcd_init() {
    sleep_ms(50);

    lcd_cmd(0x33);
    lcd_cmd(0x32);
    lcd_cmd(0x28); // 4비트 모드, 2줄
    lcd_cmd(0x0C); // 디스플레이 ON
    lcd_cmd(0x06); // 커서 이동
    lcd_cmd(0x01); // 화면 클리어
    sleep_ms(5);
}

// 커서 위치 설정
void lcd_set_cursor(int row, int col) {
    uint8_t addr = (row == 0 ? 0x80 : 0xC0) + col;
    lcd_cmd(addr);
}

// ==========================
// SHT2x 센서 읽기 함수
// ==========================

// 온도 읽기
float read_temperature() {
    uint8_t cmd = CMD_MEASURE_TEMP;
    uint8_t data[2];

    i2c_write_blocking(I2C_PORT, SHT2X_ADDR, &cmd, 1, true);
    sleep_ms(100);

    i2c_read_blocking(I2C_PORT, SHT2X_ADDR, data, 2, false);

    uint16_t raw = (data[0] << 8) | data[1];

    // 온도 계산 공식 (데이터시트 기준)
    float temp = -46.85 + (175.72 * raw / 65536.0);

    return temp;
}

// 습도 읽기
float read_humidity() {
    uint8_t cmd = CMD_MEASURE_HUMI;
    uint8_t data[2];

    i2c_write_blocking(I2C_PORT, SHT2X_ADDR, &cmd, 1, true);
    sleep_ms(100);

    i2c_read_blocking(I2C_PORT, SHT2X_ADDR, data, 2, false);

    uint16_t raw = (data[0] << 8) | data[1];

    // 습도 계산 공식
    float humi = -6 + (125.0 * raw / 65536.0);

    return humi;
}

// ==========================
// 메인 함수
// ==========================
int main() {
    stdio_init_all();

    // I2C 초기화 (100kHz)
    i2c_init(I2C_PORT, 100 * 1000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // I2C 버스 스캔 후 LCD 주소 자동 선택
    i2c_scan();

    // LCD 초기화
    lcd_init();

    char buffer[32];

    while (1) {
        float temp = read_temperature();
        float humi = read_humidity();

        // LCD 출력
        lcd_cmd(0x01); // 화면 초기화

        lcd_set_cursor(0, 0);
        sprintf(buffer, "Temp: %.2f C", temp);
        lcd_string(buffer);

        lcd_set_cursor(1, 0);
        sprintf(buffer, "Humi: %.2f %%", humi);
        lcd_string(buffer);

        sleep_ms(1000);
    }
}