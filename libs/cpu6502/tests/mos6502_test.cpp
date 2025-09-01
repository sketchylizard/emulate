#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/bus.h"
#include "common/microcode.h"
#include "common/microcode_pump.h"
#include "cpu6502/mos6502.h"

using namespace cpu6502;
using namespace Common;

namespace Common
{
// In common/test_helpers.h or wherever you put the test utilities
// Helper function for readable output
std::ostream& operator<<(std::ostream& os, const BusRequest& value)
{
  if (value.isSync())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isRead())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isWrite())
  {
    os << std::format("Bus write(${:04x}, ${:02x})", value.address, value.data);
  }
  os << "NONE";
  return os;
}
}  // namespace Common

TEST_CASE("LDA Immediate")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xA9});  // Opcode for LDA Immediate
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x42});  // Operand for LDA
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch
  CHECK(cpu.a == 0x42);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 3);  // Three microcode operations executed
}

TEST_CASE("LDA Zero Page")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xA5});  // Opcode for LDA Zero Page
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Zero page address
  CHECK(request == BusRequest::Read(0x80_addr));

  request = pump.tick(cpu, BusResponse{0x33});  // Data at $0080
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK(cpu.a == 0x33);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 4);  // Four microcode operations executed
}

TEST_CASE("LDA Zero Page,X")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x05;  // Set X register

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB5});  // Opcode for LDA Zero Page,X
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Zero page base address
  CHECK(request == BusRequest::Read(0x85_addr));  // $80 + X($05) = $85, stays in zero page

  request = pump.tick(cpu, BusResponse{0x77});  // Data at $0085
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK(cpu.a == 0x77);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 4);  // Four microcode operations executed
}

TEST_CASE("LDA Zero Page,X wraparound")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x90;  // Set X register to cause wraparound

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB5});  // Opcode for LDA Zero Page,X
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Zero page base address
  CHECK(request == BusRequest::Read(0x10_addr));  // $80 + X($90) = $110, wraps to $10

  request = pump.tick(cpu, BusResponse{0x88});  // Data at $0010
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK(cpu.a == 0x88);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 4);  // Four microcode operations executed
}

TEST_CASE("LDA Absolute")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xAD});  // Opcode for LDA Absolute
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x34});  // Low byte of address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x12});  // High byte of address
  CHECK(request == BusRequest::Read(0x1234_addr));  // Effective address

  request = pump.tick(cpu, BusResponse{0x99});  // Data at $1234
  CHECK(request == BusRequest::Fetch(3_addr));  // Next fetch

  CHECK(cpu.a == 0x99);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 5);  // Five microcode operations executed
}

TEST_CASE("LDA Absolute,X (no page crossing)")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x10;  // Set X register

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBD});  // Opcode for LDA Absolute,X
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x30});  // High byte of base address
  CHECK(request == BusRequest::Read(0x3030_addr));  // $3020 + X($10) = $3030, no page cross

  request = pump.tick(cpu, BusResponse{0x00});  // Data at $3030
  CHECK(request == BusRequest::Fetch(3_addr));  // Next fetch

  CHECK(cpu.a == 0x00);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 5);  // Five microcode operations executed
}

TEST_CASE("LDA Absolute,X (with page crossing)")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x10;  // Set X register

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBD});  // Opcode for LDA Absolute,X
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // High byte of base address
  // Should read wrong page first: $20F0 + X($10) = $2100, but initially reads $20xx page
  CHECK(request == BusRequest::Read(0x2000_addr));  // Wrong page read

  request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from wrong page
  CHECK(request == BusRequest::Read(0x2100_addr));  // Correct page read after fixup

  request = pump.tick(cpu, BusResponse{0xAA});  // Data at $2100
  CHECK(request == BusRequest::Fetch(3_addr));  // Next fetch

  CHECK(cpu.a == 0xAA);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 6);  // Six microcode operations executed (extra cycle for page cross)
}

