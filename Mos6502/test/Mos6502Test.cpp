#include "Mos6502/Mos6502.h"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte
#include <cstdint>  // for std::uint8_t
#include <utility>  // for std::pair

#include "util/hex.h"

using Address = Mos6502::Address;

TEST_CASE("Mos6502: ADC Immediate", "[cpu][adc]")
{
  Mos6502 cpu;

  // Setup
  cpu.reset();
  cpu.set_a(0x10);  // A = 0x10
  cpu.set_status(0);  // Make sure Carry is cleared (C = 0)

  // Write instruction to memory
  cpu.write_memory(Address{0x1000}, R"(
    69 22  ; ADC #$22
  )"_hex);

  cpu.set_pc(Address{0x1000});  // Set program counter to start of instruction

  // Execute (it takes 3 cycles for ADC immediate)
  cpu.step();
  cpu.step();
  cpu.step();

  // Verify result: 0x10 + 0x22 + 0 = 0x32
  CHECK(cpu.a() == 0x32);
  // CHECK_FALSE(cpu.status() & CarryFlag);
  // CHECK_FALSE(cpu.status() & OverflowFlag);
  // CHECK_FALSE(cpu.status() & ZeroFlag);
  // CHECK_FALSE(cpu.status() & NegativeFlag);
}