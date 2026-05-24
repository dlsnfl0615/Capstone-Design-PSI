cmake_minimum_required(VERSION 3.20)
project(receiver-psi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_NAME receiver-psi)

find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)

set(COMMON_SOURCES
        src/data_loader.cpp
        src/windowing.cpp
        src/hashing.cpp
        src/mod.cpp
        external/murmur3.cpp
)

# 💡 Dockerfile의 요구사항에 맞춰 단일 SHARED(동적 .so) 라이브러리로 병합
add_library(${LIB_NAME} SHARED
        ${COMMON_SOURCES}
        src/receiver-request.cpp
        src/receiver-result.cpp
)

# 컴파일 옵션 설정
if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /utf-8)
else()
    target_compile_options(${LIB_NAME} PRIVATE -fPIC)
endif()

# 인클루드 디렉토리 설정
target_include_directories(${LIB_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# 라이브러리 연결
target_link_libraries(${LIB_NAME} PRIVATE SEAL::seal)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()