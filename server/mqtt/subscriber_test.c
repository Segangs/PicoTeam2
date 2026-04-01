#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "ExampleClientSub"
#define TOPIC       "sensor/data"
#define QOS         1
#define TIMEOUT     10000L

// 메시지가 도착했을 때 실행되는 콜백 함수
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("메시지 도착!\n");
    printf("  topic: %s\n", topicName);
    printf("  message: %.*s\n", message->payloadlen, (char*)message->payload);
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 연결이 끊겼을 때 실행되는 콜백 함수
void connlost(void *context, char *cause) {
    printf("\n연결 종료: %s\n", cause);
}

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // 클라이언트 생성
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // 콜백 설정
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    // 브로커 연결
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("연결 실패, 에러 코드: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    printf("구독 시작: %s (종료하려면 Q 입력)\n", TOPIC);
    MQTTClient_subscribe(client, TOPIC, QOS);

    // Q를 누를 때까지 대기
    int ch;
    do {
        ch = getchar();
    } while (ch != 'Q' && ch != 'q');

    MQTTClient_unsubscribe(client, TOPIC);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}