#include <catch2/catch_test_macros.hpp>

#include "common/bank_switcher.h"
#include "common/memory.h"

using namespace Common;

TEST_CASE("BankSwitcher basic functionality", "[bank_switcher]")
{
  std::array<Byte, 0x2000> bank1{};
  std::array<Byte, 0x2000> bank2{};
  std::array<Byte, 0x2000> bank3{};

  for (size_t i = 0; i < bank1.size(); ++i)
  {
    bank1[i] = 0x10;
    bank2[i] = 0x20;
    bank3[i] = 0x30;
  }

  BankSwitcher switcher{
      std::span<Byte>{bank1},  // Bank 0
      std::span<const Byte>{bank2},  // Bank 1
      std::span<Byte>{bank3}  // Bank 2
  };

  // Initially mapped to bank 0
  REQUIRE(switcher.read(Address{0x0000}) == 0x10);
  REQUIRE(switcher.read(Address{0x1FFF}) == 0x10);

  // Switch to bank 1
  switcher.selectBank(1);
  REQUIRE(switcher.read(Address{0x0000}) == 0x20);
  REQUIRE(switcher.read(Address{0x1FFF}) == 0x20);

  // Switch to bank 2
  switcher.selectBank(2);
  REQUIRE(switcher.read(Address{0x0000}) == 0x30);
  REQUIRE(switcher.read(Address{0x1FFF}) == 0x30);

  // Switch back to bank 0
  switcher.selectBank(0);
  REQUIRE(switcher.read(Address{0x0000}) == 0x10);
  REQUIRE(switcher.read(Address{0x1FFF}) == 0x10);

  // Out of range address should throw
  REQUIRE_THROWS_AS(switcher.read(Address{0x7FFF}), std::out_of_range);
  REQUIRE_THROWS_AS(switcher.read(Address{0xA000}), std::out_of_range);
}
