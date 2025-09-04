#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/bus.h"
#include "common/memory.h"
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

// Helper function to execute a complete instruction for testing
// Takes the instruction bytes and runs until the next instruction fetch
// Returns the address of the next instruction (address bus at SYNC)
Address executeInstruction(MicrocodePump<mos6502>& pump, State& cpu, MemoryDevice<Byte>& memory,
    std::initializer_list<Byte> instruction, int32_t count = 1)
{
  // Copy instruction bytes to memory at current PC location
  std::size_t pc_addr = static_cast<std::size_t>(cpu.pc);
  auto memory_data = memory.data();  // Get writable access to memory

  std::size_t i = 0;
  for (Byte byte : instruction)
  {
    memory_data[pc_addr + i] = byte;
    ++i;
  }

  BusRequest request;
  BusResponse response;

  // We loop until we see count SYNCs (instruction fetches), but, the first SYNC
  // we see is fetching our instruction's opcode, so we skip that one and
  // only count subsequent SYNCs. Just add one to count to account for that.
  ++count;

  // Execute until we see a SYNC (opcode fetch) which indicates we've
  // completed the instruction and are starting the next one
  std::size_t cycle_count = 0;
  const std::size_t c_maxCycles = 20;  // Safety limit to prevent infinite loops

  do
  {
    request = pump.tick(cpu, response);
    response = memory.tick(request);

    // Check if this is the start of the next instruction (SYNC)
    // We skip the first SYNC because that's fetching our instruction's opcode
    if (request.isSync() && --count <= 0)
    {
      return request.address;  // Return the address of the next instruction
    }

    ++cycle_count;
  } while (cycle_count < c_maxCycles);

  FAIL("Instruction execution exceeded maximum cycles - possible infinite loop");
  return cpu.pc;  // Fallback, should not reach here
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0xDE});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  // Verify nothing changed
  CHECK(cpu.a == 0x42);
  CHECK(cpu.x == 0x33);
  CHECK(cpu.y == 0x55);
  CHECK(cpu.sp == 0xFE);
  CHECK(cpu.p == 0xA5);
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("CLC - Clear Carry Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Carry, true);  // Set carry flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x18});  // Opcode for CLC
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Carry) == false);  // Carry flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("SEC - Set Carry Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Carry, false);  // Clear carry flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x38});  // Opcode for SEC
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Carry) == true);  // Carry flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("CLI - Clear Interrupt Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Interrupt, true);  // Set interrupt flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x58});  // Opcode for CLI
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Interrupt) == false);  // Interrupt flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("SEI - Set Interrupt Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Interrupt, false);  // Clear interrupt flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x78});  // Opcode for SEI
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Interrupt) == true);  // Interrupt flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("CLV - Clear Overflow Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Overflow, true);  // Set overflow flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xB8});  // Opcode for CLV
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Overflow) == false);  // Overflow flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("CLD - Clear Decimal Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Decimal, true);  // Set decimal flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xD8});  // Opcode for CLD
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Decimal) == false);  // Decimal flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("SED - Set Decimal Flag", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.set(State::Flag::Decimal, false);  // Clear decimal flag initially

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xF8});  // Opcode for SED
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.has(State::Flag::Decimal) == true);  // Decimal flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("INX - Increment X Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x7F;  // Set X to positive value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xE8});  // Opcode for INX
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x80);  // X should be incremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set (0x80 has bit 7 set)
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("INX - Zero Result", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0xFF;  // Set X to wrap around

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xE8});  // Opcode for INX
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should wrap to zero
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("INY - Increment Y Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x42;  // Set Y to test value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xC8});  // Opcode for INY
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x43);  // Y should be incremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("DEX - Decrement X Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x01;  // Set X to decrement to zero

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xCA});  // Opcode for DEX
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should be decremented to zero
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("DEX - Underflow", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.x = 0x00;  // Set X to underflow

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0xCA});  // Opcode for DEX
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0xFF);  // X should underflow to 0xFF
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