TEST_CASE("LDA Absolute,Y (no page crossing)")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x08;  // Set Y register

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB9});  // Opcode for LDA Absolute,Y
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x40});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x50});  // High byte of base address
  CHECK(request == BusRequest::Read(0x5048_addr));  // $5040 + Y($08) = $5048, no page cross

  request = pump.tick(cpu, BusResponse{0x66});  // Data at $5048
  CHECK(request == BusRequest::Fetch(3_addr));  // Next fetch

  CHECK(cpu.a == 0x66);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 5);  // Five microcode operations executed
}

TEST_CASE("LDA flag behavior with zero result")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xA9});  // Opcode for LDA Immediate
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x00});  // Zero operand
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK(cpu.a == 0x00);  // A register should be loaded with zero
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 3);  // Three microcode operations executed
}

TEST_CASE("LDA flag behavior with negative result")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xA9});  // Opcode for LDA Immediate
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Negative operand (bit 7 set)
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK(cpu.a == 0x80);  // A register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 3);  // Three microcode operations executed
}

TEST_CASE("NOP", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;

  // Set initial state to verify nothing changes
  cpu.a = 0x42;
  cpu.x = 0x33;
  cpu.y = 0x55;
  cpu.sp = 0xFE;
  cpu.p = 0xA5;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xEA});  // Opcode for NOP
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  // Verify nothing changed
  CHECK(cpu.a == 0x42);
  CHECK(cpu.x == 0x33);
  CHECK(cpu.y == 0x55);
  CHECK(cpu.sp == 0xFE);
  CHECK(cpu.p == 0xA5);
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("CLC - Clear Carry Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Carry, true);  // Set carry flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x18});  // Opcode for CLC
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Carry) == false);  // Carry flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("SEC - Set Carry Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Carry, false);  // Clear carry flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x38});  // Opcode for SEC
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Carry) == true);  // Carry flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("CLI - Clear Interrupt Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Interrupt, true);  // Set interrupt flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x58});  // Opcode for CLI
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Interrupt) == false);  // Interrupt flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("SEI - Set Interrupt Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Interrupt, false);  // Clear interrupt flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x78});  // Opcode for SEI
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Interrupt) == true);  // Interrupt flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("CLV - Clear Overflow Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Overflow, true);  // Set overflow flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB8});  // Opcode for CLV
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Overflow) == false);  // Overflow flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("CLD - Clear Decimal Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Decimal, true);  // Set decimal flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xD8});  // Opcode for CLD
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Decimal) == false);  // Decimal flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("SED - Set Decimal Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Decimal, false);  // Clear decimal flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xF8});  // Opcode for SED
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Decimal) == true);  // Decimal flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("INX - Increment X Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x7F;  // Set X to positive value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xE8});  // Opcode for INX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x80);  // X should be incremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set (0x80 has bit 7 set)
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("INX - Zero Result", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0xFF;  // Set X to wrap around

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xE8});  // Opcode for INX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should wrap to zero
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("INY - Increment Y Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x42;  // Set Y to test value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xC8});  // Opcode for INY
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x43);  // Y should be incremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("DEX - Decrement X Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x01;  // Set X to decrement to zero

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xCA});  // Opcode for DEX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should be decremented to zero
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("DEX - Underflow", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x00;  // Set X to underflow

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xCA});  // Opcode for DEX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0xFF);  // X should underflow to 0xFF
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("DEY - Decrement Y Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x80;  // Set Y to negative value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x88});  // Opcode for DEY
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x7F);  // Y should be decremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TAX - Transfer A to X", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.a = 0x99;  // Set A to test value
  cpu.x = 0x00;  // Clear X initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xAA});  // Opcode for TAX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x99);  // X should equal A
  CHECK(cpu.a == 0x99);  // A should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TAY - Transfer A to Y", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.a = 0x00;  // Set A to zero
  cpu.y = 0xFF;  // Set Y to different value initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xA8});  // Opcode for TAY
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x00);  // Y should equal A
  CHECK(cpu.a == 0x00);  // A should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TXA - Transfer X to A", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x42;  // Set X to test value
  cpu.a = 0x00;  // Clear A initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x8A});  // Opcode for TXA
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.a == 0x42);  // A should equal X
  CHECK(cpu.x == 0x42);  // X should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TYA - Transfer Y to A", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x80;  // Set Y to negative value
  cpu.a = 0x00;  // Clear A initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x98});  // Opcode for TYA
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.a == 0x80);  // A should equal Y
  CHECK(cpu.y == 0x80);  // Y should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TXS - Transfer X to Stack Pointer", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0xFE;  // Set X to test value
  cpu.sp = 0x00;  // Clear SP initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x9A});  // Opcode for TXS
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.sp == 0xFE);  // SP should equal X
  CHECK(cpu.x == 0xFE);  // X should be unchanged
  // TXS does NOT affect flags
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("TSX - Transfer Stack Pointer to X", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.sp = 0x00;  // Set SP to zero
  cpu.x = 0xFF;  // Set X to different value initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBA});  // Opcode for TSX
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should equal SP
  CHECK(cpu.sp == 0x00);  // SP should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 2);  // Two microcode operations executed
}

