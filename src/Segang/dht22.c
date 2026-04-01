#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define DHT_PIN 15
#define MAX_TIMESTEPS 85

typedef struct {
    float humidity;
    float temperature;
} dht_result;

bool read_from_dht(dht_result *result) {
    int data[5] = {0, 0, 0, 0, 0};
    uint last_state = 1;
    uint j = 0;

    // 1. 센서에 시작 신호 전송
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    gpio_put(DHT_PIN, 0);
    sleep_ms(20);         // 최소 18ms 이상 유지
    gpio_put(DHT_PIN, 1);
    sleep_us(40);         // 20~40us 대기
    gpio_set_dir(DHT_PIN, GPIO_IN);

    // 2. 센서 응답 및 데이터 읽기 (타이밍 체크)
    for (uint i = 0; i < MAX_TIMESTEPS; i++) {
        uint count = 0;
        while (gpio_get(DHT_PIN) == last_state) {
            count++;
            sleep_us(1);
            if (count == 255) break;
        }
        last_state = gpio_get(DHT_PIN);
        if (count == 255) break;

        // 실제 데이터 비트 해석 (처음 3번의 상태 변화는 무시 - 응답 신호)
        if ((i >= 4) && (i % 2 == 0)) {
            data[j / 8] <<= 1;
            if (count > 40) data[j / 8] |= 1; // 신호가 길면 1, 짧으면 0
            j++;
        }
    }

    // 3. 체크섬 확인 및 데이터 변환 (40비트 읽기 성공 시)
    if ((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))) {
        result->humidity = (float)((data[0] << 8) + data[1]) / 10;
        result->temperature = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
        if (data[2] & 0x80) result->temperature *= -1; // 영하 온도 처리
        return true;
    } else {
        return false;
    }
}

int main() {
    stdio_init_all();
    gpio_init(DHT_PIN);

    printf("Pico 2 DHT22 센서 테스트 시작\n");

    while (1) {
        dht_result res;
        if (read_from_dht(&res)) {
            printf("습도: %.1f%%, 온도: %.1f°C\n", res.humidity, res.temperature);
        } else {
            printf("데이터 읽기 실패 (체크섬 오류 또는 연결 확인)\n");
        }
        sleep_ms(2000); // DHT22는 2초 간격 권장
    }
}