TEST_CASE("DEY - Decrement Y Register", "[implied]")
{
  MicrocodePump<mos6502> pump;
  State cpu;
  cpu.y = 0x80;  // Set Y to negative value

  auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
  CHECK(request == BusRequest::Fetch(0_addr));

  request = pump.tick(cpu, BusResponse{0x88});  // Opcode for DEY
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x7F);  // Y should be decremented
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x99);  // X should equal A
  CHECK(cpu.a == 0x99);  // A should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.y == 0x00);  // Y should equal A
  CHECK(cpu.a == 0x00);  // A should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.a == 0x42);  // A should equal X
  CHECK(cpu.x == 0x42);  // X should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.a == 0x80);  // A should equal Y
  CHECK(cpu.y == 0x80);  // Y should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == false);  // Zero flag should be clear
  CHECK(cpu.has(State::Flag::Negative) == true);  // Negative flag should be set
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.sp == 0xFE);  // SP should equal X
  CHECK(cpu.x == 0xFE);  // X should be unchanged
  // TXS does NOT affect flags
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
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
  CHECK(request == BusRequest::Read(1_addr));  // Dummy read

  request = pump.tick(cpu, BusResponse{0x23});  // Random data
  CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

  CHECK(cpu.x == 0x00);  // X should equal SP
  CHECK(cpu.sp == 0x00);  // SP should be unchanged
  CHECK(cpu.has(State::Flag::Zero) == true);  // Zero flag should be set
  CHECK(cpu.has(State::Flag::Negative) == false);  // Negative flag should be clear
  CHECK(pump.cyclesSinceLastFetch() == 2);  // Two microcode operations executed
}

// Branch instruction data structure
struct BranchInstruction
{
  const char* name;
  Byte opcode;
  State::Flag flag;
  bool condition;

  // For better test output
  friend std::ostream& operator<<(std::ostream& os, const BranchInstruction& branch)
  {
    return os << branch.name;
  }
};

// All branch instructions
static const BranchInstruction BEQ{"BEQ", 0xF0, State::Flag::Zero, true};
static const BranchInstruction BNE{"BNE", 0xD0, State::Flag::Zero, false};
static const BranchInstruction BPL{"BPL", 0x10, State::Flag::Negative, false};
static const BranchInstruction BMI{"BMI", 0x30, State::Flag::Negative, true};
static const BranchInstruction BCC{"BCC", 0x90, State::Flag::Carry, false};
static const BranchInstruction BCS{"BCS", 0xB0, State::Flag::Carry, true};
static const BranchInstruction BVC{"BVC", 0x50, State::Flag::Overflow, false};
static const BranchInstruction BVS{"BVS", 0x70, State::Flag::Overflow, true};

