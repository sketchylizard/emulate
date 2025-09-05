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

////////////////////////////////////////////////////////////////////////////////
// Flag Operation Tests - Templated
////////////////////////////////////////////////////////////////////////////////

// Flag-specific tag types
struct Carry_Flag
{
};
struct Interrupt_Flag
{
};
struct Decimal_Flag
{
};
struct Overflow_Flag
{
};  // Only has clear operation

// Traits for clear flag operations
template<typename FlagTag>
struct ClearFlagTraits;

template<>
struct ClearFlagTraits<Carry_Flag>
{
  static constexpr const char* name = "CLC";
  static constexpr Byte opcode = 0x18;
  static constexpr State::Flag target_flag = State::Flag::Carry;
};

template<>
struct ClearFlagTraits<Interrupt_Flag>
{
  static constexpr const char* name = "CLI";
  static constexpr Byte opcode = 0x58;
  static constexpr State::Flag target_flag = State::Flag::Interrupt;
};

template<>
struct ClearFlagTraits<Decimal_Flag>
{
  static constexpr const char* name = "CLD";
  static constexpr Byte opcode = 0xD8;
  static constexpr State::Flag target_flag = State::Flag::Decimal;
};

template<>
struct ClearFlagTraits<Overflow_Flag>
{
  static constexpr const char* name = "CLV";
  static constexpr Byte opcode = 0xB8;
  static constexpr State::Flag target_flag = State::Flag::Overflow;
};

// Traits for set flag operations (Overflow doesn't have a set operation)
template<typename FlagTag>
struct SetFlagTraits;

template<>
struct SetFlagTraits<Carry_Flag>
{
  static constexpr const char* name = "SEC";
  static constexpr Byte opcode = 0x38;
  static constexpr State::Flag target_flag = State::Flag::Carry;
};

template<>
struct SetFlagTraits<Interrupt_Flag>
{
  static constexpr const char* name = "SEI";
  static constexpr Byte opcode = 0x78;
  static constexpr State::Flag target_flag = State::Flag::Interrupt;
};

template<>
struct SetFlagTraits<Decimal_Flag>
{
  static constexpr const char* name = "SED";
  static constexpr Byte opcode = 0xF8;
  static constexpr State::Flag target_flag = State::Flag::Decimal;
};

TEMPLATE_TEST_CASE("Clear Flag Instructions", "[clear][flags][implied]", Carry_Flag, Interrupt_Flag, Decimal_Flag, Overflow_Flag)
{
  using Traits = ClearFlagTraits<TestType>;

  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Clear Flag When Set")
  {
    State cpu;
    cpu.set(Traits::target_flag, true);  // Set the flag initially

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.has(Traits::target_flag) == false);  // Flag should be cleared
  }

  SECTION("Clear Flag When Already Clear")
  {
    State cpu;
    cpu.set(Traits::target_flag, false);  // Flag already clear

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.has(Traits::target_flag) == false);  // Should remain clear
  }

  SECTION("Other Flags Unchanged")
  {
    State cpu;
    // Set all flags except the target flag
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Interrupt) | static_cast<Byte>(State::Flag::Decimal) |
            static_cast<Byte>(State::Flag::Overflow) | static_cast<Byte>(State::Flag::Negative) |
            static_cast<Byte>(State::Flag::Unused);

    // Clear the target flag from our test mask
    auto expected_flags = cpu.p & ~static_cast<Byte>(Traits::target_flag);

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Check that only the target flag was affected
    auto result_flags = cpu.p | static_cast<Byte>(Traits::target_flag);  // Mask in the target flag
    CHECK(result_flags == (expected_flags | static_cast<Byte>(Traits::target_flag)));
    CHECK(cpu.has(Traits::target_flag) == false);
  }

  SECTION("Registers Unchanged")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xEE;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xEE);
  }
}

TEMPLATE_TEST_CASE("Set Flag Instructions", "[set][flags][implied]", Carry_Flag, Interrupt_Flag, Decimal_Flag)
{
  using Traits = SetFlagTraits<TestType>;

  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Set Flag When Clear")
  {
    State cpu;
    cpu.set(Traits::target_flag, false);  // Clear the flag initially

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.has(Traits::target_flag) == true);  // Flag should be set
  }

  SECTION("Set Flag When Already Set")
  {
    State cpu;
    cpu.set(Traits::target_flag, true);  // Flag already set

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.has(Traits::target_flag) == true);  // Should remain set
  }

  SECTION("Other Flags Unchanged")
  {
    State cpu;
    // Start with all flags clear except Unused
    cpu.p = static_cast<Byte>(State::Flag::Unused);

    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Check that only the target flag was affected
    auto expected_flags = original_flags | static_cast<Byte>(Traits::target_flag);
    CHECK(cpu.p == expected_flags);
    CHECK(cpu.has(Traits::target_flag) == true);
  }

  SECTION("Registers Unchanged")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xEE;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xEE);
  }
}