TEST_CASE("BEQ - Branch taken (Zero flag set)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x1000_addr;
  cpu.set(State::Flag::Zero, true);  // Set zero flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x1000_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Opcode for BEQ
  CHECK(request == BusRequest::Read(0x1001_addr));

  request = pump.tick(cpu, BusResponse{0x05});  // Branch offset +5
  CHECK(request == BusRequest::Fetch(0x1007_addr));  // PC(1002) + offset(5) = 1007

  CHECK(cpu.pc == 0x1008_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken, same page
}

TEST_CASE("BEQ - Branch not taken (Zero flag clear)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x1000_addr;
  cpu.set(State::Flag::Zero, false);  // Clear zero flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x1000_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Opcode for BEQ
  CHECK(request == BusRequest::Fetch(0x1002_addr));  // PC should advance normally

  CHECK(cpu.pc == 0x1003_addr);  // PC should be after branch instruction
  CHECK(pump.microcodeCount() == 2);  // Only TWO cycles for branch not taken
}

TEST_CASE("BNE - Branch taken (Zero flag clear)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x2000_addr;
  cpu.set(State::Flag::Zero, false);  // Clear zero flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x2000_addr));

  request = pump.tick(cpu, BusResponse{0xD0});  // Opcode for BNE
  CHECK(request == BusRequest::Read(0x2001_addr));

  // Branch to self is caught by trap detection
  CHECK_THROWS(pump.tick(cpu, BusResponse{0xFE}));
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken, same page
}

TEST_CASE("BNE - Branch not taken (Zero flag set)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x2000_addr;
  cpu.set(State::Flag::Zero, true);  // Set zero flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x2000_addr));

  request = pump.tick(cpu, BusResponse{0xD0});  // Opcode for BNE
  // branch not taken, so PC should advance normally
  CHECK(request == BusRequest::Fetch(0x2002_addr));

  CHECK(cpu.pc == 0x2003_addr);  // PC should be after branch instruction
  CHECK(pump.microcodeCount() == 2);  // Only TWO cycles for branch not taken
}

TEST_CASE("BPL - Branch taken (Negative flag clear)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x3000_addr;
  cpu.set(State::Flag::Negative, false);  // Clear negative flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x3000_addr));

  request = pump.tick(cpu, BusResponse{0x10});  // Opcode for BPL
  CHECK(request == BusRequest::Read(0x3001_addr));

  request = pump.tick(cpu, BusResponse{0x10});  // Branch offset +16
  CHECK(request == BusRequest::Fetch(0x3012_addr));  // PC(3002) + offset(16) = 3012

  CHECK(cpu.pc == 0x3013_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken, same page
}

TEST_CASE("BMI - Branch taken (Negative flag set), page crossing", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x4000_addr;
  cpu.set(State::Flag::Negative, true);  // Set negative flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x4000_addr));

  request = pump.tick(cpu, BusResponse{0x30});  // Opcode for BMI
  CHECK(request == BusRequest::Read(0x4001_addr));

  // page was crossed, so the first read is from the wrong page
  request = pump.tick(cpu, BusResponse{0xF0});  // Branch offset -16 (0xF0 = -16 signed)
  // PC(4002) + offset(-16) = 3FF2, but the first read is from wrong page
  CHECK(request == BusRequest::Read(0x40F2_addr));

  request = pump.tick(cpu, BusResponse{0x99});  // Random data, should be ignored
  CHECK(request == BusRequest::Fetch(0x3FF2_addr));  // PC(4002) + offset(-16) = 3FF2

  CHECK(cpu.pc == 0x3FF3_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 4);  // Three cycles for branch taken, cross page
}

TEST_CASE("BCC - Branch taken (Carry flag clear)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x5000_addr;
  cpu.set(State::Flag::Carry, false);  // Clear carry flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x5000_addr));

  request = pump.tick(cpu, BusResponse{0x90});  // Opcode for BCC
  CHECK(request == BusRequest::Read(0x5001_addr));

  request = pump.tick(cpu, BusResponse{0x7F});  // Branch offset +127 (max positive)
  CHECK(request == BusRequest::Fetch(0x5081_addr));  // PC(5002) + offset(127) = 5081

  CHECK(cpu.pc == 0x5082_addr);  // PC should be at branch target + 1 (since we've already fetched)
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken, same page
}

TEST_CASE("BCS - Branch taken (Carry flag set) page crossing", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x6000_addr;
  cpu.set(State::Flag::Carry, true);  // Set carry flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x6000_addr));

  request = pump.tick(cpu, BusResponse{0xB0});  // Opcode for BCS
  CHECK(request == BusRequest::Read(0x6001_addr));

  // page was crossed, so the first read is from the wrong page
  request = pump.tick(cpu, BusResponse{0x80});  // Branch offset -128 (0x80 = -128 signed)
  // PC(6002) + offset(-128) = 5F82, but the first read is from wrong page
  CHECK(request == BusRequest::Read(0x6082_addr));

  request = pump.tick(cpu, BusResponse{0xAA});  // rando data, should be ignored
  CHECK(request == BusRequest::Fetch(0x5F82_addr));  // PC(6002) + offset(-128) = 5F82

  CHECK(cpu.pc == 0x5F83_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 4);  // Three cycles for branch taken, same page
}

TEST_CASE("BVC - Branch taken (Overflow flag clear)", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x7000_addr;
  cpu.set(State::Flag::Overflow, false);  // Clear overflow flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x7000_addr));

  request = pump.tick(cpu, BusResponse{0x50});  // Opcode for BVC
  CHECK(request == BusRequest::Read(0x7001_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // Branch offset +32
  CHECK(request == BusRequest::Fetch(0x7022_addr));  // PC(7002) + offset(32) = 7022

  CHECK(cpu.pc == 0x7023_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken, same page
}

TEST_CASE("BVS - Branch taken (Overflow flag set), page crossing", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x8000_addr;
  cpu.set(State::Flag::Overflow, true);  // Set overflow flag

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x8000_addr));

  request = pump.tick(cpu, BusResponse{0x70});  // Opcode for BVS
  CHECK(request == BusRequest::Read(0x8001_addr));

  request = pump.tick(cpu, BusResponse{0xE0});  // Branch offset -32 (0xE0 = -32 signed), but wrong page
  // PC(8002) + offset(-32) = 7FE2, but the first read is from wrong page
  CHECK(request == BusRequest::Read(0x80E2_addr));

  request = pump.tick(cpu, BusResponse{0x55});  // Random data, should be ignored
  CHECK(request == BusRequest::Fetch(0x7FE2_addr));  // PC(8002) + offset(-32) = 7FE2

  CHECK(cpu.pc == 0x7FE3_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 4);  // Three cycles for branch taken, different page
}

TEST_CASE("Branch with page crossing - forward", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x20F0_addr;  // Set PC near page boundary
  cpu.set(State::Flag::Zero, true);  // Set zero flag for BEQ

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x20F0_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Opcode for BEQ
  CHECK(request == BusRequest::Read(0x20F1_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // Branch offset +32
  // PC after instruction = 20F2, target = 20F2 + 32 = 2112 (crosses page boundary)
  CHECK(request == BusRequest::Read(0x2012_addr));  // Wrong page read first

  request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from page crossing fixup
  CHECK(request == BusRequest::Fetch(0x2112_addr));  // Correct address after fixup

  CHECK(cpu.pc == 0x2113_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 4);  // Four cycles for page crossing branch
}

TEST_CASE("Branch with page crossing - backward", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x2110_addr;  // Set PC in upper part of page
  cpu.set(State::Flag::Carry, false);  // Clear carry flag for BCC

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x2110_addr));

  request = pump.tick(cpu, BusResponse{0x90});  // Opcode for BCC
  CHECK(request == BusRequest::Read(0x2111_addr));

  request = pump.tick(cpu, BusResponse{0xE0});  // Branch offset -32 (0xE0 = -32 signed)
  // PC after instruction = 2112, target = 2112 + (-32) = 20F2 (crosses page boundary)
  CHECK(request == BusRequest::Read(0x21F2_addr));  // Wrong page read first

  request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from page crossing fixup
  CHECK(request == BusRequest::Fetch(0x20F2_addr));  // Correct address after fixup

  CHECK(cpu.pc == 0x20F3_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 4);  // Four cycles for page crossing branch
}

TEST_CASE("Branch forward - no page crossing", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x3080_addr;  // Set PC in middle of page
  cpu.set(State::Flag::Negative, false);  // Clear negative flag for BPL

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x3080_addr));

  request = pump.tick(cpu, BusResponse{0x10});  // Opcode for BPL
  CHECK(request == BusRequest::Read(0x3081_addr));

  request = pump.tick(cpu, BusResponse{0x30});  // Branch offset +48
  // PC after instruction = 3082, target = 3082 + 48 = 30B2 (same page)
  CHECK(request == BusRequest::Fetch(0x30B2_addr));  // Direct to target, no page crossing

  CHECK(cpu.pc == 0x30B3_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 3);  // Three cycles for same page branch
}

TEST_CASE("Branch backward - no page crossing", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x4080_addr;  // Set PC in middle of page
  cpu.set(State::Flag::Overflow, true);  // Set overflow flag for BVS

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x4080_addr));

  request = pump.tick(cpu, BusResponse{0x70});  // Opcode for BVS
  CHECK(request == BusRequest::Read(0x4081_addr));

  request = pump.tick(cpu, BusResponse{0xD0});  // Branch offset -48 (0xD0 = -48 signed)
  // PC after instruction = 4082, target = 4082 + (-48) = 4052 (same page)
  CHECK(request == BusRequest::Fetch(0x4052_addr));  // Direct to target, no page crossing

  CHECK(cpu.pc == 0x4053_addr);  // PC should be at branch target
  CHECK(pump.microcodeCount() == 3);  // Three cycles for same page branch
}