TEST_CASE("Branch Instructions - All Variants", "[branch][relative]")
{
  auto branch = GENERATE(BEQ, BNE, BPL, BMI, BCC, BCS, BVC, BVS);

  SECTION("Branch Not Taken")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x1000_addr;

    // Set flag to opposite of branch condition (so branch is NOT taken)
    cpu.set(branch.flag, !branch.condition);

    auto request = pump.tick(cpu, BusResponse{});  // Initial tick to fetch opcode
    CHECK(request == BusRequest::Fetch(0x1000_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});  // Branch opcode
    CHECK(request == BusRequest::Read(0x1001_addr));

    request = pump.tick(cpu, BusResponse{0x10});  // Branch offset (ignored)
    CHECK(request == BusRequest::Fetch(0x1002_addr));  // Next instruction

    CHECK(cpu.pc == 0x1003_addr);  // PC should advance normally
    CHECK(pump.cyclesSinceLastFetch() == 2);  // Two cycles for branch not taken
  }

  SECTION("Branch Taken - Same Page")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x2000_addr;

    // Set flag to match branch condition (so branch IS taken)
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x2000_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x2001_addr));

    request = pump.tick(cpu, BusResponse{0x10});  // Branch offset +16
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x2012_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x91});  // random data
    CHECK(request == BusRequest::Fetch(0x2012_addr));  // PC(2002) + offset(16) = 2012

    CHECK(cpu.pc == 0x2013_addr);  // PC should be at branch target
    CHECK(pump.cyclesSinceLastFetch() == 3);  // Three cycles for branch taken, same page
  }

  SECTION("Branch Taken - Page Crossing Forward")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x20F0_addr;  // Near page boundary

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x20F0_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x20F1_addr));

    request = pump.tick(cpu, BusResponse{0x20});  // Branch offset +32
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x2012_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x91});  // random data
    // PC after instruction = 20F2, target = 20F2 + 32 = 2112 (crosses page boundary)
    CHECK(request == BusRequest::Read(0x2112_addr));  // Wrong page read first

    request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from page crossing fixup
    CHECK(request == BusRequest::Fetch(0x2112_addr));  // Correct address after fixup

    CHECK(cpu.pc == 0x2113_addr);  // PC should be at branch target
    CHECK(pump.cyclesSinceLastFetch() == 4);  // Four cycles for page crossing branch
  }

  SECTION("Branch Taken - Page Crossing Backward")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x2110_addr;  // In upper part of page

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x2110_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x2111_addr));

    request = pump.tick(cpu, BusResponse{0xE0});  // Branch offset -32
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x21F2_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x95});  // random data
    // PC after instruction = 2112, target = 2112 + (-32) = 20F2 (crosses page boundary)
    CHECK(request == BusRequest::Read(0x20F2_addr));

    request = pump.tick(cpu, BusResponse{0xFF});  // Dummy data from page crossing fixup
    CHECK(request == BusRequest::Fetch(0x20F2_addr));  // Correct address after fixup

    CHECK(cpu.pc == 0x20F3_addr);  // PC should be at branch target
    CHECK(pump.cyclesSinceLastFetch() == 4);  // Four cycles for page crossing branch
  }

  SECTION("Branch Self-Jump Trap")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x3000_addr;

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x3000_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x3001_addr));

    // Self-branch: offset -2 should create infinite loop
    // PC after instruction = 3002, target = 3002 + (-2) = 3000 (back to start)
    CHECK_THROWS_AS(pump.tick(cpu, BusResponse{0xFE}), TrapException);
  }

  SECTION("Branch Zero Offset")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x4000_addr;

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x4000_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x4001_addr));

    request = pump.tick(cpu, BusResponse{0x00});  // Zero offset
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x4002_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x91});  // random data
    // PC after instruction = 4002, target = 4002 + 0 = 4002 (branch to next instruction)
    CHECK(request == BusRequest::Fetch(0x4002_addr));

    CHECK(cpu.pc == 0x4003_addr);  // PC should be at next instruction
    CHECK(pump.cyclesSinceLastFetch() == 3);  // Three cycles for branch taken
  }

  SECTION("Branch Maximum Positive Offset")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x5000_addr;

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x5000_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x5001_addr));

    request = pump.tick(cpu, BusResponse{0x7F});  // Maximum positive offset (+127)
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x5081_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x91});  // random data
    // PC after instruction = 5002, target = 5002 + 127 = 5081
    CHECK(request == BusRequest::Fetch(0x5081_addr));

    CHECK(cpu.pc == 0x5082_addr);
    CHECK(pump.cyclesSinceLastFetch() == 3);  // Three cycles (same page)
  }

  SECTION("Branch Maximum Negative Offset")
  {
    MicrocodePump<mos6502> pump;
    State cpu;
    cpu.pc = 0x6080_addr;  // Start high enough to avoid underflow

    // Set flag to match branch condition
    cpu.set(branch.flag, branch.condition);

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0x6080_addr));

    request = pump.tick(cpu, BusResponse{branch.opcode});
    CHECK(request == BusRequest::Read(0x6081_addr));

    request = pump.tick(cpu, BusResponse{0x80});  // Maximum negative offset (-128)
    // Dummy read to consume cycle
    CHECK(request == BusRequest::Read(0x6002_addr));  // Spurious read

    request = pump.tick(cpu, BusResponse{0x91});  // random data
    // PC after instruction = 6082, target = 6082 + (-128) = 6002
    CHECK(request == BusRequest::Fetch(0x6002_addr));

    CHECK(cpu.pc == 0x6003_addr);
    CHECK(pump.cyclesSinceLastFetch() == 3);  // Three cycles (same page)
  }
}

