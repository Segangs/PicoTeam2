cmake_minimum_required(VERSION 3.13)

# [중요] 1. 맥용 플래그가 추가되는 것을 방지 (project 명령어 전에 위치해야 함)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_C_STANDARD 17)

# [2] 컴파일러 수동 지정 (CMake가 헷갈리지 않게 확실히 못박기)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc) # 어셈블러도 GCC를 사용

# Pico 칩셋과 보드이름 강제 지정
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pico2)


# 1. SDK 경로를 프로젝트 내부의 deps/pico-sdk로 강제 지정합니다.
# 이렇게 하면 팀원들이 개별적으로 환경변수를 잡지 않아도 됩니다.
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/pico-sdk")

# 2. SDK 내부의 필수 파일을 불러옵니다.
# (pico-sdk/external/pico_sdk_import.cmake 파일을 루트로 복사해두면 더 편리합니다)
include(deps/pico-sdk/external/pico_sdk_import.cmake)

# project(SmartFarm_Project C CXX ASM)
project(PicoTeam2_Project C CXX ASM)


# 3. Pico SDK를 초기화합니다.
pico_sdk_init()


# 4. 각 팀원의 폴더나 src 폴더를 포함시킵니다.
# (이 기능을 통해 각 폴더 내부의 CMakeLists.txt를 읽어옵니다)

#add_subdirectory(src)
#add_subdirectory(Segang)
add_subdirectory(Siyeong)
#add_subdirectory(Jeyoon)