TEST_CASE("Branch zero offset", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x5000_addr;
  cpu.set(State::Flag::Zero, false);  // Clear zero flag for BNE

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x5000_addr));

  request = pump.tick(cpu, BusResponse{0xD0});  // Opcode for BNE
  CHECK(request == BusRequest::Read(0x5001_addr));

  request = pump.tick(cpu, BusResponse{0x00});  // Branch offset 0
  // PC after instruction = 5002, target = 5002 + 0 = 5002 (branch to next instruction)
  CHECK(request == BusRequest::Fetch(0x5002_addr));  // Branch to next instruction

  CHECK(cpu.pc == 0x5003_addr);  // PC should be at branch target (next instruction)
  CHECK(pump.microcodeCount() == 3);  // Three cycles for branch taken
}

TEST_CASE("Self-branch trap detection", "[branch][relative]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.pc = 0x6000_addr;
  cpu.set(State::Flag::Carry, true);  // Set carry flag for BCS

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0x6000_addr));

  request = pump.tick(cpu, BusResponse{0xB0});  // Opcode for BCS
  CHECK(request == BusRequest::Read(0x6001_addr));

  // Branch offset -2 should create infinite loop: PC(6002) + (-2) = 6000
  CHECK_THROWS_AS(pump.tick(cpu, BusResponse{0xFE}), TrapException);
}

