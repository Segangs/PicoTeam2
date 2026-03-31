# 1. 실행 파일 이름 설정 (팀원들과 겹치지 않게 'jeyoon_servo'로 지정)
add_executable(jeyoon_servo
    servomotor.c
    # 추가로 만든 .c 파일이 있다면 여기에 나열하세요.
)

# Build-time Wi-Fi credential injection.
# Example:
# cmake -S . -B build -DJY_WIFI_SSID="MyWiFi" -DJY_WIFI_PASSWORD="MyPass"
set(JY_WIFI_SSID "" CACHE STRING "Wi-Fi SSID for jeyoon_servo")
set(JY_WIFI_PASSWORD "" CACHE STRING "Wi-Fi password for jeyoon_servo")

# 2. Pico 표준 라이브러리 및 PWM 하드웨어 라이브러리 연결
target_link_libraries(jeyoon_servo 
    pico_stdlib
    hardware_pwm
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

target_include_directories(jeyoon_servo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

if(JY_WIFI_SSID)
    target_compile_definitions(jeyoon_servo PRIVATE WIFI_SSID="${JY_WIFI_SSID}")
endif()

if(JY_WIFI_PASSWORD)
    target_compile_definitions(jeyoon_servo PRIVATE WIFI_PASSWORD="${JY_WIFI_PASSWORD}")
endif()

# 3. USB 시리얼 통신 활성화 (보드에 LED가 없으므로 필수!)
# 이 설정을 해야 printf() 결과가 터미널에 보입니다.
pico_enable_stdio_usb(jeyoon_servo 1)
pico_enable_stdio_uart(jeyoon_servo 0)

# 4. UF2 파일 등 빌드 결과물 생성
pico_add_extra_outputs(jeyoon_servo)
