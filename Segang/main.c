#include <stdio.h>
#include "pico/stdlib.h"

int main()
{
    // 1. 시리얼 통신 초기화
    stdio_init_all();

    // 2. 내장 LED 설정
    // PICO_DEFAULT_LED_PIN은 보통 25번입니다.
    // 기존: const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    // 변경: 숫자 25를 직접 대입
    const uint LED_PIN = 25;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("Pico 2 LED & Serial Test Start!\n");

    while (true)
    {
        // LED 켜기
        gpio_put(LED_PIN, 1);
        printf("[System] LED ON! Heartbeat is pulsing...\n");
        sleep_ms(500);

        // LED 끄기
        gpio_put(LED_PIN, 0);
        printf("[System] LED OFF\n");
        sleep_ms(500);
    }
}