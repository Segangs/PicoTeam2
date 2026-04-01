cmake_minimum_required(VERSION 3.13)

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
