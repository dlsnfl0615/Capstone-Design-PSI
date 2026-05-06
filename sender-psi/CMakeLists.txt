cmake_minimum_required(VERSION 3.20)
project(psi LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 실행 파일 이름 설정 (반복 사용을 위해 변수로 관리)
set(EXE_NAME windowing)

# 패키지 찾기
find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)

# 실행 파일 생성
add_executable(${EXE_NAME}
    src/data_loader.cpp
    src/hashing.cpp
    src/evaluate.cpp
    src/sender-main.cpp
    src/mod.cpp
    external/murmur3.cpp
)

# 컴파일 옵션 (MSVC 전용 utf-8 설정)
if(MSVC)
    target_compile_options(${EXE_NAME} PRIVATE /utf-8)
endif()

# 인클루드 디렉토리 설정
target_include_directories(${EXE_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# 라이브러리 연결
target_link_libraries(${EXE_NAME} PRIVATE SEAL::seal)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${EXE_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

# 데이터 폴더를 빌드 디렉토리로 복사
# 실행 파일이 csv 파일을 상대 경로로 찾을 수 있도록 함
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})