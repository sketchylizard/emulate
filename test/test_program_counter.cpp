#include <catch2/catch_test_macros.hpp>

#include "cpu/program_counter.h"
#include <cstddef>  // for std::byte

TEST_CASE("ProgramCounter: default construction")
{
  ProgramCounter pc;

  CHECK(pc.read() == 0x0000);
  CHECK(pc.lo() == std::byte{0x00});
  CHECK(pc.hi() == std::byte{0x00});
}

TEST_CASE("ProgramCounter: write and read")
{
  ProgramCounter pc;
  pc.write(0x1234);

  CHECK(pc.read() == 0x1234);
  CHECK(pc.lo() == std::byte{0x34});
  CHECK(pc.hi() == std::byte{0x12});
}

TEST_CASE("ProgramCounter: set high and low bytes individually")
{
  ProgramCounter pc;

  pc.write(0x0000);
  pc.set_lo(std::byte{0xCD});
  CHECK(pc.read() == 0x00CD);

  pc.set_hi(std::byte{0xAB});
  CHECK(pc.read() == 0xABCD);

  // Round trip
  CHECK(pc.lo() == std::byte{0xCD});
  CHECK(pc.hi() == std::byte{0xAB});
}

TEST_CASE("ProgramCounter: increment")
{
  ProgramCounter pc;
  pc.write(0x0000);

  ++pc;
  CHECK(pc.read() == 0x0001);

  pc += 5;
  CHECK(pc.read() == 0x0006);

  pc.write(0xFFFE);
  ++pc;
  CHECK(pc.read() == 0xFFFF);

  // Test wrap around
  ++pc;
  CHECK(pc.read() == 0x0000);

  pc += 0x10;
  CHECK(pc.read() == 0x0010);

// Wrap around
  pc += 0xFFFE;
  CHECK(pc.read() == 0x000E);
}

