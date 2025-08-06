# Compiler-specific warning flags and settings

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  message(STATUS "Using Clang warning flags")
  add_compile_options(
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wold-style-cast -Wcast-align -Wundef -Wdouble-promotion -Werror
  )

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(STATUS "Using GCC warning flags")
  add_compile_options(
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wcast-align -Wnull-dereference -Wdouble-promotion -Wformat=2 -Werror
  )

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(STATUS "Using MSVC warning flags")
  add_compile_options(/W4 /WX)
endif()
