#include <catch2/catch_test_macros.hpp>
#include <format>

#include "common/address.h"

using namespace Common;

TEST_CASE("Address type operations", "[address]")
{
  // Test to exercise operator overloading for the Address type
  Address addr1 = 0x1000_addr;
  Address addr2 = 0x2000_addr;

  CHECK(static_cast<uint16_t>(addr1) == 0x1000);
  CHECK(static_cast<uint16_t>(addr2) == 0x2000);

  Address addr3 = addr1 + 0x1234;
  CHECK(addr3 == 0x2234_addr);

  Address addr4 = addr2 + 0xFFFF;
  CHECK(addr4 == 0x1FFF_addr);

  CHECK(addr1 == addr1);
  CHECK(addr1 != addr2);
  CHECK(addr1 < addr2);
  CHECK(addr2 > addr1);
  CHECK(addr1 <= addr1);
  CHECK(addr1 <= addr2);
  CHECK(addr2 >= addr1);
  CHECK(addr2 >= addr2);

  addr1 += 0x10;
  CHECK(addr1 == 0x1010_addr);

  addr1 += 0xFFFF;
  CHECK(addr1 == 0x100F_addr);

  addr1 -= 0x20;
  CHECK(addr1 == 0xfef_addr);

  addr1 -= 0xFFFF;  // This will wrap around, it is essentially subtracting -1/adding 1
  CHECK(addr1 == 0x0FF0_addr);

  ++addr1;
  CHECK(addr1 == 0x0FF1_addr);

  --addr1;
  CHECK(addr1 == 0x0FF0_addr);

  auto addr5 = addr1++;
  CHECK(addr5 == 0x0FF0_addr);
  CHECK(addr1 == 0x0FF1_addr);

  auto addr6 = addr1--;
  CHECK(addr6 == 0x0FF1_addr);
  CHECK(addr1 == 0x0FF0_addr);

  CHECK(LoByte(addr1) == 0xF0);
  CHECK(HiByte(addr1) == 0x0F);

  addr1 = 0xFFFF_addr;
  ++addr1;
  CHECK(addr1 == 0x0000_addr);
  --addr1;
  CHECK(addr1 == 0xFFFF_addr);

  addr1 = MakeAddress(0x34, 0x12);
  CHECK(addr1 == 0x1234_addr);

  auto str = std::format("Address is ${:04X}", addr1);
  CHECK(str == "Address is $1234");
}