////////////////////////////////////////////////////////////////////////////////
// Jump Instructions
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("JMP Absolute - Functional", "[jump][absolute][functional]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Forward Jump")
  {
    State cpu;
    cpu.pc = 0x1000_addr;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x34, 0x12});  // JMP $1234

    CHECK(addressOfNextInstruction == 0x1234_addr);
  }

  SECTION("Backward Jump")
  {
    State cpu;
    cpu.pc = 0x5000_addr;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x00, 0x10});  // JMP $1000

    CHECK(addressOfNextInstruction == 0x1000_addr);
  }

  SECTION("Same Page Jump")
  {
    State cpu;
    cpu.pc = 0x4000_addr;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x80, 0x40});  // JMP $4080

    CHECK(addressOfNextInstruction == 0x4080_addr);
  }

  SECTION("Jump to Zero Page")
  {
    State cpu;
    cpu.pc = 0x8000_addr;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x80, 0x00});  // JMP $0080

    CHECK(addressOfNextInstruction == 0x0080_addr);
  }

  SECTION("Jump to High Memory")
  {
    State cpu;
    cpu.pc = 0x1000_addr;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0xFF, 0xFF});  // JMP $FFFF

    CHECK(addressOfNextInstruction == 0xFFFF_addr);
  }

  SECTION("Register State Preserved")
  {
    State cpu;
    cpu.pc = 0x2000_addr;

    // Set all registers to known values
    cpu.a = 0x42;
    cpu.x = 0x33;
    cpu.y = 0x55;
    cpu.sp = 0xFE;
    cpu.p = 0xA5;

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x00, 0x30});  // JMP $3000

    CHECK(addressOfNextInstruction == 0x3000_addr);  // PC should change

    // All other registers should be preserved
    CHECK(cpu.a == 0x42);
    CHECK(cpu.x == 0x33);
    CHECK(cpu.y == 0x55);
    CHECK(cpu.sp == 0xFE);
    CHECK(cpu.p == 0xA5);  // All flags preserved
  }
}

TEST_CASE("JMP Indirect - Functional", "[jump][indirect][functional]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Indirect Jump")
  {
    State cpu;
    cpu.pc = 0x2000_addr;

    // Set up indirect address at $3010
    memory_array[0x3010] = 0xAB;  // Target low byte
    memory_array[0x3011] = 0xCD;  // Target high byte

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0x10, 0x30});  // JMP ($3010)

    CHECK(addressOfNextInstruction == 0xCDAB_addr);  // Should jump to $CDAB
  }

  SECTION("Indirect Jump to Zero Page")
  {
    State cpu;
    cpu.pc = 0x4000_addr;

    // Set up indirect address at $1000
    memory_array[0x1000] = 0x80;  // Target low byte
    memory_array[0x1001] = 0x00;  // Target high byte

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0x00, 0x10});  // JMP ($1000)

    CHECK(addressOfNextInstruction == 0x0080_addr);  // Should jump to $0080
  }

  SECTION("Page Boundary Bug - JMP ($xxFF)")
  {
    State cpu;
    cpu.pc = 0x1000_addr;

    // Set up the bug case: indirect address $20FF
    memory_array[0x20FF] = 0x34;  // Target low byte from $20FF
    memory_array[0x2000] = 0x56;  // Target high byte from $2000 (bug: should be $2100)

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0xFF, 0x20});  // JMP ($20FF)

    // 6502 bug: high byte comes from $2000, not $2100
    CHECK(addressOfNextInstruction == 0x5634_addr);  // Should jump to $5634 (not $??34)
  }

  SECTION("Indirect Jump - No Bug Case")
  {
    State cpu;
    cpu.pc = 0x3000_addr;

    // Set up normal case (not at page boundary)
    memory_array[0x4020] = 0x78;  // Target low byte
    memory_array[0x4021] = 0x9A;  // Target high byte

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0x20, 0x40});  // JMP ($4020)

    CHECK(addressOfNextInstruction == 0x9A78_addr);  // Should jump to $9A78
  }

  SECTION("Register State Preserved")
  {
    State cpu;
    cpu.pc = 0xA000_addr;

    // Set register state to verify preservation
    cpu.a = 0xFF;
    cpu.x = 0x00;
    cpu.y = 0x88;
    cpu.sp = 0x77;
    cpu.p = 0x23;

    memory_array[0xB000] = 0xEF;  // Target low
    memory_array[0xB001] = 0xBE;  // Target high

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0x00, 0xB0});  // JMP ($B000)

    CHECK(addressOfNextInstruction == 0xBEEF_addr);  // PC should change

    // All other registers should be preserved
    CHECK(cpu.a == 0xFF);
    CHECK(cpu.x == 0x00);
    CHECK(cpu.y == 0x88);
    CHECK(cpu.sp == 0x77);
    CHECK(cpu.p == 0x23);  // All flags preserved
  }
}