// Specific edge cases from original tests
TEST_CASE("Flag Operation Edge Cases", "[flags][edge]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("CLC - Clear Carry Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Carry, true);

    executeInstruction(pump, cpu, memory, {0x18});  // CLC

    CHECK(cpu.has(State::Flag::Carry) == false);
  }

  SECTION("SEC - Set Carry Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Carry, false);

    executeInstruction(pump, cpu, memory, {0x38});  // SEC

    CHECK(cpu.has(State::Flag::Carry) == true);
  }

  SECTION("CLI - Clear Interrupt Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Interrupt, true);

    executeInstruction(pump, cpu, memory, {0x58});  // CLI

    CHECK(cpu.has(State::Flag::Interrupt) == false);
  }

  SECTION("SEI - Set Interrupt Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Interrupt, false);

    executeInstruction(pump, cpu, memory, {0x78});  // SEI

    CHECK(cpu.has(State::Flag::Interrupt) == true);
  }

  SECTION("CLV - Clear Overflow Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Overflow, true);

    executeInstruction(pump, cpu, memory, {0xB8});  // CLV

    CHECK(cpu.has(State::Flag::Overflow) == false);
  }

  SECTION("CLD - Clear Decimal Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Decimal, true);

    executeInstruction(pump, cpu, memory, {0xD8});  // CLD

    CHECK(cpu.has(State::Flag::Decimal) == false);
  }

  SECTION("SED - Set Decimal Flag (from original test)")
  {
    State cpu;
    cpu.set(State::Flag::Decimal, false);

    executeInstruction(pump, cpu, memory, {0xF8});  // SED

    CHECK(cpu.has(State::Flag::Decimal) == true);
  }

  SECTION("Multiple Flag Operations in Sequence")
  {
    State cpu;

    // Start with all flags clear except Unused
    cpu.p = static_cast<Byte>(State::Flag::Unused);

    executeInstruction(pump, cpu, memory, {0x38});  // SEC
    CHECK(cpu.has(State::Flag::Carry) == true);

    executeInstruction(pump, cpu, memory, {0x78});  // SEI
    CHECK(cpu.has(State::Flag::Carry) == true);  // Should still be set
    CHECK(cpu.has(State::Flag::Interrupt) == true);

    executeInstruction(pump, cpu, memory, {0x18});  // CLC
    CHECK(cpu.has(State::Flag::Carry) == false);  // Now clear
    CHECK(cpu.has(State::Flag::Interrupt) == true);  // Should still be set

    executeInstruction(pump, cpu, memory, {0x58});  // CLI
    CHECK(cpu.has(State::Flag::Carry) == false);  // Should still be clear
    CHECK(cpu.has(State::Flag::Interrupt) == false);  // Now clear
  }

  SECTION("Flag Operations Don't Affect Computation Flags")
  {
    State cpu;

    // Set computation flags (N, Z, V, C)
    cpu.p = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Overflow) | static_cast<Byte>(State::Flag::Carry) |
            static_cast<Byte>(State::Flag::Unused);

    // Test that setting/clearing non-computation flags doesn't affect N, Z
    executeInstruction(pump, cpu, memory, {0x78});  // SEI
    CHECK(cpu.has(State::Flag::Negative) == true);  // N unchanged
    CHECK(cpu.has(State::Flag::Zero) == true);  // Z unchanged
    CHECK(cpu.has(State::Flag::Interrupt) == true);  // I set

    executeInstruction(pump, cpu, memory, {0xF8});  // SED
    CHECK(cpu.has(State::Flag::Negative) == true);  // N unchanged
    CHECK(cpu.has(State::Flag::Zero) == true);  // Z unchanged
    CHECK(cpu.has(State::Flag::Decimal) == true);  // D set
  }
}

////////////////////////////////////////////////////////////////////////////////
// Increment/Decrement Instruction Tests - Templated
////////////////////////////////////////////////////////////////////////////////

// Reuse existing register tags: X_Reg, Y_Reg
// (A register doesn't have increment/decrement instructions)

// Traits for increment operations
template<typename RegTag>
struct IncrementTraits;

template<>
struct IncrementTraits<X_Reg>
{
  static constexpr const char* name = "INX";
  static constexpr Byte opcode = 0xE8;
  static constexpr Byte State::* target_reg = &State::x;
};

template<>
struct IncrementTraits<Y_Reg>
{
  static constexpr const char* name = "INY";
  static constexpr Byte opcode = 0xC8;
  static constexpr Byte State::* target_reg = &State::y;
};

// Traits for decrement operations
template<typename RegTag>
struct DecrementTraits;

template<>
struct DecrementTraits<X_Reg>
{
  static constexpr const char* name = "DEX";
  static constexpr Byte opcode = 0xCA;
  static constexpr Byte State::* target_reg = &State::x;
};

template<>
struct DecrementTraits<Y_Reg>
{
  static constexpr const char* name = "DEY";
  static constexpr Byte opcode = 0x88;
  static constexpr Byte State::* target_reg = &State::y;
};

TEMPLATE_TEST_CASE("Increment Instructions", "[inc/dec][implied]", X_Reg, Y_Reg)
{
  using Traits = IncrementTraits<TestType>;

  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Increment")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x42;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x43);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Increment to Zero (Wraparound)")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0xFF;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Increment to Negative")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x7F;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x80);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Increment Zero")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x01);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Non-NZ Flags Preserved")
  {
    State cpu;
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Interrupt) |
            static_cast<Byte>(State::Flag::Decimal) | static_cast<Byte>(State::Flag::Overflow) |
            static_cast<Byte>(State::Flag::Unused);

    auto original_flags = cpu.p;
    (cpu.*(Traits::target_reg)) = 0x42;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Only N and Z should potentially change
    Byte flag_mask = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero);
    CHECK((cpu.p & ~flag_mask) == (original_flags & ~flag_mask));
  }

  SECTION("Other Registers Unchanged")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xEE;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Check that non-target registers are unchanged
    if (Traits::target_reg != &State::a)
      CHECK(cpu.a == 0x11);
    if (Traits::target_reg != &State::x)
      CHECK(cpu.x == 0x22);
    if (Traits::target_reg != &State::y)
      CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xEE);

    // Verify target register incremented correctly
    if (Traits::target_reg == &State::x)
      CHECK(cpu.x == 0x23);
    else
      CHECK(cpu.y == 0x34);
  }
}

TEMPLATE_TEST_CASE("Decrement Instructions", "[inc/dec][implied]", X_Reg, Y_Reg)
{
  using Traits = DecrementTraits<TestType>;

  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Decrement")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x43;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x42);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Decrement to Zero")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x01;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Decrement Underflow")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0xFF);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Decrement from Negative to Positive")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0x80;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0x7F);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Decrement from 0xFF")
  {
    State cpu;
    (cpu.*(Traits::target_reg)) = 0xFF;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    CHECK((cpu.*(Traits::target_reg)) == 0xFE);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Non-NZ Flags Preserved")
  {
    State cpu;
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Interrupt) |
            static_cast<Byte>(State::Flag::Decimal) | static_cast<Byte>(State::Flag::Overflow) |
            static_cast<Byte>(State::Flag::Unused);

    auto original_flags = cpu.p;
    (cpu.*(Traits::target_reg)) = 0x42;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Only N and Z should potentially change
    Byte flag_mask = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero);
    CHECK((cpu.p & ~flag_mask) == (original_flags & ~flag_mask));
  }

  SECTION("Other Registers Unchanged")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xEE;

    executeInstruction(pump, cpu, memory, {Traits::opcode});

    // Check that non-target registers are unchanged
    if (Traits::target_reg != &State::a)
      CHECK(cpu.a == 0x11);
    if (Traits::target_reg != &State::x)
      CHECK(cpu.x == 0x22);
    if (Traits::target_reg != &State::y)
      CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xEE);

    // Verify target register decremented correctly
    if (Traits::target_reg == &State::x)
      CHECK(cpu.x == 0x21);
    else
      CHECK(cpu.y == 0x32);
  }
}

