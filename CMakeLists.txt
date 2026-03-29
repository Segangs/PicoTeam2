cmake_minimum_required(VERSION 3.13)
project(PicoTeam2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)

# 1. SDK 경로를 프로젝트 내부의 deps/pico-sdk로 강제 지정합니다.
# 이렇게 하면 팀원들이 개별적으로 환경변수를 잡지 않아도 됩니다.
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/pico-sdk")

# 2. SDK 내부의 필수 파일을 불러옵니다.
# (pico-sdk/external/pico_sdk_import.cmake 파일을 루트로 복사해두면 더 편리합니다)
include(deps/pico-sdk/external/pico_sdk_import.cmake)

project(SmartFarm_Project C CXX ASM)

# 3. Pico SDK를 초기화합니다.
pico_sdk_init()

# 4. 각 팀원의 폴더나 src 폴더를 포함시킵니다.
# (이 기능을 통해 각 폴더 내부의 CMakeLists.txt를 읽어옵니다)

add_subdirectory(src)
add_subdirectory(Segang)
add_subdirectory(Siyeong)
add_subdirectory(Jeyoon)