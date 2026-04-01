#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

int main() {
    // 1. 표준 입출력(UART) 초기화 (printf 사용을 위함)
    stdio_init_all();

    // 2. ADC 하드웨어 초기화
    adc_init();

    // 3. GP26 핀을 ADC 입력으로 설정 (ADC 0번 채널)
    adc_gpio_init(26);
    adc_select_input(0);

    printf("--- 조도 센서 모니터링 시작 (C 언어) ---\n");

    while (true) {
        // 4. ADC 값 읽기 (0 ~ 4095)
        // Pico의 ADC는 12비트 해상도를 가집니다.
        uint16_t raw_value = adc_read();

        // 5. 전압 값으로 계산 (3.3V 기준)
        const float conversion_factor = 3.3f / (1 << 12);
        float voltage = raw_value * conversion_factor;

        // 6. 시리얼 모니터에 출력
        printf("Raw Value: %u | Voltage: %.2f V\n", raw_value, voltage);

        // 0.5초 대기
        sleep_ms(500);
    }

    return 0;
}