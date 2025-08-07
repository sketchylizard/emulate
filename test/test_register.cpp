
#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte

#include "cpu/register.h"

// Dummy tags
struct A_reg
{
};
struct X_reg
{
};
struct Y_reg
{
};

TEST_CASE("Basic Register tests", "[register]")
{
  // Register initializes to zero
  Register<A_reg> areg;
  REQUIRE(areg.read() == std::byte{0x00});

  // Register stores written value
  Register<X_reg> xreg;
  xreg.write(std::byte{0xAB});
  REQUIRE(xreg.read() == std::byte{0xAB});

  // Registers of different tags are distinct types
  areg.write(std::byte{0x01});
  xreg.write(std::byte{0xFF});

  CHECK(areg.read() == std::byte{0x01});
  CHECK(xreg.read() == std::byte{0xFF});

  // Resetting a register sets it back to zero
  using YRegister = Register<Y_reg, std::byte{0x10}>;
  YRegister yreg;  // Custom reset value
  CHECK(yreg.read() == std::byte{0x10});
  yreg.write(std::byte{0x20});
  CHECK(yreg.read() == std::byte{0x20});

  // Reset by re-initializing
  yreg = YRegister{};
  CHECK(yreg.read() == std::byte{0x10});
}