TEST_CASE("JMP Self-Jump Trap Detection", "[jump][trap][functional]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("JMP Absolute Self-Jump")
  {
    State cpu;
    cpu.pc = 0x7000_addr;

    // Self-jump: JMP $7000 when PC starts at $7000
    CHECK_THROWS_AS(executeInstruction(pump, cpu, memory, {0x4C, 0x00, 0x70}), TrapException);
  }

  SECTION("JMP Absolute Near Miss (Not Trapped)")
  {
    State cpu;
    cpu.pc = 0x8000_addr;

    // Near miss: JMP $8001 when PC starts at $8000 (should NOT trap)
    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x01, 0x80});

    CHECK(addressOfNextInstruction == 0x8001_addr);  // Should execute normally
  }

  SECTION("JMP Indirect Self-Jump")
  {
    State cpu;
    cpu.pc = 0x5000_addr;

    // Set up indirect jump that points back to itself
    memory_array[0x6000] = 0x00;  // Target low byte
    memory_array[0x6001] = 0x50;  // Target high byte -> $5000

    // This should trap because it jumps back to $5000
    CHECK_THROWS_AS(executeInstruction(pump, cpu, memory, {0x6C, 0x00, 0x60}), TrapException);
  }
}

TEST_CASE("JMP Edge Cases", "[jump][functional]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Jump to Instruction Boundary")
  {
    State cpu;
    cpu.pc = 0x1000_addr;

    // Jump to the byte right after this JMP instruction
    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x4C, 0x03, 0x10});  // JMP $1003

    CHECK(addressOfNextInstruction == 0x1003_addr);
  }

  SECTION("Indirect Jump with Zero Address")
  {
    State cpu;
    cpu.pc = 0x2000_addr;

    memory_array[0x0000] = 0x00;  // Target low
    memory_array[0x0001] = 0x30;  // Target high

    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x6C, 0x00, 0x00});  // JMP ($0000)

    CHECK(addressOfNextInstruction == 0x3000_addr);
  }

  SECTION("Multiple Jumps in Sequence")
  {
    State cpu;
    cpu.pc = 0x1000_addr;

    // Set up a chain of jumps
    memory_array[0x1000] = 0x4C;
    memory_array[0x1001] = 0x00;
    memory_array[0x1002] = 0x20;  // JMP $2000
    memory_array[0x2000] = 0x4C;
    memory_array[0x2001] = 0x00;
    memory_array[0x2002] = 0x30;  // JMP $3000

    // Execute first jump
    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {});
    REQUIRE(addressOfNextInstruction == 0x2000_addr);

    // We need to fix up the PC because calling executeInstruction twice gets it out of sync.
    cpu.pc = addressOfNextInstruction;

    // Execute second jump
    addressOfNextInstruction = executeInstruction(pump, cpu, memory, {});
    CHECK(addressOfNextInstruction == 0x3000_addr);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Load tests
////////////////////////////////////////////////////////////////////////////////

// Test-only tag types for registers
struct A_Reg
{
};
struct X_Reg
{
};
struct Y_Reg
{
};

// Traits for register tag types
template<typename RegTag>
struct RegisterTraits;

template<>
struct RegisterTraits<A_Reg>
{
  static constexpr Byte State::* reg = &State::a;
  static constexpr const char* name = "LDA";
  static constexpr Byte immediate_opcode = 0xA9;
  static constexpr Byte zeropage_opcode = 0xA5;
  static constexpr Byte zeropage_indexed_opcode = 0xB5;  // LDA uses X
  static constexpr Byte absolute_opcode = 0xAD;
  static constexpr Byte absolute_indexed_opcode = 0xBD;  // LDA uses X
  static constexpr Byte index_reg_value = 0x05;  // Use X register
  static constexpr void setIndexReg(State& cpu, Byte value)
  {
    cpu.x = value;
  }
};