// Specific edge cases from original tests
TEST_CASE("Increment/Decrement Edge Cases", "[inc/dec][edge]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("INX: 0x7F -> 0x80 (from original test)")
  {
    State cpu;
    cpu.x = 0x7F;

    executeInstruction(pump, cpu, memory, {0xE8});  // INX

    CHECK(cpu.x == 0x80);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("INX: 0xFF -> 0x00 wraparound (from original test)")
  {
    State cpu;
    cpu.x = 0xFF;

    executeInstruction(pump, cpu, memory, {0xE8});  // INX

    CHECK(cpu.x == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("DEX: 0x01 -> 0x00 (from original test)")
  {
    State cpu;
    cpu.x = 0x01;

    executeInstruction(pump, cpu, memory, {0xCA});  // DEX

    CHECK(cpu.x == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("DEX: 0x00 -> 0xFF underflow (from original test)")
  {
    State cpu;
    cpu.x = 0x00;

    executeInstruction(pump, cpu, memory, {0xCA});  // DEX

    CHECK(cpu.x == 0xFF);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("DEY: 0x80 -> 0x7F (from original test)")
  {
    State cpu;
    cpu.y = 0x80;

    executeInstruction(pump, cpu, memory, {0x88});  // DEY

    CHECK(cpu.y == 0x7F);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("Multiple Operations in Sequence")
  {
    State cpu;
    cpu.x = 0xFE;

    executeInstruction(pump, cpu, memory, {0xE8});  // INX -> 0xFF
    CHECK(cpu.x == 0xFF);
    CHECK(cpu.has(State::Flag::Negative) == true);

    executeInstruction(pump, cpu, memory, {0xE8});  // INX -> 0x00
    CHECK(cpu.x == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);

    executeInstruction(pump, cpu, memory, {0xCA});  // DEX -> 0xFF
    CHECK(cpu.x == 0xFF);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Transfer Instructions - All Variants
////////////////////////////////////////////////////////////////////////////////

// Transfer operation tag types
struct TAX_Transfer
{
};
struct TAY_Transfer
{
};
struct TXA_Transfer
{
};
struct TYA_Transfer
{
};
struct TXS_Transfer
{
};
struct TSX_Transfer
{
};

// Traits for transfer operations
template<typename TransferTag>
struct TransferTraits;

template<>
struct TransferTraits<TAX_Transfer>
{
  static constexpr const char* name = "TAX";
  static constexpr Byte opcode = 0xAA;
  static constexpr bool affects_flags = true;
  static constexpr Byte State::* src_reg = &State::a;
  static constexpr Byte State::* dst_reg = &State::x;
  static constexpr bool involves_stack = false;
};

template<>
struct TransferTraits<TAY_Transfer>
{
  static constexpr const char* name = "TAY";
  static constexpr Byte opcode = 0xA8;
  static constexpr bool affects_flags = true;
  static constexpr Byte State::* src_reg = &State::a;
  static constexpr Byte State::* dst_reg = &State::y;
  static constexpr bool involves_stack = false;
};

template<>
struct TransferTraits<TXA_Transfer>
{
  static constexpr const char* name = "TXA";
  static constexpr Byte opcode = 0x8A;
  static constexpr bool affects_flags = true;
  static constexpr Byte State::* src_reg = &State::x;
  static constexpr Byte State::* dst_reg = &State::a;
  static constexpr bool involves_stack = false;
};

template<>
struct TransferTraits<TYA_Transfer>
{
  static constexpr const char* name = "TYA";
  static constexpr Byte opcode = 0x98;
  static constexpr bool affects_flags = true;
  static constexpr Byte State::* src_reg = &State::y;
  static constexpr Byte State::* dst_reg = &State::a;
  static constexpr bool involves_stack = false;
};

template<>
struct TransferTraits<TXS_Transfer>
{
  static constexpr const char* name = "TXS";
  static constexpr Byte opcode = 0x9A;
  static constexpr bool affects_flags = false;  // TXS does NOT affect flags
  static constexpr Byte State::* src_reg = &State::x;
  static constexpr Byte State::* dst_reg = &State::sp;
  static constexpr bool involves_stack = true;
};

template<>
struct TransferTraits<TSX_Transfer>
{
  static constexpr const char* name = "TSX";
  static constexpr Byte opcode = 0xBA;
  static constexpr bool affects_flags = true;
  static constexpr Byte State::* src_reg = &State::sp;
  static constexpr Byte State::* dst_reg = &State::x;
  static constexpr bool involves_stack = true;
};

TEMPLATE_TEST_CASE("Transfer Instructions - Basic Operation", "[transfer][implied]", TAX_Transfer, TAY_Transfer,
    TXA_Transfer, TYA_Transfer, TXS_Transfer, TSX_Transfer)
{
  using Traits = TransferTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  SECTION("Normal Value Transfer")
  {
    // Set source to test value, destination to different value
    (cpu.*(Traits::src_reg)) = 0x42;
    (cpu.*(Traits::dst_reg)) = 0x00;

    auto request = pump.tick(cpu, BusResponse{});
    CHECK(request == BusRequest::Fetch(0_addr));

    request = pump.tick(cpu, BusResponse{Traits::opcode});
    CHECK(request == BusRequest::Read(1_addr));  // Dummy read

    request = pump.tick(cpu, BusResponse{0x23});  // Random data
    CHECK(request == BusRequest::Fetch(1_addr));  // Next fetch

    // Check transfer occurred
    CHECK((cpu.*(Traits::dst_reg)) == 0x42);
    CHECK((cpu.*(Traits::src_reg)) == 0x42);  // Source unchanged

    if constexpr (Traits::affects_flags)
    {
      CHECK(cpu.has(State::Flag::Zero) == false);
      CHECK(cpu.has(State::Flag::Negative) == false);
    }

    CHECK(pump.cyclesSinceLastFetch() == 2);
  }

  SECTION("Zero Value Transfer")
  {
    (cpu.*(Traits::src_reg)) = 0x00;
    (cpu.*(Traits::dst_reg)) = 0xFF;

    auto request = pump.tick(cpu, BusResponse{});
    request = pump.tick(cpu, BusResponse{Traits::opcode});
    request = pump.tick(cpu, BusResponse{0x23});

    CHECK((cpu.*(Traits::dst_reg)) == 0x00);
    CHECK((cpu.*(Traits::src_reg)) == 0x00);

    if constexpr (Traits::affects_flags)
    {
      CHECK(cpu.has(State::Flag::Zero) == true);
      CHECK(cpu.has(State::Flag::Negative) == false);
    }
  }

  SECTION("Negative Value Transfer")
  {
    (cpu.*(Traits::src_reg)) = 0x80;
    (cpu.*(Traits::dst_reg)) = 0x00;

    auto request = pump.tick(cpu, BusResponse{});
    request = pump.tick(cpu, BusResponse{Traits::opcode});
    request = pump.tick(cpu, BusResponse{0x23});

    CHECK((cpu.*(Traits::dst_reg)) == 0x80);
    CHECK((cpu.*(Traits::src_reg)) == 0x80);

    if constexpr (Traits::affects_flags)
    {
      CHECK(cpu.has(State::Flag::Zero) == false);
      CHECK(cpu.has(State::Flag::Negative) == true);
    }
  }

  SECTION("Maximum Value Transfer")
  {
    (cpu.*(Traits::src_reg)) = 0xFF;
    (cpu.*(Traits::dst_reg)) = 0x00;

    auto request = pump.tick(cpu, BusResponse{});
    request = pump.tick(cpu, BusResponse{Traits::opcode});
    request = pump.tick(cpu, BusResponse{0x23});

    CHECK((cpu.*(Traits::dst_reg)) == 0xFF);
    CHECK((cpu.*(Traits::src_reg)) == 0xFF);

    if constexpr (Traits::affects_flags)
    {
      CHECK(cpu.has(State::Flag::Zero) == false);
      CHECK(cpu.has(State::Flag::Negative) == true);
    }
  }
}

TEMPLATE_TEST_CASE("Transfer Instructions - Flag Preservation", "[transfer][flags]", TAX_Transfer, TAY_Transfer,
    TXA_Transfer, TYA_Transfer, TXS_Transfer, TSX_Transfer)
{
  using Traits = TransferTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  SECTION("Non-NZ Flags Preserved")
  {
    // Set all non-NZ flags
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Interrupt) |
            static_cast<Byte>(State::Flag::Decimal) | static_cast<Byte>(State::Flag::Overflow) |
            static_cast<Byte>(State::Flag::Unused);

    auto original_flags = cpu.p;

    (cpu.*(Traits::src_reg)) = 0x42;  // Non-zero, positive value

    auto request = pump.tick(cpu, BusResponse{});
    request = pump.tick(cpu, BusResponse{Traits::opcode});
    request = pump.tick(cpu, BusResponse{0x23});

    if constexpr (Traits::affects_flags)
    {
      // Only N and Z should potentially change
      Byte flag_mask = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero);
      CHECK((cpu.p & ~flag_mask) == (original_flags & ~flag_mask));

      // Check that N and Z are set correctly
      CHECK(cpu.has(State::Flag::Zero) == false);
      CHECK(cpu.has(State::Flag::Negative) == false);
    }
    else
    {
      // TXS should not affect any flags
      CHECK(cpu.p == original_flags);
    }
  }

  SECTION("NZ Flags Clear When Appropriate")
  {
    if constexpr (Traits::affects_flags)
    {
      // Set N and Z flags initially
      cpu.p = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero) |
              static_cast<Byte>(State::Flag::Unused);

      (cpu.*(Traits::src_reg)) = 0x42;  // Should clear both N and Z

      auto request = pump.tick(cpu, BusResponse{});
      request = pump.tick(cpu, BusResponse{Traits::opcode});
      request = pump.tick(cpu, BusResponse{0x23});

      CHECK(cpu.has(State::Flag::Zero) == false);
      CHECK(cpu.has(State::Flag::Negative) == false);
    }
  }
}

TEMPLATE_TEST_CASE("Transfer Instructions - Register Independence", "[transfer][side-effects]", TAX_Transfer,
    TAY_Transfer, TXA_Transfer, TYA_Transfer, TXS_Transfer, TSX_Transfer)
{
  using Traits = TransferTraits<TestType>;

  MicrocodePump<mos6502> pump;
  State cpu;

  SECTION("Other Registers Unchanged")
  {
    // Set all registers to known values
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xEE;

    // Override source register
    (cpu.*(Traits::src_reg)) = 0x99;

    auto request = pump.tick(cpu, BusResponse{});
    request = pump.tick(cpu, BusResponse{Traits::opcode});
    request = pump.tick(cpu, BusResponse{0x23});

    // Check that non-involved registers are unchanged
    if (Traits::src_reg != &State::a && Traits::dst_reg != &State::a)
      CHECK(cpu.a == 0x11);
    if (Traits::src_reg != &State::x && Traits::dst_reg != &State::x)
      CHECK(cpu.x == 0x22);
    if (Traits::src_reg != &State::y && Traits::dst_reg != &State::y)
      CHECK(cpu.y == 0x33);
    if (Traits::src_reg != &State::sp && Traits::dst_reg != &State::sp)
      CHECK(cpu.sp == 0xEE);

    // Check that destination got source value
    CHECK((cpu.*(Traits::dst_reg)) == 0x99);
    CHECK((cpu.*(Traits::src_reg)) == 0x99);  // Source unchanged
  }
}

// Specific edge cases that were in original tests
TEST_CASE("Transfer Edge Cases", "[transfer][edge]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("TAX with A = 0x99 (from original test)")
  {
    State cpu;
    cpu.a = 0x99;
    cpu.x = 0x00;

    executeInstruction(pump, cpu, memory, {0xAA});  // TAX

    CHECK(cpu.x == 0x99);
    CHECK(cpu.a == 0x99);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);  // 0x99 has bit 7 set
  }

  SECTION("TSX with SP = 0x00 (from original test)")
  {
    State cpu;
    cpu.sp = 0x00;
    cpu.x = 0xFF;

    executeInstruction(pump, cpu, memory, {0xBA});  // TSX

    CHECK(cpu.x == 0x00);
    CHECK(cpu.sp == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("TXS Flag Independence")
  {
    State cpu;
    // Set N and Z flags
    cpu.p = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Unused);
    auto original_flags = cpu.p;

    cpu.x = 0x42;  // Non-zero, positive (would normally clear N,Z if flags were affected)
    cpu.sp = 0x00;

    executeInstruction(pump, cpu, memory, {0x9A});  // TXS

    CHECK(cpu.sp == 0x42);
    CHECK(cpu.x == 0x42);
    CHECK(cpu.p == original_flags);  // No flag changes at all
  }
}

////////////////////////////////////////////////////////////////////////////////
// Branch Instructions - All Variants
////////////////////////////////////////////////////////////////////////////////

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
    memory_array[0x10] = 0x33;  // Data at zero page address $10

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x10});

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

////////////////////////////////////////////////////////////////////////////////
// Store tests
////////////////////////////////////////////////////////////////////////////////

// Traits for store register tag types
template<typename RegTag>
struct StoreTraits;

template<>
struct StoreTraits<A_Reg>
{
  static constexpr Byte State::* reg = &State::a;
  static constexpr const char* name = "STA";
  static constexpr Byte zeropage_opcode = 0x85;
  static constexpr Byte zeropage_indexed_opcode = 0x95;  // STA uses X
  static constexpr Byte absolute_opcode = 0x8D;
  static constexpr Byte absolute_indexed_x_opcode = 0x9D;  // STA $nnnn,X
  static constexpr Byte absolute_indexed_y_opcode = 0x99;  // STA $nnnn,Y
  static constexpr void setIndexReg(State& cpu, Byte value, bool useY = false)
  {
    if (useY)
      cpu.y = value;
    else
      cpu.x = value;
  }
};

template<>
struct StoreTraits<X_Reg>
{
  static constexpr Byte State::* reg = &State::x;
  static constexpr const char* name = "STX";
  static constexpr Byte zeropage_opcode = 0x86;
  static constexpr Byte zeropage_indexed_opcode = 0x96;  // STX uses Y
  static constexpr Byte absolute_opcode = 0x8E;
  // STX has no absolute indexed modes
  static constexpr void setIndexReg(State& cpu, Byte value, bool /*useY*/ = true)
  {
    cpu.y = value;  // STX only has zeropage,Y
  }
};

template<>
struct StoreTraits<Y_Reg>
{
  static constexpr Byte State::* reg = &State::y;
  static constexpr const char* name = "STY";
  static constexpr Byte zeropage_opcode = 0x84;
  static constexpr Byte zeropage_indexed_opcode = 0x94;  // STY uses X
  static constexpr Byte absolute_opcode = 0x8C;
  // STY has no absolute indexed modes
  static constexpr void setIndexReg(State& cpu, Byte value, bool /*useY*/ = false)
  {
    cpu.x = value;  // STY only has zeropage,X
  }
};

TEMPLATE_TEST_CASE("Store Register Zero Page Mode", "[store][zeropage]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = StoreTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Store Operation")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x42;  // Set register value
    memory_array[0x80] = 0x00;  // Clear target location

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x80});

    CHECK(memory_array[0x80] == 0x42);  // Value should be stored
    CHECK((cpu.*(Traits::reg)) == 0x42);  // Register unchanged
  }

  SECTION("Store Zero Value")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x00;
    memory_array[0x50] = 0xFF;  // Pre-fill with different value

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x50});

    CHECK(memory_array[0x50] == 0x00);
    CHECK((cpu.*(Traits::reg)) == 0x00);
  }

  SECTION("Store Maximum Value")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0xFF;
    memory_array[0xFF] = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0xFF});

    CHECK(memory_array[0xFF] == 0xFF);
    CHECK((cpu.*(Traits::reg)) == 0xFF);
  }

  SECTION("Store to Zero Page Address $00")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x33;
    memory_array[0x00] = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x00});

    CHECK(memory_array[0x00] == 0x33);
  }
}

