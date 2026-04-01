# 1. 실행 파일 이름 설정 (팀원들과 겹치지 않게 'segang_main'으로 지정)
add_executable(segang_main
    main.c
    #${PICO_SDK_PATH}/lib/lwip/src/apps/mqtt/mqtt.c
    # 추가로 만든 .c 파일이 있다면 여기에 나열하세요.
)

# 2. Pico 라이브러리 연결
target_link_libraries(segang_main
    pico_stdlib
    hardware_gpio
    hardware_adc
    hardware_pwm
    hardware_clocks
    # Wi-Fi 및 MQTT(lwIP) 통신을 위한 백그라운드 스레드 세이프 네트워크 스택
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_lwip_mqtt                              # MQTT 라이브러리 (pico_ 추가)
)

# Wi-Fi 국가 설정 (한국: KR)
target_compile_definitions(segang_main PRIVATE
    CYW43_LWIP=1
    WIFI_SSID=\"A******\" # 비공개 처리 26.4.6.
    WIFI_PASSWORD=\"*******\" # 비공개 처리 26.4.6.
)

target_include_directories(segang_main PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_SDK_PATH}/lib/lwip/src/include
)

# 3. USB 시리얼 통신 활성화 (보드에 LED가 없으므로 필수!)
# 이 설정을 해야 printf() 결과가 맥의 터미널(screen)에 보입니다.
pico_enable_stdio_usb(segang_main 1)
pico_enable_stdio_uart(segang_main 0)

# 4. UF2 파일 등 빌드 결과물 생성
pico_add_extra_outputs(segang_main)