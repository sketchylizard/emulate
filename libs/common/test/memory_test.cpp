#include <catch2/catch_test_macros.hpp>
#include <format>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/memory.h"

using namespace Common;

TEST_CASE("Memory addressing")
{
  CHECK(true);
}

TEST_CASE("Array backed memory device", "[memory]")
{
  std::array<Byte, 0x100> mem = {};
  MemoryDevice device(mem, 0x2000_addr);
  CHECK(sizeof(device) == sizeof(&mem) + sizeof(size_t));  // Should be just a pointer and an address

  std::vector<Byte> mem1;
  mem1.resize(0x100);
  MemoryDevice<decltype(mem1)> device1(mem1, 0x2000_addr);
  CHECK(sizeof(device1) == sizeof(&mem1) + sizeof(size_t));  // Should be just a pointer and an address
}

// We should write some tests for LoadFile
