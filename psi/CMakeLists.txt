cmake_minimum_required(VERSION 3.20)

project(psi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)

add_executable(psi
    receiver/data_loader.cpp
    receiver/data_loader.h
    receiver/hashing.h
    receiver/hashing.cpp
    receiver/receiver.cpp
    receiver/psi_receiver.h
    receiver/psi_receiver.cpp
    external/murmur3.cpp
)

target_include_directories(psi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)

# 기본 라이브러리 연결
target_link_libraries(psi PRIVATE SEAL::seal)

# OpenMP가 발견되었을 때만 연결하도록 수정
if(OpenMP_CXX_FOUND)
    target_link_libraries(psi PRIVATE OpenMP::OpenMP_CXX)
endif()