// Template traits for LDX and LDY instructions
template<typename T>
struct LoadRegisterTraits;

template<>
struct LoadRegisterTraits<struct LDX_Tag>
{
  static constexpr const char* name = "LDX";
  static constexpr Byte State::* reg = &State::x;
  static constexpr Byte immediate_opcode = 0xA2;
  static constexpr Byte zeropage_opcode = 0xA6;
  static constexpr Byte zeropage_y_opcode = 0xB6;  // LDX uses Y indexing
  static constexpr Byte absolute_opcode = 0xAE;
  static constexpr Byte absolute_y_opcode = 0xBE;  // LDX uses Y indexing
};

template<>
struct LoadRegisterTraits<struct LDY_Tag>
{
  static constexpr const char* name = "LDY";
  static constexpr Byte State::* reg = &State::y;
  static constexpr Byte immediate_opcode = 0xA0;
  static constexpr Byte zeropage_opcode = 0xA4;
  static constexpr Byte zeropage_x_opcode = 0xB4;  // LDY uses X indexing
  static constexpr Byte absolute_opcode = 0xAC;
  static constexpr Byte absolute_x_opcode = 0xBC;  // LDY uses X indexing
};

TEMPLATE_TEST_CASE("Load Register Immediate", "[load][immediate]", LDX_Tag, LDY_Tag)
{
  using Traits = LoadRegisterTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{Traits::immediate_opcode});
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x42});  // Test value
  CHECK(request == BusRequest::Fetch(2_addr));  // Next fetch

  CHECK((cpu.*(Traits::reg)) == 0x42);  // Register should be loaded
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.microcodeCount() == 3);  // Three cycles for immediate mode
}

