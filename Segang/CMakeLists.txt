# 1. 실행 파일 이름 설정 (팀원들과 겹치지 않게 'segang_main'으로 지정)
add_executable(segang_main
    soilmoisture.c
    # 추가로 만든 .c 파일이 있다면 여기에 나열하세요.
)

# 2. Pico 라이브러리 연결
target_link_libraries(segang_main 
    pico_stdlib
    pico_cyw43_arch_none
    hardware_adc
)

# 3. USB 시리얼 통신 활성화 (보드에 LED가 없으므로 필수!)
# 이 설정을 해야 printf() 결과가 맥의 터미널(screen)에 보입니다.
pico_enable_stdio_usb(segang_main 1)
pico_enable_stdio_uart(segang_main 0)

# 4. UF2 파일 등 빌드 결과물 생성
pico_add_extra_outputs(segang_main)