template<>
struct RegisterTraits<X_Reg>
{
  static constexpr Byte State::* reg = &State::x;
  static constexpr const char* name = "LDX";
  static constexpr Byte immediate_opcode = 0xA2;
  static constexpr Byte zeropage_opcode = 0xA6;
  static constexpr Byte zeropage_indexed_opcode = 0xB6;  // LDX uses Y
  static constexpr Byte absolute_opcode = 0xAE;
  static constexpr Byte absolute_indexed_opcode = 0xBE;  // LDX uses Y
  static constexpr Byte index_reg_value = 0x05;  // Use Y register
  static constexpr void setIndexReg(State& cpu, Byte value)
  {
    cpu.y = value;
  }
};

template<>
struct RegisterTraits<Y_Reg>
{
  static constexpr Byte State::* reg = &State::y;
  static constexpr const char* name = "LDY";
  static constexpr Byte immediate_opcode = 0xA0;
  static constexpr Byte zeropage_opcode = 0xA4;
  static constexpr Byte zeropage_indexed_opcode = 0xB4;  // LDY uses X
  static constexpr Byte absolute_opcode = 0xAC;
  static constexpr Byte absolute_indexed_opcode = 0xBC;  // LDY uses X
  static constexpr Byte index_reg_value = 0x05;  // Use X register
  static constexpr void setIndexReg(State& cpu, Byte value)
  {
    cpu.x = value;
  }
};

TEMPLATE_TEST_CASE("Load Register Immediate Mode", "[load][immediate]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Value")
  {
    State cpu;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x42});

    CHECK((cpu.*(Traits::reg)) == 0x42);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Zero Value")
  {
    State cpu;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x00});

    CHECK((cpu.*(Traits::reg)) == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Negative Value")
  {
    State cpu;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x80});

    CHECK((cpu.*(Traits::reg)) == 0x80);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Maximum Positive Value (0x7F)")
  {
    State cpu;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x7F});

    CHECK((cpu.*(Traits::reg)) == 0x7F);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Maximum Value (0xFF)")
  {
    State cpu;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0xFF});

    CHECK((cpu.*(Traits::reg)) == 0xFF);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }
}

TEMPLATE_TEST_CASE("Load Register Zero Page Mode", "[load][zeropage]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Operation")
  {
    State cpu;
    memory_array[0x80] = 0x99;  // Data at zero page address $80

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x80});

    CHECK((cpu.*(Traits::reg)) == 0x99);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Zero Page Address $00")
  {
    State cpu;
    memory_array[0x00] = 0x33;  // Data at zero page address $00

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x00});

    CHECK((cpu.*(Traits::reg)) == 0x33);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Zero Page Address $FF")
  {
    State cpu;
    memory_array[0xFF] = 0x77;  // Data at zero page address $FF

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0xFF});

    CHECK((cpu.*(Traits::reg)) == 0x77);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }
}

TEMPLATE_TEST_CASE("Load Register Zero Page Indexed Mode", "[load][zeropage][indexed]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Indexing")
  {
    State cpu;
    memory_array[0x85] = 0x33;  // Data at $80 + 5 = $85

    Traits::setIndexReg(cpu, 0x05);  // Set appropriate index register

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK((cpu.*(Traits::reg)) == 0x33);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Zero Page Wraparound")
  {
    State cpu;
    memory_array[0x10] = 0x77;  // Data at $80 + $90 = $110 -> $10 (wraparound)

    Traits::setIndexReg(cpu, 0x90);  // Set index to cause wraparound

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK((cpu.*(Traits::reg)) == 0x77);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Index Zero")
  {
    State cpu;
    memory_array[0x80] = 0x44;  // Data at $80 + 0 = $80

    Traits::setIndexReg(cpu, 0x00);  // Zero index

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK((cpu.*(Traits::reg)) == 0x44);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }
}

TEMPLATE_TEST_CASE("Load Register Absolute Mode", "[load][absolute]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Operation")
  {
    State cpu;
    memory_array[0x1234] = 0xAB;  // Data at absolute address $1234

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0x34, 0x12});  // Little-endian

    CHECK((cpu.*(Traits::reg)) == 0xAB);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Low Memory Address")
  {
    State cpu;
    memory_array[0x0200] = 0x55;  // Data at $0200

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0x00, 0x02});

    CHECK((cpu.*(Traits::reg)) == 0x55);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("High Memory Address")
  {
    State cpu;
    memory_array[0xFFFF] = 0x88;  // Data at $FFFF

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0xFF, 0xFF});

    CHECK((cpu.*(Traits::reg)) == 0x88);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }
}

