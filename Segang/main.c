#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // CYW43 라이브러리 포함

int main() {
    stdio_init_all();
    sleep_ms(2000); // USB 시리얼 연결이 안정화될 때까지 대기

    // CYW43 드라이버 초기화 (Wi-Fi 칩)
    if (cyw43_arch_init()) {
        printf("CYW43 초기화 실패\n");
        return -1;
    }

    while (true) {
        // WL_GPIO0에 연결된 LED 켜기
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(500);

        // LED 끄기
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(500);
    }
}