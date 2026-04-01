#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

int main() {
    stdio_init_all();

    // ADC 설정 (A0 핀 - GP26)
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0); // 26번 핀은 ADC 0번 채널입니다.

    // Digital 설정 (D0 핀 - GP15)
    const uint D0_PIN = 15;
    gpio_init(D0_PIN);
    gpio_set_dir(D0_PIN, GPIO_IN);

    

    while (true) {
        // Analog 값 읽기 (0 ~ 4095)
        uint16_t raw_value = adc_read();
        
        // Digital 값 읽기 (0 또는 1)
        bool digital_value = gpio_get(D0_PIN);

        printf("Analog: %u, Digital: %d\n", raw_value, digital_value);

        sleep_ms(100); // 0.1초 대기
    }
}