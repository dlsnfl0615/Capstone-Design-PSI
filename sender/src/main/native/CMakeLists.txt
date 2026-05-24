cmake_minimum_required(VERSION 3.20)
project(sender-psi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_NAME sender-psi)

find_package(SEAL CONFIG REQUIRED)
find_package(OpenMP)
find_package(Threads REQUIRED)

set(COMMON_SOURCES
        src/data_loader.cpp
        src/hashing.cpp
        src/evaluate.cpp
        src/mod.cpp
        src/cache_io.cpp
        src/metadata.cpp
        external/murmur3.cpp
)

add_library(${LIB_NAME} SHARED
        ${COMMON_SOURCES}
        src/sender-main.cpp
        src/preprocess_main.cpp
)

if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /utf-8)
else()
    target_compile_options(${LIB_NAME} PRIVATE -fPIC)
endif()

target_include_directories(${LIB_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(${LIB_NAME} PRIVATE
        SEAL::seal
        Threads::Threads
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()