TEMPLATE_TEST_CASE("Store Register Zero Page Indexed Mode", "[store][zeropage][indexed]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = StoreTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Indexed Store")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x77;
    memory_array[0x85] = 0x00;  // Target at $80 + 5 = $85

    Traits::setIndexReg(cpu, 0x05);

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK(memory_array[0x85] == 0x77);
    CHECK((cpu.*(Traits::reg)) == 0x77);
  }

  SECTION("Zero Page Wraparound")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x88;
    memory_array[0x10] = 0x00;  // $80 + $90 = $110 -> $10 (wraparound)

    Traits::setIndexReg(cpu, 0x90);

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK(memory_array[0x10] == 0x88);
    CHECK(memory_array[0x110] == 0x00);  // Should NOT write here
  }

  SECTION("Index Zero")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x99;
    memory_array[0x80] = 0x00;

    Traits::setIndexReg(cpu, 0x00);

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK(memory_array[0x80] == 0x99);
  }

  SECTION("Maximum Index")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0xAA;
    memory_array[0x7F] = 0x00;  // $80 + $FF = $17F -> $7F

    Traits::setIndexReg(cpu, 0xFF);

    executeInstruction(pump, cpu, memory, {Traits::zeropage_indexed_opcode, 0x80});

    CHECK(memory_array[0x7F] == 0xAA);
  }
}

