# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# [중요] SDK를 불러오기 전에 보드 정보를 먼저 알려줘야 합니다.
set(PICO_BOARD pico2_w CACHE STRING "Board type")
# set(PICO_PLATFORM rp2350 CACHE STRING "Platform type")

# [1] 맥북의 -arch arm64 오류를 방지하는 범용 설정
# 윈도우에서는 이 명령어가 무시되므로 안전합니다.
set(CMAKE_OSX_ARCHITECTURES "")

# [2] SDK 경로 설정
# 팀원 모두가 프로젝트 루트의 deps/pico-sdk에 SDK를 뒀다고 가정합니다. (서브모듈 권장)
if (NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "deps/pico-sdk")
endif()

# [3] SDK 불러오기
# 루트 폴더에 pico_sdk_import.cmake 파일을 복사해두는 것이 가장 깔끔합니다.
include(pico_sdk_import.cmake)

project(PicoTeam2_Project C CXX ASM)

# [4] Pico SDK 초기화
pico_sdk_init()

# [5] 팀원 폴더 추가
#add_subdirectory(src)
add_subdirectory(Segang)
#add_subdirectory(Siyeong)
#add_subdirectory(Jeyoon)