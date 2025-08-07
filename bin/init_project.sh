#!/bin/bash

set -e

PROJECT_NAME="6502-emulator"
mkdir -p $PROJECT_NAME/{src/cpu,src/util,test,cmake}

cd $PROJECT_NAME

#------------------------------
# conanfile.txt
#------------------------------
cat > conanfile.txt <<EOF
[requires]
catch2/3.5.4

[generators]
CMakeDeps
CMakeToolchain
EOF

#------------------------------
# CMake Common Config (Warnings)
#------------------------------
cat > cmake/Common.cmake <<'EOF'
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
EOF

#------------------------------
# Top-level CMakeLists.txt
#------------------------------
cat > CMakeLists.txt <<EOF
cmake_minimum_required(VERSION 3.21)
project(emu6502 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Common.cmake)
include(\${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)
EOF

#------------------------------
# src/CMakeLists.txt
#------------------------------
cat > src/CMakeLists.txt <<EOF
add_library(cpu6502 STATIC)

target_sources(cpu6502
  PRIVATE
    cpu/cpu.cpp
  PUBLIC
    cpu/cpu.hpp
    cpu/alu.hpp
    cpu/register.hpp
    cpu/flags.hpp
    util/bit_ops.hpp
    util/trace.hpp
)

target_include_directories(cpu6502 PUBLIC \${CMAKE_CURRENT_SOURCE_DIR})
EOF

#------------------------------
# test/CMakeLists.txt
#------------------------------
cat > test/CMakeLists.txt <<EOF
find_package(Catch2 REQUIRED)

add_executable(unit_tests
  test_main.cpp
  test_register.cpp
  test_alu.cpp
)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain cpu6502)

include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
EOF

#------------------------------
# src/cpu/register.hpp
#------------------------------
cat > src/cpu/register.hpp <<'EOF'
#pragma once
#include <cstdint>

namespace emu6502 {

template<typename Tag>
struct Register {
  uint8_t value = 0;

  constexpr void write(uint8_t v) { value = v; }
  constexpr uint8_t read() const { return value; }
  constexpr void reset() { value = 0; }
};

struct A_reg {};
struct X_reg {};
struct Y_reg {};

} // namespace emu6502
EOF

#------------------------------
# src/cpu/alu.hpp
#------------------------------
cat > src/cpu/alu.hpp <<'EOF'
#pragma once
#include <cstdint>
#include <bit>

namespace emu6502 {

enum class ALUOp { AND, ORA, EOR };

constexpr uint8_t alu(ALUOp op, uint8_t a, uint8_t b) {
  switch (op) {
    case ALUOp::AND: return a & b;
    case ALUOp::ORA: return a | b;
    case ALUOp::EOR: return a ^ b;
  }
  return 0;
}

} // namespace emu6502
EOF

#------------------------------
# src/cpu/flags.hpp
#------------------------------
cat > src/cpu/flags.hpp <<'EOF'
#pragma once
#include <cstdint>

namespace emu6502 {

enum ArithmeticFlags : uint8_t {
  Carry = 0x01,
  Zero = 0x02,
  InterruptDisable = 0x04,
  Decimal = 0x08,
  Break = 0x10,
  Unused = 0x20,
  Overflow = 0x40,
  Negative = 0x80
};

struct Flags {
  uint8_t P = Unused;

  constexpr void set(ArithmeticFlags flag) { P |= flag; }
  constexpr void clear(ArithmeticFlags flag) { P &= ~flag; }
  constexpr bool is_set(ArithmeticFlags flag) const { return P & flag; }

  constexpr void update_nz(uint8_t val) {
    if (val == 0) set(Zero); else clear(Zero);
    if (val & 0x80) set(Negative); else clear(Negative);
  }
};

} // namespace emu6502
EOF

#------------------------------
# src/cpu/cpu.cpp
#------------------------------
cat > src/cpu/cpu.cpp <<'EOF'
#include "cpu.hpp"

namespace emu6502 {
// Future CPU implementation goes here.
}
EOF

#------------------------------
# src/cpu/cpu.hpp
#------------------------------
cat > src/cpu/cpu.hpp <<'EOF'
#pragma once

namespace emu6502 {

class CPU {
public:
  void tick();
};

} // namespace emu6502
EOF

#------------------------------
# src/util/bit_ops.hpp
#------------------------------
cat > src/util/bit_ops.hpp <<'EOF'
#pragma once
#include <cstdint>

namespace emu6502 {

constexpr bool bit(uint8_t val, int n) {
  return (val >> n) & 1;
}

} // namespace emu6502
EOF

#------------------------------
# src/util/trace.hpp
#------------------------------
cat > src/util/trace.hpp <<'EOF'
#pragma once
#include <cstdio>
#include <cstdint>

namespace emu6502 {

inline void trace(const char* fmt, ...) {
#ifdef ENABLE_TRACE
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
#endif
}

} // namespace emu6502
EOF

#------------------------------
# test/test_main.cpp
#------------------------------
cat > test/test_main.cpp <<'EOF'
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
EOF

#------------------------------
# test/test_register.cpp
#------------------------------
cat > test/test_register.cpp <<'EOF'
#include <catch2/catch_test_macros.hpp>
#include "cpu/register.hpp"

TEST_CASE("Register basic read/write") {
  emu6502::Register<emu6502::A_reg> reg;
  reg.write(0x42);
  REQUIRE(reg.read() == 0x42);
}
EOF

#------------------------------
# test/test_alu.cpp
#------------------------------
cat > test/test_alu.cpp <<'EOF'
#include <catch2/catch_test_macros.hpp>
#include "cpu/alu.hpp"

TEST_CASE("ALU basic logic ops") {
  using namespace emu6502;

  REQUIRE(alu(ALUOp::AND, 0xF0, 0x0F) == 0x00);
  REQUIRE(alu(ALUOp::ORA, 0xF0, 0x0F) == 0xFF);
  REQUIRE(alu(ALUOp::EOR, 0xAA, 0xFF) == 0x55);
}
EOF

echo "✅ Project initialized in ./$PROJECT_NAME"