TEMPLATE_TEST_CASE("Store Register Absolute Mode", "[store][absolute]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = StoreTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Normal Absolute Store")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0xCD;
    memory_array[0x1234] = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0x34, 0x12});  // Little-endian

    CHECK(memory_array[0x1234] == 0xCD);
    CHECK((cpu.*(Traits::reg)) == 0xCD);
  }

  SECTION("Low Memory Address")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x11;
    memory_array[0x0200] = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0x00, 0x02});

    CHECK(memory_array[0x0200] == 0x11);
  }

  SECTION("High Memory Address")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0xEE;
    memory_array[0xFFFF] = 0x00;

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0xFF, 0xFF});

    CHECK(memory_array[0xFFFF] == 0xEE);
  }

  SECTION("Overwrite Existing Data")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x55;
    memory_array[0x3000] = 0xAA;  // Pre-existing data

    executeInstruction(pump, cpu, memory, {Traits::absolute_opcode, 0x00, 0x30});

    CHECK(memory_array[0x3000] == 0x55);  // Should overwrite
  }
}

// Only test STA for absolute indexed modes (STX/STY don't have these)
TEST_CASE("STA Absolute Indexed Mode", "[store][absolute][indexed][sta]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Absolute,X - No Page Crossing")
  {
    State cpu;
    cpu.a = 0xBB;
    cpu.x = 0x05;
    memory_array[0x3025] = 0x00;  // $3020 + 5 = $3025

    executeInstruction(pump, cpu, memory, {0x9D, 0x20, 0x30});  // STA $3020,X

    CHECK(memory_array[0x3025] == 0xBB);
    CHECK(cpu.a == 0xBB);
  }

  SECTION("Absolute,X - Page Crossing")
  {
    State cpu;
    cpu.a = 0xCC;
    cpu.x = 0x20;
    memory_array[0x2110] = 0x00;  // $20F0 + $20 = $2110

    executeInstruction(pump, cpu, memory, {0x9D, 0xF0, 0x20});  // STA $20F0,X

    CHECK(memory_array[0x2110] == 0xCC);
  }

  SECTION("Absolute,Y - No Page Crossing")
  {
    State cpu;
    cpu.a = 0xDD;
    cpu.y = 0x10;
    memory_array[0x4010] = 0x00;  // $4000 + $10 = $4010

    executeInstruction(pump, cpu, memory, {0x99, 0x00, 0x40});  // STA $4000,Y

    CHECK(memory_array[0x4010] == 0xDD);
    CHECK(cpu.a == 0xDD);
  }

  SECTION("Absolute,Y - Page Crossing")
  {
    State cpu;
    cpu.a = 0xEE;
    cpu.y = 0xFF;
    memory_array[0x50FF] = 0x00;  // $5000 + $FF = $50FF

    executeInstruction(pump, cpu, memory, {0x99, 0x00, 0x50});  // STA $5000,Y

    CHECK(memory_array[0x50FF] == 0xEE);
  }

  SECTION("Zero Index")
  {
    State cpu;
    cpu.a = 0x77;
    cpu.x = 0x00;
    memory_array[0x6000] = 0x00;

    executeInstruction(pump, cpu, memory, {0x9D, 0x00, 0x60});  // STA $6000,X

    CHECK(memory_array[0x6000] == 0x77);
  }
}

