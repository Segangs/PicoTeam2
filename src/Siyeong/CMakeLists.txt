cmake_minimum_required(VERSION 3.13)

<<<<<<< HEAD
# 프로젝트 이름
project(pico_test C CXX ASM)

# 실행파일
add_executable(pico_test
    TempHumiTest.c
)

# 라이브러리 링크
target_link_libraries(pico_test
    pico_stdlib
    hardware_i2c
)

# USB 출력 활성화
pico_enable_stdio_usb(pico_test 1)
pico_enable_stdio_uart(pico_test 0)

# UF2 생성
pico_add_extra_outputs(pico_test)
=======
# 1. SDK 경로 설정 및 가져오기
set(PICO_SDK_PATH "/home/sia/PicoTeam2/deps/pico-sdk")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_test C CXX ASM)

# 2. SDK 초기화
pico_sdk_init()

# 3. 실행파일 생성
add_executable(pico_test1 Test1.c)

# 4. 헤더 파일 경로 추가 (lwipopts.h 위치 확보)
target_include_directories(pico_test1 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    /home/sia/PicoTeam2/deps/pico-sdk/test/kitchen_sink
)

# 5. Wi-Fi 및 MQTT 설정 정의 (C 코드에서 사용)
target_compile_definitions(pico_test1 PRIVATE
    WIFI_SSID=\"bindsoft\"
    WIFI_PASSWORD=\"bindsoft24\"
    MQTT_SERVER=\"221.145.210.230\"
)

# 6. 라이브러리 링크 (여기에 필요한 모든 라이브러리 집중)
target_link_libraries(pico_test1
    pico_stdlib
    hardware_adc
    hardware_pwm
    hardware_clocks
    hardware_gpio
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
)

# [삭제됨] pico_set_lwip_runtime_config(pico_test1 1) <- 에러 원인이므로 제거

# 7. 표준 입출력 설정
pico_enable_stdio_usb(pico_test1 1)
pico_enable_stdio_uart(pico_test1 0)

# 8. UF2 파일 생성
pico_add_extra_outputs(pico_test1)
>>>>>>> 52b4d37 (readme 수정)