TEMPLATE_TEST_CASE("Load Register Immediate - Zero Result", "[load][immediate]", LDX_Tag, LDY_Tag)
{
  using Traits = LoadRegisterTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{Traits::immediate_opcode});
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x00});  // Zero value
  CHECK(request == BusRequest::Fetch(2_addr));

  CHECK((cpu.*(Traits::reg)) == 0x00);
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 3);
}

TEMPLATE_TEST_CASE("Load Register Immediate - Negative Result", "[load][immediate]", LDX_Tag, LDY_Tag)
{
  using Traits = LoadRegisterTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{Traits::immediate_opcode});
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Negative value
  CHECK(request == BusRequest::Fetch(2_addr));

  CHECK((cpu.*(Traits::reg)) == 0x80);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.microcodeCount() == 3);
}

TEMPLATE_TEST_CASE("Load Register Zero Page", "[load][zeropage]", LDX_Tag, LDY_Tag)
{
  using Traits = LoadRegisterTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{Traits::zeropage_opcode});
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Zero page address
  CHECK(request == BusRequest::Read(0x80_addr));

  request = pump.tick(cpu, BusResponse{0x99});  // Data at $0080
  CHECK(request == BusRequest::Fetch(2_addr));

  CHECK((cpu.*(Traits::reg)) == 0x99);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == true);
  CHECK(pump.microcodeCount() == 4);
}

// LDX Zero Page,Y test
TEST_CASE("LDX Zero Page,Y", "[load][zeropage][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x05;  // Set Y register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB6});  // LDX Zero Page,Y opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x80});  // Zero page base address
  CHECK(request == BusRequest::Read(0x85_addr));  // $80 + Y($05) = $85

  request = pump.tick(cpu, BusResponse{0x33});  // Data at $0085
  CHECK(request == BusRequest::Fetch(2_addr));

  CHECK(cpu.x == 0x33);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 4);
}

// LDY Zero Page,X test
TEST_CASE("LDY Zero Page,X", "[load][zeropage][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x10;  // Set X register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB4});  // LDY Zero Page,X opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Zero page base address
  CHECK(request == BusRequest::Read(0x00_addr));  // $F0 + X($10) = $100, wraps to $00

  request = pump.tick(cpu, BusResponse{0x77});  // Data at $0000
  CHECK(request == BusRequest::Fetch(2_addr));

  CHECK(cpu.y == 0x77);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 4);
}

