cmake_minimum_required(VERSION 3.20)
project(psi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)

# 공통 소스 파일 (모든 프로그램에서 공통으로 사용하는 파일들)
set(COMMON_SOURCES
    receiver/fhe_setup.cpp
    receiver/fhe_setup.h
    receiver/hashing.cpp
    receiver/hashing.h
    receiver/data_loader.cpp
    receiver/data_loader.h
    external/murmur3.cpp
)

# 1. Receiver Request (수신자 요청 생성)
add_executable(receiver_request
    ${COMMON_SOURCES}
    receiver/receiver_request.cpp
    receiver/psi_receiver.cpp
    receiver/psi_receiver.h
)

# 2. Sender (송신자 연산)
add_executable(sender
    sender/data_loader.h
    sender/data_loader.cpp
    sender/fhe_setup.h
    sender/fhe_setup.cpp
    sender/hashing.h
    sender/hashing.cpp
    sender/sender.cpp
    sender/psi_sender.cpp
    sender/psi_sender.h
    external/murmur3.cpp
)

# 3. Receiver Result (수신자 결과 확인)
add_executable(receiver_result
    ${COMMON_SOURCES}
    receiver/receiver_result.cpp
    receiver/psi_receiver.cpp
    receiver/psi_receiver.h
)

# 컴파일 옵션 및 라이브러리 연결
add_compile_options(/utf-8)

foreach(exe receiver_request sender receiver_result)
    target_include_directories(${exe} PRIVATE 
        ${CMAKE_SOURCE_DIR}/receiver
        ${CMAKE_SOURCE_DIR}/sender
        ${CMAKE_SOURCE_DIR}/external
    )
    target_link_libraries(${exe} PRIVATE SEAL::seal)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${exe} PRIVATE OpenMP::OpenMP_CXX)
    endif()
endforeach()