TEMPLATE_TEST_CASE("Load Register Absolute Indexed Mode", "[load][absolute][indexed]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("No Page Crossing")
  {
    State cpu;
    memory_array[0x3025] = 0x55;  // Data at $3020 + 5 = $3025

    Traits::setIndexReg(cpu, 0x05);

    executeInstruction(pump, cpu, memory, {Traits::absolute_indexed_opcode, 0x20, 0x30});  // $3020

    CHECK((cpu.*(Traits::reg)) == 0x55);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Page Crossing Forward")
  {
    State cpu;
    memory_array[0x2110] = 0xCC;  // Data at $20F0 + $20 = $2110 (page crossing)

    Traits::setIndexReg(cpu, 0x20);

    executeInstruction(pump, cpu, memory, {Traits::absolute_indexed_opcode, 0xF0, 0x20});  // $20F0

    CHECK((cpu.*(Traits::reg)) == 0xCC);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Maximum Index Value")
  {
    State cpu;
    memory_array[0x30FF] = 0x99;  // Data at $3000 + $FF = $30FF

    Traits::setIndexReg(cpu, 0xFF);

    executeInstruction(pump, cpu, memory, {Traits::absolute_indexed_opcode, 0x00, 0x30});  // $3000

    CHECK((cpu.*(Traits::reg)) == 0x99);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Zero Index")
  {
    State cpu;
    memory_array[0x4000] = 0x11;  // Data at $4000 + 0 = $4000

    Traits::setIndexReg(cpu, 0x00);

    executeInstruction(pump, cpu, memory, {Traits::absolute_indexed_opcode, 0x00, 0x40});  // $4000

    CHECK((cpu.*(Traits::reg)) == 0x11);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }
}

TEMPLATE_TEST_CASE("Load Register Side Effects", "[load][functional]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = RegisterTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Register Independence")
  {
    State cpu;

    // Set all registers to known values
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xFF;
    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x99});

    // Check that target register changed
    CHECK((cpu.*(Traits::reg)) == 0x99);

    // Check that other registers are unchanged
    if (Traits::reg != &State::a)
      CHECK(cpu.a == 0x11);
    if (Traits::reg != &State::x)
      CHECK(cpu.x == 0x22);
    if (Traits::reg != &State::y)
      CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xFF);  // Stack pointer should never change

    // Only N and Z flags should potentially change
    Byte flag_mask = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero);
    CHECK((cpu.p & ~flag_mask) == (original_flags & ~flag_mask));
  }

  SECTION("Flags Only Change on N and Z")
  {
    State cpu;

    // Set all flags except N and Z
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Interrupt) |
            static_cast<Byte>(State::Flag::Decimal) | static_cast<Byte>(State::Flag::Overflow) |
            static_cast<Byte>(State::Flag::Unused);

    executeInstruction(pump, cpu, memory, {Traits::immediate_opcode, 0x42});

    // Check that non-NZ flags are preserved
    CHECK(cpu.has(State::Flag::Carry) == true);
    CHECK(cpu.has(State::Flag::Interrupt) == true);
    CHECK(cpu.has(State::Flag::Decimal) == true);
    CHECK(cpu.has(State::Flag::Overflow) == true);
    CHECK(cpu.has(State::Flag::Unused) == true);

    // N and Z should be updated based on loaded value
    CHECK(cpu.has(State::Flag::Negative) == false);  // 0x42 is positive
    CHECK(cpu.has(State::Flag::Zero) == false);  // 0x42 is non-zero
  }
}
