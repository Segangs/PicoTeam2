<<<<<<< HEAD
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
=======
# 1. 실행 파일 설정
add_executable(jeyoon_servomotor
    servomotor.c
)

add_executable(jeyoon_watersensor
    watersensor.c
)

add_executable(jeyoon_waterpump
    waterpump.c
)

add_executable(jeyoon_soil_pump
    soil_pump.c
)

add_executable(jeyoon_battery_test
    Battery_test.c
)

add_executable(jeyoon_battery_level
    Battery_level.c
)

add_executable(jeyoon_motortest
    motortest.c
)
# Build-time Wi-Fi credential injection.
# Example:
# cmake -S . -B build -DJY_WIFI_SSID="MyWiFi" -DJY_WIFI_PASSWORD="MyPass"
set(JY_WIFI_SSID "" CACHE STRING "Wi-Fi SSID for jeyoon")
set(JY_WIFI_PASSWORD "" CACHE STRING "Wi-Fi password for jeyoon")

# 2. 서보 제어용 타깃 라이브러리
target_link_libraries(jeyoon_servomotor
>>>>>>> 52b4d37 (readme 수정)
    pico_stdlib
    hardware_pwm
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

<<<<<<< HEAD
target_include_directories(jeyoon_servo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

if(JY_WIFI_SSID)
    target_compile_definitions(jeyoon_servo PRIVATE WIFI_SSID="${JY_WIFI_SSID}")
endif()

if(JY_WIFI_PASSWORD)
    target_compile_definitions(jeyoon_servo PRIVATE WIFI_PASSWORD="${JY_WIFI_PASSWORD}")
=======
target_include_directories(jeyoon_servomotor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

if (JY_WIFI_SSID)
    target_compile_definitions(jeyoon_servomotor PRIVATE WIFI_SSID="${JY_WIFI_SSID}")
endif()

if (JY_WIFI_PASSWORD)
    target_compile_definitions(jeyoon_servomotor PRIVATE WIFI_PASSWORD="${JY_WIFI_PASSWORD}")
>>>>>>> 52b4d37 (readme 수정)
endif()

# 3. USB 시리얼 통신 활성화 (보드에 LED가 없으므로 필수!)
# 이 설정을 해야 printf() 결과가 터미널에 보입니다.
<<<<<<< HEAD
pico_enable_stdio_usb(jeyoon_servo 1)
pico_enable_stdio_uart(jeyoon_servo 0)

# 4. UF2 파일 등 빌드 결과물 생성
pico_add_extra_outputs(jeyoon_servo)
=======
pico_enable_stdio_usb(jeyoon_servomotor 1)
pico_enable_stdio_uart(jeyoon_servomotor 0)

# 4. UF2 파일 등 빌드 결과물 생성
pico_add_extra_outputs(jeyoon_servomotor)

# 5. 워터센서 전용 타깃 라이브러리
target_link_libraries(jeyoon_watersensor
    pico_stdlib
    hardware_adc
)

target_include_directories(jeyoon_watersensor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_watersensor 1)
pico_enable_stdio_uart(jeyoon_watersensor 0)
pico_add_extra_outputs(jeyoon_watersensor)

# 6. 워터펌프 전용 타깃 라이브러리
target_link_libraries(jeyoon_waterpump
    pico_stdlib
)

target_compile_definitions(jeyoon_waterpump PRIVATE
    PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=0
    PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
)

target_include_directories(jeyoon_waterpump PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_waterpump 1)
pico_enable_stdio_uart(jeyoon_waterpump 0)
pico_add_extra_outputs(jeyoon_waterpump)

# 7. 토양센서 + 워터펌프 연동 타깃
target_link_libraries(jeyoon_soil_pump
    pico_stdlib
    hardware_adc
)

target_compile_definitions(jeyoon_soil_pump PRIVATE
    PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=0
    PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
)

target_include_directories(jeyoon_soil_pump PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_soil_pump 1)
pico_enable_stdio_uart(jeyoon_soil_pump 0)
pico_add_extra_outputs(jeyoon_soil_pump)

# 8. 배터리 테스트: 서보모터 + 워터펌프 동시 제어
target_link_libraries(jeyoon_battery_test
    pico_stdlib
    hardware_pwm
    hardware_gpio
)

if (TARGET pico_cyw43_arch_none)
    target_link_libraries(jeyoon_battery_test
        pico_cyw43_arch_none
    )
endif()

target_include_directories(jeyoon_battery_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_battery_test 1)
pico_enable_stdio_uart(jeyoon_battery_test 0)
pico_add_extra_outputs(jeyoon_battery_test)

# 9. 배터리/전압 표시용 LCD 타깃
target_link_libraries(jeyoon_battery_level
    pico_stdlib
    hardware_adc
    hardware_i2c
)

target_include_directories(jeyoon_battery_level PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_battery_level 1)
pico_enable_stdio_uart(jeyoon_battery_level 0)
pico_add_extra_outputs(jeyoon_battery_level)

# 10. 모터 테스트(시리얼 제어) 타깃
target_link_libraries(jeyoon_motortest
    pico_stdlib
    hardware_pwm
)

target_include_directories(jeyoon_motortest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(jeyoon_motortest 1)
pico_enable_stdio_uart(jeyoon_motortest 0)
pico_add_extra_outputs(jeyoon_motortest)
>>>>>>> 52b4d37 (readme 수정)
