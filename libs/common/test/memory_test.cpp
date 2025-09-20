#include <catch2/catch_test_macros.hpp>
#include <format>

#include "common/address.h"
#include "common/memory.h"

using namespace Common;

TEST_CASE("Memory addressing")
{
  CHECK(true);
}

TEST_CASE("Array backed memory device", "[memory]")
{
  std::array<Byte, 256> ram{};

  ram[0] = 0xAA;
  ram[1] = 0xBB;
  ram[2] = 0xCC;
  ram[255] = 0xFF;

  MemoryDevice mem(ram, Address{0x2000});

  REQUIRE(mem.size() == 256);
  REQUIRE(mem.startAddress() == Address{0x2000});
  REQUIRE(mem.endAddress() == Address{0x20FF});
  CHECK(mem.read(Address{0x2000}) == 0xAA);
  CHECK(mem.read(Address{0x2001}) == 0xBB);
  CHECK(mem.read(Address{0x2002}) == 0xCC);
  CHECK(mem.read(Address{0x20FF}) == 0xFF);

  mem.write(Address{0x2001}, 0xDD);
  CHECK(mem.read(Address{0x2001}) == 0xDD);
}

TEST_CASE("C-Style array backed memory device", "[memory]")
{
  Byte ram[256]{};

  ram[0] = 0xAA;
  ram[1] = 0xBB;
  ram[2] = 0xCC;
  ram[255] = 0xFF;

  MemoryDevice mem(ram, Address{0x2000});

  REQUIRE(mem.size() == 256);
  REQUIRE(mem.startAddress() == Address{0x2000});
  REQUIRE(mem.endAddress() == Address{0x20FF});
  CHECK(mem.read(Address{0x2000}) == 0xAA);
  CHECK(mem.read(Address{0x2001}) == 0xBB);
  CHECK(mem.read(Address{0x2002}) == 0xCC);
  CHECK(mem.read(Address{0x20FF}) == 0xFF);

  mem.write(Address{0x2001}, 0xDD);
  CHECK(mem.read(Address{0x2001}) == 0xDD);
}

TEST_CASE("Vector backed memory device", "[memory]")
{
  std::vector<Byte> ram(256);

  ram[0] = 0xAA;
  ram[1] = 0xBB;
  ram[2] = 0xCC;
  ram[255] = 0xFF;

  MemoryDevice mem(ram, Address{0x2000});

  REQUIRE(mem.size() == 256);
  REQUIRE(mem.startAddress() == Address{0x2000});
  REQUIRE(mem.endAddress() == Address{0x20FF});
  CHECK(mem.read(Address{0x2000}) == 0xAA);
  CHECK(mem.read(Address{0x2001}) == 0xBB);
  CHECK(mem.read(Address{0x2002}) == 0xCC);
  CHECK(mem.read(Address{0x20FF}) == 0xFF);

  mem.write(Address{0x2001}, 0xDD);
  CHECK(mem.read(Address{0x2001}) == 0xDD);
}

TEST_CASE("Const span is not writable", "[memory]")
{
  std::array<Byte, 256> ram{};

  ram[0] = 0xAA;
  ram[1] = 0xBB;
  ram[2] = 0xCC;
  ram[255] = 0xFF;

  MemoryDevice mem(std::span<const Byte>(ram), Address{0x2000});

  REQUIRE(mem.size() == 256);
  REQUIRE(mem.startAddress() == Address{0x2000});
  REQUIRE(mem.endAddress() == Address{0x20FF});
  CHECK(mem.read(Address{0x2000}) == 0xAA);
  CHECK(mem.read(Address{0x2001}) == 0xBB);
  CHECK(mem.read(Address{0x2002}) == 0xCC);
  CHECK(mem.read(Address{0x20FF}) == 0xFF);

  // This write should be a no-op
  mem.write(Address{0x2001}, 0xDD);
  CHECK(mem.read(Address{0x2001}) == 0xBB);
}

// We should write some tests for LoadFile
