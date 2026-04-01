#include "pico/stdlib.h"

int main() {
    // GPIO 25번을 출력으로 설정 (내장 LED)
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true) {
        gpio_put(LED_PIN, 1);  // LED ON
        sleep_ms(500);         // 500ms 대기

        gpio_put(LED_PIN, 0);  // LED OFF
        sleep_ms(500);         // 500ms 대기
    }
}