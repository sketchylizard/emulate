#include "Mos6502/Mos6502.h"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte
#include <cstdint>  // for std::uint8_t
#include <utility>  // for std::pair

#include "Mos6502/Bus.h"
#include "Mos6502/Memory.h"
#include "util/hex.h"

TEST_CASE("Mos6502: ADC Immediate", "[cpu][adc]")
{
  // Setup
  Memory<Byte, Address{0x0000}, Address{0xFFFF}> memory;  // 64KB memory

  // Write instruction to memory
  memory.Load(Address{0x1000}, R"(
    69 22  ; ADC #$22
  )"_hex);

  Bus bus = {};

  Mos6502 cpu;
  cpu.set_a(0x10);  // A = 0x10
  cpu.set_status(0);  // Make sure Carry is cleared (C = 0)

  auto tick = [&](Bus bus)
  {
    bus = cpu.Tick(bus);
    bus = memory.Tick(bus);
    return bus;
  };

  auto programStart = Address{0x1000};

  // Set the reset vector to 0x1000
  memory[Mos6502::c_brkVector] = LoByte(programStart);
  memory[Mos6502::c_brkVector + 1] = HiByte(programStart);

  // Execute (it takes 7 cycles for the reset + 2 for ADC immediate)
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  // Reset should be done
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  // ADC should be done

  // Verify result: 0x10 + 0x22 + 0 = 0x32
  CHECK(cpu.a() == 0x32);
  // CHECK_FALSE(cpu.status() & CarryFlag);
  // CHECK_FALSE(cpu.status() & OverflowFlag);
  // CHECK_FALSE(cpu.status() & ZeroFlag);
  // CHECK_FALSE(cpu.status() & NegativeFlag);
}