cmake_minimum_required(VERSION 3.20)
project(sdr_recorder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SDR_RECORDER_BUILD_TESTS "Build unit tests" ON)

add_library(sdr_core
    src/config.cpp
    src/demodulator.cpp
    src/session_recorder.cpp
)
target_include_directories(sdr_core PUBLIC include)
target_compile_options(sdr_core PRIVATE -Wall -Wextra -Wpedantic)

find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h PATHS /usr/local/include)
find_library(RTLSDR_LIBRARY rtlsdr PATHS /usr/local/lib)
find_library(MP3LAME_LIBRARY NAMES mp3lame libmp3lame.so.0
             PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)

if(NOT RTLSDR_INCLUDE_DIR OR NOT RTLSDR_LIBRARY)
    message(FATAL_ERROR "librtlsdr development files were not found")
endif()
if(NOT MP3LAME_LIBRARY)
    message(FATAL_ERROR "libmp3lame was not found")
endif()

add_executable(sdr-recorder
    src/main.cpp
    src/mp3_sink.cpp
    src/rtl_sdr_source.cpp
)
target_include_directories(sdr-recorder PRIVATE include "${RTLSDR_INCLUDE_DIR}")
target_link_libraries(sdr-recorder PRIVATE sdr_core "${RTLSDR_LIBRARY}" "${MP3LAME_LIBRARY}")
target_compile_options(sdr-recorder PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS sdr-recorder RUNTIME DESTINATION bin)

if(SDR_RECORDER_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(sdr_core_tests
        src/mp3_sink.cpp
        tests/config_test.cpp
        tests/demodulator_test.cpp
        tests/mp3_sink_test.cpp
        tests/session_recorder_test.cpp
    )
    target_link_libraries(sdr_core_tests PRIVATE sdr_core GTest::gtest_main
                                                "${MP3LAME_LIBRARY}")
    target_compile_options(sdr_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    include(GoogleTest)
    gtest_discover_tests(sdr_core_tests)
endif()
