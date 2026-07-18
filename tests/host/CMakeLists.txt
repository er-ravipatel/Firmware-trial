cmake_minimum_required(VERSION 3.16)
project(lumen_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

# Pure-logic firmware modules are shared between the host tests and the device build.
set(FW_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/src)
include_directories(${FW_SRC})

add_executable(host_tests
    test_main.cpp
    test_sha256.cpp
    test_ringbuffer.cpp
    test_eventbus.cpp
    ${FW_SRC}/util/Sha256.cpp
)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
