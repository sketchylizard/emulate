cmake_minimum_required(VERSION 3.21)
project(emu6502 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CMAKE_COMPILE_WARNING_AS_ERROR
  "Treat all compiler warnings as errors" ON)

include(cmake/Common.cmake)

add_subdirectory(util)
add_subdirectory(libs/common)
add_subdirectory(Mos6502)
