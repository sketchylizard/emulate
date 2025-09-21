cmake_minimum_required(VERSION 3.21)
project(emu6502 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest) # enables BUILD_TESTING option
include(cmake/Common.cmake)

option(BUILD_SLOW_TESTS "Enable slow suites (Klaus, fuzz)" ON)

option(CMAKE_COMPILE_WARNING_AS_ERROR
  "Treat all compiler warnings as errors" ON)

add_subdirectory(libs/common)
add_subdirectory(libs/cpu6502)
add_subdirectory(libs/apple2)

# if(BUILD_TESTING)
add_subdirectory(tests) # integration tests

# endif()