TEMPLATE_TEST_CASE("Load Register Absolute", "[load][absolute]", LDX_Tag, LDY_Tag)
{
  using Traits = LoadRegisterTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{Traits::absolute_opcode});
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x34});  // Low byte of address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x12});  // High byte of address
  CHECK(request == BusRequest::Read(0x1234_addr));  // Effective address

  request = pump.tick(cpu, BusResponse{0xAB});  // Data at $1234
  CHECK(request == BusRequest::Fetch(3_addr));

  CHECK((cpu.*(Traits::reg)) == 0xAB);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == true);
  CHECK(pump.microcodeCount() == 5);
}

// LDX Absolute,Y test (no page crossing)
TEST_CASE("LDX Absolute,Y - No Page Crossing", "[load][absolute][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x08;  // Set Y register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBE});  // LDX Absolute,Y opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x30});  // High byte of base address
  CHECK(request == BusRequest::Read(0x3028_addr));  // $3020 + Y($08) = $3028, no page cross

  request = pump.tick(cpu, BusResponse{0x55});  // Data at $3028
  CHECK(request == BusRequest::Fetch(3_addr));

  CHECK(cpu.x == 0x55);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 5);
}

// LDY Absolute,X test (no page crossing)
TEST_CASE("LDY Absolute,X - No Page Crossing", "[load][absolute][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x10;  // Set X register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBC});  // LDY Absolute,X opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0x40});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x50});  // High byte of base address
  CHECK(request == BusRequest::Read(0x5050_addr));  // $5040 + X($10) = $5050, no page cross

  request = pump.tick(cpu, BusResponse{0x22});  // Data at $5050
  CHECK(request == BusRequest::Fetch(3_addr));

  CHECK(cpu.y == 0x22);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 5);
}

// LDX Absolute,Y test (with page crossing)
TEST_CASE("LDX Absolute,Y - With Page Crossing", "[load][absolute][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x10;  // Set Y register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBE});  // LDX Absolute,Y opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0xF8});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x20});  // High byte of base address
  // $20F8 + Y($10) = $2108, crosses page boundary
  CHECK(request == BusRequest::Read(0x2008_addr));  // Wrong page read first

  request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from wrong page
  CHECK(request == BusRequest::Read(0x2108_addr));  // Correct page read after fixup

  request = pump.tick(cpu, BusResponse{0xCC});  // Data at $2108
  CHECK(request == BusRequest::Fetch(3_addr));

  CHECK(cpu.x == 0xCC);
  CHECK(cpu.has(State::Flag::Zero) == false);
  CHECK(cpu.has(State::Flag::Negative) == true);
  CHECK(pump.microcodeCount() == 6);  // Six cycles due to page crossing
}

// LDY Absolute,X test (with page crossing)
TEST_CASE("LDY Absolute,X - With Page Crossing", "[load][absolute][indexed]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x20;  // Set X register for indexing

  auto request = pump.tick(cpu, BusResponse{});
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xBC});  // LDY Absolute,X opcode
  CHECK(request == BusRequest::Read(1_addr));

  request = pump.tick(cpu, BusResponse{0xF0});  // Low byte of base address
  CHECK(request == BusRequest::Read(2_addr));

  request = pump.tick(cpu, BusResponse{0x40});  // High byte of base address
  // $40F0 + X($20) = $4110, crosses page boundary
  CHECK(request == BusRequest::Read(0x4010_addr));  // Wrong page read first

  request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from wrong page
  CHECK(request == BusRequest::Read(0x4110_addr));  // Correct page read after fixup

  request = pump.tick(cpu, BusResponse{0x00});  // Data at $4110
  CHECK(request == BusRequest::Fetch(3_addr));

  CHECK(cpu.y == 0x00);
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);
  CHECK(pump.microcodeCount() == 6);  // Six cycles due to page crossing
}
