cmake_minimum_required(VERSION 3.21)
project(emu6502 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Common.cmake)
include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

# add dependencies
include(cmake/CPM.cmake)

CPMAddPackage(NAME Klaus6502
 URL "https://github.com/Klaus2m5/6502_65C02_functional_tests/archive/refs/heads/master.zip"
 DOWNLOAD_ONLY
 )

message(STATUS "Downloaded 6502 functional tests: ${Klaus6502_SOURCE_DIR}")

add_subdirectory(util)
add_subdirectory(libs/common)
add_subdirectory(Mos6502)