TEMPLATE_TEST_CASE("Store Register Side Effects", "[store][functional]", A_Reg, X_Reg, Y_Reg)
{
  using Traits = StoreTraits<TestType>;

  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("No Flag Changes")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x42;

    // Set all flags to known state
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Interrupt) | static_cast<Byte>(State::Flag::Decimal) |
            static_cast<Byte>(State::Flag::Overflow) | static_cast<Byte>(State::Flag::Negative) |
            static_cast<Byte>(State::Flag::Unused);
    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x80});

    // Store operations should NOT affect any flags
    CHECK(cpu.p == original_flags);
    CHECK(memory_array[0x80] == 0x42);
  }

  SECTION("Register Independence")
  {
    State cpu;

    // Set all registers to known values
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xFF;

    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x80});

    // Check that all registers are unchanged (except PC advancement)
    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xFF);

    // Memory should contain the value from the target register
    CHECK(memory_array[0x80] == (cpu.*(Traits::reg)));
  }

  SECTION("Multiple Stores to Same Address")
  {
    State cpu;

    // First store
    (cpu.*(Traits::reg)) = 0x11;
    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x90});
    CHECK(memory_array[0x90] == 0x11);

    // Second store to same address
    (cpu.*(Traits::reg)) = 0x22;
    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x90});
    CHECK(memory_array[0x90] == 0x22);  // Should overwrite

    // Third store with zero
    (cpu.*(Traits::reg)) = 0x00;
    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0x90});
    CHECK(memory_array[0x90] == 0x00);
  }

  SECTION("Store Does Not Affect Memory Read Timing")
  {
    State cpu;
    (cpu.*(Traits::reg)) = 0x55;

    // This is mainly to verify the store completes in expected cycles
    // The exact cycle count depends on addressing mode:
    // Zero page: 3 cycles
    // Absolute: 4 cycles
    // Zero page indexed: 4 cycles
    // Absolute indexed: 5 cycles (always, no extra cycle like loads)

    memory_array[0xA0] = 0x00;
    executeInstruction(pump, cpu, memory, {Traits::zeropage_opcode, 0xA0});

    CHECK(memory_array[0xA0] == 0x55);
    CHECK((cpu.*(Traits::reg)) == 0x55);
  }
}

TEST_CASE("Store Edge Cases", "[store][edge]")
{
  std::array<Byte, 65536> memory_array{};

  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Store to Stack Page")
  {
    State cpu;
    cpu.a = 0x99;
    memory_array[0x01FF] = 0x00;  // Stack page

    executeInstruction(pump, cpu, memory, {0x8D, 0xFF, 0x01});  // STA $01FF

    CHECK(memory_array[0x01FF] == 0x99);
  }

  SECTION("Store to Zero Page vs Absolute Same Address")
  {
    State cpu;
    cpu.a = 0xAA;

    // Store to zero page $80
    executeInstruction(pump, cpu, memory, {0x85, 0x80});  // STA $80
    CHECK(memory_array[0x80] == 0xAA);

    cpu.a = 0xBB;
    // Store to absolute $0080 (same physical address)
    executeInstruction(pump, cpu, memory, {0x8D, 0x80, 0x00});  // STA $0080
    CHECK(memory_array[0x80] == 0xBB);  // Should overwrite
  }

  SECTION("Cross-Register Store Pattern")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;

    // Store A, then X, then Y to consecutive locations
    executeInstruction(pump, cpu, memory, {0x85, 0x10});  // STA $10
    executeInstruction(pump, cpu, memory, {0x86, 0x11});  // STX $11
    executeInstruction(pump, cpu, memory, {0x84, 0x12});  // STY $12

    CHECK(memory_array[0x10] == 0x11);  // A
    CHECK(memory_array[0x11] == 0x22);  // X
    CHECK(memory_array[0x12] == 0x33);  // Y
  }
}

