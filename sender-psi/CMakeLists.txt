cmake_minimum_required(VERSION 3.20)
project(psi LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SEAL 경로 직접 지정
set(SEAL_DIR "/home/2022111531/local/lib/cmake/SEAL-4.1" CACHE PATH "Path to SEALConfig.cmake")

# 패키지 찾기
find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)
find_package(Threads REQUIRED)

# =========================
# 1. 실제 sender 실행 파일
# =========================
add_executable(sender
        src/data_loader.cpp
        src/hashing.cpp
        src/evaluate.cpp
        src/sender-main.cpp
        src/mod.cpp
        src/cache_io.cpp
        src/metadata.cpp
        external/murmur3.cpp
)

# =========================
# 2. 전처리 실행 파일
# =========================
add_executable(preprocess_sender
        src/preprocess_main.cpp
        src/data_loader.cpp
        src/hashing.cpp
        src/cache_io.cpp
        src/metadata.cpp
        external/murmur3.cpp
)

# =========================
# 공통 include 설정
# =========================
target_include_directories(sender PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        /home/2022111531/local/include/SEAL-4.1
)

target_include_directories(preprocess_sender PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        /home/2022111531/local/include/SEAL-4.1
)

# =========================
# MSVC 옵션
# =========================
if(MSVC)
    target_compile_options(sender PRIVATE /utf-8)
    target_compile_options(preprocess_sender PRIVATE /utf-8)
endif()

# =========================
# 라이브러리 연결
# =========================
target_link_libraries(sender PRIVATE SEAL::seal Threads::Threads)
target_link_libraries(preprocess_sender PRIVATE Threads::Threads)

if(OpenMP_CXX_FOUND)
    target_link_libraries(sender PRIVATE OpenMP::OpenMP_CXX)
    target_link_libraries(preprocess_sender PRIVATE OpenMP::OpenMP_CXX)
endif()

# 데이터 폴더를 빌드 디렉토리로 복사
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})