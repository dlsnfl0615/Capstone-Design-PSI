cmake_minimum_required(VERSION 3.20)
project(psi LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# SEAL 경로 직접 지정
# =========================
set(SEAL_DIR "/home/2022111531/local/lib/cmake/SEAL-4.1" CACHE PATH "Path to SEALConfig.cmake")

# 패키지 찾기
find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)

# 공통 소스
set(COMMON_SOURCES
        src/data_loader.cpp
        src/windowing.cpp
        src/hashing.cpp
        src/mod.cpp
        external/murmur3.cpp
)

# receiver-request 실행 파일
add_executable(receiver-request
        ${COMMON_SOURCES}
        src/receiver-request.cpp
)

# receiver-result 실행 파일
add_executable(receiver-result
        ${COMMON_SOURCES}
        src/receiver-result.cpp
)

# 공통 설정을 적용할 target 목록
set(TARGETS
        receiver-request
        receiver-result
)

foreach(target ${TARGETS})

    # 컴파일 옵션
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()

    # 인클루드 디렉토리 설정
    target_include_directories(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/external
            /home/2022111531/local/include/SEAL-4.1
    )

    # SEAL 라이브러리 연결
    target_link_libraries(${target} PRIVATE SEAL::seal)

    # OpenMP 연결
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
    endif()

endforeach()

# 데이터 폴더를 빌드 디렉토리로 복사
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})