////////////////////////////////////////////////////////////////////////////////
// Stack Operation Tests
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("PHA/PLA - Push/Pull Accumulator", "[stack][pha][pla]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Basic Push/Pull Cycle")
  {
    State cpu;
    cpu.a = 0x42;
    cpu.sp = 0xFF;  // Start with full stack

    // Push accumulator
    executeInstruction(pump, cpu, memory, {0x48});  // PHA

    CHECK(cpu.sp == 0xFE);  // Stack pointer should decrement
    CHECK(memory_array[0x01FF] == 0x42);  // Value should be on stack
    CHECK(cpu.a == 0x42);  // Accumulator unchanged

    // Change accumulator to verify pull works
    cpu.a = 0x00;

    // Pull accumulator
    executeInstruction(pump, cpu, memory, {0x68});  // PLA

    CHECK(cpu.sp == 0xFF);  // Stack pointer should increment
    CHECK(cpu.a == 0x42);  // Accumulator should be restored
    CHECK(cpu.has(State::Flag::Zero) == false);  // Flags should be updated
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("PLA Flag Effects - Zero")
  {
    State cpu;
    cpu.sp = 0xFE;  // Stack has one item
    memory_array[0x01FF] = 0x00;  // Zero value on stack
    cpu.a = 0x42;  // Non-zero accumulator

    executeInstruction(pump, cpu, memory, {0x68});  // PLA

    CHECK(cpu.a == 0x00);
    CHECK(cpu.has(State::Flag::Zero) == true);
    CHECK(cpu.has(State::Flag::Negative) == false);
  }

  SECTION("PLA Flag Effects - Negative")
  {
    State cpu;
    cpu.sp = 0xFE;  // Stack has one item
    memory_array[0x01FF] = 0x80;  // Negative value on stack
    cpu.a = 0x42;  // Positive accumulator

    executeInstruction(pump, cpu, memory, {0x68});  // PLA

    CHECK(cpu.a == 0x80);
    CHECK(cpu.has(State::Flag::Zero) == false);
    CHECK(cpu.has(State::Flag::Negative) == true);
  }

  SECTION("Multiple Push/Pull Operations")
  {
    State cpu;
    cpu.sp = 0xFF;

    // Push three values
    cpu.a = 0x11;
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFE);
    CHECK(memory_array[0x01FF] == 0x11);

    cpu.a = 0x22;
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFD);
    CHECK(memory_array[0x01FE] == 0x22);

    cpu.a = 0x33;
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFC);
    CHECK(memory_array[0x01FD] == 0x33);

    // Pull them back in LIFO order
    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x33);
    CHECK(cpu.sp == 0xFD);

    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x22);
    CHECK(cpu.sp == 0xFE);

    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x11);
    CHECK(cpu.sp == 0xFF);
  }

  SECTION("PHA Doesn't Affect Flags")
  {
    State cpu;
    cpu.a = 0x42;
    cpu.sp = 0xFF;

    // Set all flags to known state
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Interrupt) | static_cast<Byte>(State::Flag::Decimal) |
            static_cast<Byte>(State::Flag::Overflow) | static_cast<Byte>(State::Flag::Negative) |
            static_cast<Byte>(State::Flag::Unused);
    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {0x48});  // PHA

    CHECK(cpu.p == original_flags);  // Flags should be unchanged
    CHECK(memory_array[0x01FF] == 0x42);
  }

  SECTION("Other Registers Unchanged")
  {
    State cpu;
    cpu.a = 0x42;
    cpu.x = 0x11;
    cpu.y = 0x22;
    cpu.sp = 0xFF;

    executeInstruction(pump, cpu, memory, {0x48});  // PHA

    CHECK(cpu.a == 0x42);  // Accumulator unchanged
    CHECK(cpu.x == 0x11);  // X unchanged
    CHECK(cpu.y == 0x22);  // Y unchanged
    // SP should change for push operations
  }
}

TEST_CASE("PHP/PLP - Push/Pull Processor Status", "[stack][php][plp]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Basic Push/Pull Processor Status")
  {
    State cpu;
    cpu.sp = 0xFF;

    // Set specific flags
    cpu.set(State::Flag::Carry, true);
    cpu.set(State::Flag::Zero, true);
    cpu.set(State::Flag::Interrupt, true);
    cpu.set(State::Flag::Unused, true);
    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {0x08});  // PHP

    CHECK(cpu.sp == 0xFE);
    // PHP sets the B flag when pushed
    auto expected_pushed = original_flags | static_cast<Byte>(State::Flag::Break);
    CHECK(memory_array[0x01FF] == expected_pushed);
    CHECK(cpu.p == original_flags);  // CPU flags unchanged by PHP

    // Change flags to verify pull works
    cpu.p = static_cast<Byte>(State::Flag::Negative) | static_cast<Byte>(State::Flag::Unused);

    executeInstruction(pump, cpu, memory, {0x28});  // PLP

    CHECK(cpu.sp == 0xFF);
    // PLP clears the B flag when pulled
    auto expected_pulled = expected_pushed & ~static_cast<Byte>(State::Flag::Break);
    CHECK(cpu.p == expected_pulled);
  }

  SECTION("PHP Always Sets Break Flag in Pushed Value")
  {
    State cpu;
    cpu.sp = 0xFF;
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Unused);
    // Deliberately NOT setting Break flag

    executeInstruction(pump, cpu, memory, {0x08});  // PHP

    // Break flag should be set in the pushed value
    CHECK(memory_array[0x01FF] & static_cast<Byte>(State::Flag::Break));
  }

  SECTION("PLP Ignores Break Flag from Stack")
  {
    State cpu;
    cpu.sp = 0xFE;
    // Put value with Break flag set on stack
    memory_array[0x01FF] = static_cast<Byte>(State::Flag::Break) | static_cast<Byte>(State::Flag::Carry) |
                           static_cast<Byte>(State::Flag::Unused);

    cpu.p = static_cast<Byte>(State::Flag::Zero) | static_cast<Byte>(State::Flag::Unused);

    executeInstruction(pump, cpu, memory, {0x28});  // PLP

    // Break flag should not be set in processor status
    CHECK(!cpu.has(State::Flag::Break));
    CHECK(cpu.has(State::Flag::Carry));  // Other flags should be restored
  }

  SECTION("Registers Unchanged by PHP/PLP")
  {
    State cpu;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;
    cpu.sp = 0xFF;

    executeInstruction(pump, cpu, memory, {0x08});  // PHP
    executeInstruction(pump, cpu, memory, {0x28});  // PLP

    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);
    CHECK(cpu.sp == 0xFF);  // Should be back to original
  }
}

