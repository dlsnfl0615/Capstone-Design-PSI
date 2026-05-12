cmake_minimum_required(VERSION 3.20)
project(psi LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 라이브러리 이름 설정 (Java에서 로드할 이름)
set(LIB_NAME receiver-psi)

# 패키지 찾기
find_package(SEAL CONFIG REQUIRED)

# 실행 파일(.exe)이 아닌 공유 라이브러리(.dll)를 생성함
add_library(${LIB_NAME} SHARED
    src/data_loader.cpp
    src/windowing.cpp
    src/hashing.cpp
    src/mod.cpp
    src/receiver-request.cpp
    src/receiver-result.cpp
    external/murmur3.cpp
)

# 컴파일 옵션 (MSVC 전용 utf-8 설정)
if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /utf-8)
endif()

# 인클루드 디렉토리 설정
target_include_directories(${LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# 라이브러리 연결
target_link_libraries(${LIB_NAME} PRIVATE SEAL::seal)

# 데이터 폴더를 빌드 디렉토리로 복사
# 라이브러리가 로드되어 실행될 때 상대 경로로 파일을 찾을 수 있게 함
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})