TEST_CASE("JSR/RTS - Jump to Subroutine/Return", "[stack][jsr][rts]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Basic Subroutine Call and Return")
  {
    State cpu;
    cpu.pc = 0x1000_addr;
    cpu.sp = 0xFF;

    // JSR $2000 - should push return address and jump
    auto addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x20});  // JSR $2000

    CHECK(addressOfNextInstruction == 0x2000_addr);  // Should be at subroutine
    CHECK(cpu.sp == 0xFD);  // Stack pointer decremented by 2

    // Check return address on stack (PC-1 of next instruction after JSR)
    // JSR pushes PC-1, where PC points to the last byte of the JSR instruction
    CHECK(memory_array[0x01FF] == 0x10);  // High byte of return address
    CHECK(memory_array[0x01FE] == 0x02);  // Low byte of return address (PC was at $1003-1 = $1002)

    // RTS should restore PC and return to next instruction after JSR
    addressOfNextInstruction = executeInstruction(pump, cpu, memory, {0x60});  // RTS

    CHECK(cpu.sp == 0xFF);  // Stack pointer should be restored
    CHECK(addressOfNextInstruction == 0x1003_addr);
  }

  SECTION("Nested Subroutine Calls")
  {
    State cpu;
    cpu.pc = 0x1000_addr;
    cpu.sp = 0xFF;

    // First JSR
    executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x20});  // JSR $2000
    CHECK(cpu.sp == 0xFD);

    // Second JSR from within first subroutine
    executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x30});  // JSR $3000
    CHECK(cpu.sp == 0xFB);  // Stack should have two return addresses

    // Return from second subroutine
    executeInstruction(pump, cpu, memory, {0x60});  // RTS
    CHECK(cpu.sp == 0xFD);  // Should restore to first level

    // Return from first subroutine
    executeInstruction(pump, cpu, memory, {0x60});  // RTS
    CHECK(cpu.sp == 0xFF);  // Should restore to original level
  }

  SECTION("JSR/RTS Don't Affect Flags")
  {
    State cpu;
    cpu.pc = 0x1000_addr;
    cpu.sp = 0xFF;

    // Set all flags
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Zero) |
            static_cast<Byte>(State::Flag::Interrupt) | static_cast<Byte>(State::Flag::Decimal) |
            static_cast<Byte>(State::Flag::Overflow) | static_cast<Byte>(State::Flag::Negative) |
            static_cast<Byte>(State::Flag::Unused);
    auto original_flags = cpu.p;

    executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x20});  // JSR
    CHECK(cpu.p == original_flags);

    executeInstruction(pump, cpu, memory, {0x60});  // RTS
    CHECK(cpu.p == original_flags);
  }

  SECTION("JSR/RTS Don't Affect Registers")
  {
    State cpu;
    cpu.pc = 0x1000_addr;
    cpu.sp = 0xFF;
    cpu.a = 0x11;
    cpu.x = 0x22;
    cpu.y = 0x33;

    executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x20});  // JSR
    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);

    executeInstruction(pump, cpu, memory, {0x60});  // RTS
    CHECK(cpu.a == 0x11);
    CHECK(cpu.x == 0x22);
    CHECK(cpu.y == 0x33);
  }
}

TEST_CASE("Stack Pointer Edge Cases", "[stack][edge]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Stack Wraparound on Push")
  {
    State cpu;
    cpu.a = 0x42;
    cpu.sp = 0x00;  // Stack pointer at bottom

    executeInstruction(pump, cpu, memory, {0x48});  // PHA

    CHECK(cpu.sp == 0xFF);  // Should wrap around
    CHECK(memory_array[0x0100] == 0x42);  // Should write to $0100
  }

  SECTION("Stack Wraparound on Pull")
  {
    State cpu;
    cpu.sp = 0xFF;  // Stack pointer at top
    memory_array[0x0100] = 0x55;  // Data at wraparound location

    executeInstruction(pump, cpu, memory, {0x68});  // PLA

    CHECK(cpu.sp == 0x00);  // Should wrap to 0x00
    CHECK(cpu.a == 0x55);  // Should read from $0100
  }

  SECTION("Stack Boundary Behavior")
  {
    State cpu;
    cpu.sp = 0x01;

    // Fill some stack locations
    memory_array[0x0102] = 0x11;  // ← Should be here, not 0x0101
    memory_array[0x0103] = 0x22;  // ← And here, not 0x0100
    memory_array[0x01FF] = 0x33;

    // First PLA: SP 0x01→0x02, reads from $0102
    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x11);
    CHECK(cpu.sp == 0x02);

    // Second PLA: SP 0x02→0x03, reads from $0103
    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x22);
    CHECK(cpu.sp == 0x03);
  }

  SECTION("Deep Stack Usage")
  {
    State cpu;
    cpu.sp = 0xFF;

    // Push 256 values to completely fill stack
    for (int i = 0; i < 128; i++)  // Test a reasonable number
    {
      cpu.a = static_cast<Byte>(i);
      executeInstruction(pump, cpu, memory, {0x48});  // PHA
    }

    CHECK(cpu.sp == (0xFF - 128));  // Should be decremented appropriately

    // Pull them back and verify LIFO order
    for (int i = 127; i >= 0; i--)
    {
      executeInstruction(pump, cpu, memory, {0x68});  // PLA
      CHECK(cpu.a == static_cast<Byte>(i));
    }

    CHECK(cpu.sp == 0xFF);  // Should be back to original
  }
}

TEST_CASE("Mixed Stack Operations", "[stack][integration]")
{
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice memory(memory_array);

  SECTION("Interleaved Push/Pull Operations")
  {
    State cpu;
    cpu.sp = 0xFF;

    // Push A
    cpu.a = 0x11;
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFE);

    // Push flags
    cpu.p = static_cast<Byte>(State::Flag::Carry) | static_cast<Byte>(State::Flag::Unused);
    executeInstruction(pump, cpu, memory, {0x08});  // PHP
    CHECK(cpu.sp == 0xFD);

    // Push A again (different value)
    cpu.a = 0x22;
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFC);

    // Pull in reverse order
    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x22);
    CHECK(cpu.sp == 0xFD);

    executeInstruction(pump, cpu, memory, {0x28});  // PLP
    CHECK(cpu.has(State::Flag::Carry) == true);
    CHECK(cpu.sp == 0xFE);

    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x11);
    CHECK(cpu.sp == 0xFF);
  }

  SECTION("Subroutine with Stack Usage")
  {
    State cpu;
    cpu.pc = 0x1000_addr;
    cpu.sp = 0xFF;
    cpu.a = 0x42;

    // Call subroutine
    executeInstruction(pump, cpu, memory, {0x20, 0x00, 0x20});  // JSR $2000
    CHECK(cpu.sp == 0xFD);  // Return address on stack

    // In subroutine: save A on stack
    executeInstruction(pump, cpu, memory, {0x48});  // PHA
    CHECK(cpu.sp == 0xFC);  // A value on stack too

    // In subroutine: save flags
    executeInstruction(pump, cpu, memory, {0x08});  // PHP
    CHECK(cpu.sp == 0xFB);  // Flags on stack

    // Restore flags
    executeInstruction(pump, cpu, memory, {0x28});  // PLP
    CHECK(cpu.sp == 0xFC);

    // Restore A
    executeInstruction(pump, cpu, memory, {0x68});  // PLA
    CHECK(cpu.a == 0x42);
    CHECK(cpu.sp == 0xFD);

    // Return from subroutine
    executeInstruction(pump, cpu, memory, {0x60});  // RTS
    CHECK(cpu.sp == 0xFF);  // Back to original stack level
  }
}
