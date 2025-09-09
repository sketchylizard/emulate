#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <format>
#include <fstream>
#include <span>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "cpu6502/address_mode.h"
#include "cpu6502/state.h"

using namespace Common;
using namespace cpu6502;

static bool operator==(const State& lhs, const State& rhs) noexcept
{
  return lhs.pc == rhs.pc && lhs.a == rhs.a && lhs.x == rhs.x && lhs.y == rhs.y && lhs.sp == rhs.sp && lhs.p == rhs.p &&
         lhs.hi == rhs.hi && lhs.lo == rhs.lo;
}

TEST_CASE("requestAddress8", "[addressing]")
{
  State cpu;
  cpu.pc = 0x8001_addr;
  cpu.lo = 0xEE;
  cpu.hi = 0xFF;  // Set hi/lo to ensure they are changed

  auto request = AddressMode::requestAddress8(cpu, BusResponse{});
  CHECK(request.request == BusRequest::Read(0x8001_addr));

  // Only the PC should be incremented
  CHECK(cpu.pc == 0x8002_addr);
}

TEST_CASE("requestAddress16", "[addressing]")
{
  State cpu;
  cpu.pc = 0x8002_addr;

  // Request 16-bit address should read 2 bytes, low byte first, then high byte

  auto [request, next] = AddressMode::requestAddress16(cpu, BusResponse{0x12});
  CHECK(request == BusRequest::Read(0x8002_addr));

  auto [request2, next2] = next(cpu, BusResponse{0x34});
  CHECK(request2 == BusRequest::Read(0x8003_addr));
  CHECK(next2 == nullptr);

  // Only the PC, and the lo byte should be set
  CHECK(cpu.pc == 0x8004_addr);
  CHECK(cpu.lo == 0x34);
}

TEST_CASE("requestAddress", "[addressing]")
{
  State cpu;
  cpu.pc = 0x8003_addr;
  cpu.lo = 0x34;

  auto [request, next] = Absolute::ops[0](cpu, BusResponse{0x99});  // random
  CHECK(request == BusRequest::Read(0x8003_addr));
  CHECK(next != nullptr);
  auto [request2, next2] = next(cpu, BusResponse{0x34});
  CHECK(request2 == BusRequest::Read(0x8004_addr));
  CHECK(next2 == nullptr);

  // Only the PC, the lo byte, and the hi byte should be set
  CHECK(cpu.pc == 0x8005_addr);
  CHECK(cpu.lo == 0x34);
}

// Helper to select register for template test case
template<int N>
struct RegType
{
};
using XReg = RegType<0>;
using YReg = RegType<1>;

bool executeAddress(State& cpu, std::span<const Microcode> codeToExecute, std::span<const Cycle> cycles)
{
  auto cycleCount = 0;
  MicrocodeResponse response{};

  for (const auto& cycle : cycles)
  {
    ++cycleCount;
    do
    {
      // Execute the next microcode step
      if (response.injection == nullptr)
      {
        // Start of a new instruction or no next step; fetch the next microcode operation
        if (codeToExecute.empty())
        {
          UNSCOPED_INFO("No more microcode to execute at cycle " << cycleCount);
          return false;
        }
        response = codeToExecute.front()(cpu, BusResponse{cycle.input});
        codeToExecute = codeToExecute.subspan(1);
      }
      else
      {
        response = response.injection(cpu, BusResponse{cycle.input});
      }
    } while (!response.request);

    // Compare with expected result
    if (response.request != cycle.expected)
    {
      UNSCOPED_INFO(
          "Cycle " << cycleCount << ": Expected "
                   << std::format("{{address: {:04x}, data: {}, control: {}}}",
                          static_cast<uint16_t>(cycle.expected.address), std::format("{:#04x}", cycle.expected.data),
                          std::format("{:#04x}", static_cast<uint8_t>(cycle.expected.control)))
                   << " but got "
                   << std::format("{{address: {:04x}, data: {}, control: {}}}",
                          static_cast<uint16_t>(response.request.address), std::format("{:#04x}", response.request.data),
                          std::format("{:#04x}", static_cast<uint8_t>(response.request.control))));
      return false;
    }
  }
  return true;
}

TEMPLATE_TEST_CASE("AbsoluteIndex", "[addressing]", AbsoluteX, AbsoluteY)
{
  constexpr auto reg = std::is_same_v<TestType, AbsoluteX> ? &State::x : &State::y;

  State cpu;
  cpu.pc = 0x1000_addr;

  SECTION("Basic offset - no page crossing")
  {
    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x1000_addr)},
        // Step 1: Read high byte of address
        {0x34, BusRequest::Read(0x1001_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x1235_addr)},
    };

    executeAddress(cpu, TestType::ops, cycles);

    CHECK(cpu.pc == 0x1002_addr);
    CHECK(cpu.lo == 0x35);
    CHECK(cpu.hi == 0x12);
    CHECK((cpu.*reg) == 0x01);
  }

  SECTION("Page boundary crossing - forward overflow")
  {
    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x1000_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x1001_addr)},
        // Step 2: Add index register to low byte and read from effective address (wrong address first)
        {0x12, BusRequest::Read(0x127F_addr)},
        // Step 3: Read from correct effective address after page boundary fix
        {0x99, BusRequest::Read(0x137F_addr)},
    };
    executeAddress(cpu, TestType::ops, cycles);

    CHECK(cpu.pc == 0x1002_addr);
    CHECK(cpu.lo == 0x7F);
    CHECK(cpu.hi == 0x13);
    CHECK((cpu.*reg) == 0xFF);
  }

  SECTION("Page boundary crossing - maximum offset")
  {
    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x99, BusRequest::Read(0x1000_addr)},
        // Step 1: Read high byte of address
        {0xFF, BusRequest::Read(0x1001_addr)},
        // Step 2: Add index register to low byte and read from effective address (wrong address first)
        {0x12, BusRequest::Read(0x12FE_addr)},
        // Step 3: Read from correct effective address after page boundary fix
        {0x99, BusRequest::Read(0x13FE_addr)},
    };
    executeAddress(cpu, TestType::ops, cycles);

    // Hi byte corrected, lo byte wrapped
    CHECK(cpu.hi == 0x13);  // Corrected from 0x12 to 0x13
    CHECK(cpu.lo == 0xFE);  // 0xFF + 0xFF = 0x1FE, low byte = 0xFE
  }

  SECTION("No page boundary crossing - near boundary")
  {
    (cpu.*reg) = 0x7F;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x99, BusRequest::Read(0x1000_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x1001_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x12FF_addr)},
    };

    executeAddress(cpu, TestType::ops, cycles);

    // No correction needed
    CHECK(cpu.hi == 0x12);  // Unchanged
    CHECK(cpu.lo == 0xFF);  // 0x80 + 0x7F = 0xFF
  }
}

TEMPLATE_TEST_CASE("absolute-indexed", "[addressing]", AbsoluteX, AbsoluteY)
{
  constexpr auto reg = std::is_same_v<TestType, AbsoluteX> ? &State::x : &State::y;

  SECTION("Basic offset - no page crossing")
  {
    State cpu;
    cpu.pc = 0x0800_addr;

    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x34, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x1235_addr)},
    };
    CHECK(executeAddress(cpu, TestType::ops, cycles));
  }

  SECTION("Page boundary crossing - forward overflow")
  {
    State cpu;
    cpu.pc = 0x800_addr;

    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address (wrong address first)
        {0x12, BusRequest::Read(0x127F_addr)},
        // Step 3: Read from correct effective address after page boundary fix
        {0x99, BusRequest::Read(0x137F_addr)},
    };
    CHECK(executeAddress(cpu, TestType::ops, cycles));
  }

  SECTION("Page boundary crossing - maximum offset")
  {
    State cpu;
    cpu.pc = 0x0800_addr;

    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address (wrong address first)
        {0x12, BusRequest::Read(0x127F_addr)},
        // Step 3: Read from correct effective address after page boundary fix
        {0x99, BusRequest::Read(0x137F_addr)},
    };
    CHECK(executeAddress(cpu, TestType::ops, cycles));
  }

  SECTION("No page boundary crossing - near boundary")
  {
    State cpu;
    cpu.pc = 0x0800_addr;

    (cpu.*reg) = 0x7F;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x12FF_addr)},
    };
    CHECK(executeAddress(cpu, TestType::ops, cycles));
  }
}

TEST_CASE("requestZeroPageAddress", "[addressing]")
{
  State cpu;
  auto request = ZeroPage::ops[0](cpu, BusResponse{});
  CHECK(request.request == BusRequest::Read(0x0000_addr));  // PC is 0, so read from $0000
  request = ZeroPage::ops[1](cpu, BusResponse{0x42});
  CHECK(request.request == BusRequest::Read(0x0042_addr));  // Zero page address formed from response

  // Lo byte should be set from response data
  CHECK(cpu.lo == 0x42);
  CHECK(cpu.hi == 0x00);
}

TEMPLATE_TEST_CASE("requestZeroPageAddressIndexed", "[addressing]", ZeroPageX, ZeroPageY)
{
  constexpr auto reg = std::is_same_v<TestType, ZeroPageX> ? &State::x : &State::y;

  SECTION("Normal addition within zero page")
  {
    State cpu;
    cpu.hi = 0x00;

    (cpu.*reg) = 0x10;

    auto request = TestType::ops[1](cpu, BusResponse{0x80});
    // first read is done without adding the index
    CHECK(request.request == BusRequest::Read(0x0080_addr));

    // Second read should be correct, $80 + $10 = $90
    request = TestType::ops[2](cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x0090_addr));

    State expected{.lo = 0x90, .hi = 0x00};
    (expected.*reg) = 0x10;
    CHECK((cpu == expected));
  }

  SECTION("Overflow wraps within zero page")
  {
    State cpu;
    cpu.hi = 0x00;

    (cpu.*reg) = 0x10;

    // first read is done without adding the index
    auto request = TestType::ops[1](cpu, BusResponse{0xF8});
    CHECK(request.request == BusRequest::Read(0x00F8_addr));

    // Second read should wrap around, $F8 + $10 = $108, wraps to $08
    request = TestType::ops[2](cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x0008_addr));

    State expected{.lo = 0x08, .hi = 0x00};
    (expected.*reg) = 0x10;
    CHECK((cpu == expected));
  }

  SECTION("Maximum overflow")
  {
    State cpu;
    cpu.hi = 0x00;

    (cpu.*reg) = 0xFF;

    // first read is done without adding the index
    auto request = TestType::ops[1](cpu, BusResponse{0xFF});
    CHECK(request.request == BusRequest::Read(0x00FF_addr));

    // Second read should wrap around, $FF + $FF = $1FE, wraps to $FE
    request = TestType::ops[2](cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x00FE_addr));

    State expected{.lo = 0xFE, .hi = 0x00};
    (expected.*reg) = 0xFF;
    CHECK((cpu == expected));
  }

  SECTION("No offset - register is zero")
  {
    State cpu;
    cpu.hi = 0x00;

    (cpu.*reg) = 0x00;

    auto request = TestType::ops[1](cpu, BusResponse{0x42});
    CHECK(request.request == BusRequest::Read(0x0042_addr));  // $42 + $00 = $42

    State expected{.lo = 0x42, .hi = 0x00};
    (expected.*reg) = 0x00;
    CHECK((cpu == expected));
  }
}

TEST_CASE("Zero page addressing mode complete sequence", "[addressing]")
{
  SECTION("Basic zero page read")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // requestAddress8: dummy input, request operand, set hi=0
        {0x42, Common::BusRequest::Read(0x0042_addr)},  // requestZeroPageAddress: receive ZP addr (0x42), request
                                                        // from $0042
    };

    CHECK(executeAddress(cpu, ZeroPage::ops, cycles));

    // Verify final state after addressing mode complete
    CHECK(cpu.pc == 0x8002_addr);  // PC incremented by requestAddress8
    CHECK(cpu.hi == 0x00);  // High byte set to 0 for zero page
    CHECK(cpu.lo == 0x42);  // Low byte contains zero page address
  }

  SECTION("Zero page address 0x00")
  {
    State cpu;
    cpu.pc = 0x9000_addr;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x9000_addr)},  // Request operand
        {0x00, Common::BusRequest::Read(0x0000_addr)},  // Request from address $00
    };

    CHECK(executeAddress(cpu, ZeroPage::ops, cycles));

    CHECK(cpu.pc == 0x9001_addr);
    CHECK(cpu.hi == 0x00);
    CHECK(cpu.lo == 0x00);
  }

  SECTION("Zero page address 0xFF")
  {
    State cpu;
    cpu.pc = 0xA000_addr;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0xA000_addr)},  // Request operand
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Request from address $FF
    };

    CHECK(executeAddress(cpu, ZeroPage::ops, cycles));

    CHECK(cpu.pc == 0xA001_addr);
    CHECK(cpu.hi == 0x00);
    CHECK(cpu.lo == 0xFF);
  }

  SECTION("Multiple sequential zero page operations")
  {
    State cpu;
    cpu.pc = 0x8000_addr;

    // First zero page access
    Cycle cycles1[] = {
        {0x00, Common::BusRequest::Read(0x8000_addr)},
        {0x10, Common::BusRequest::Read(0x0010_addr)},
    };

    CHECK(executeAddress(cpu, ZeroPage::ops, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x10);

    // Second zero page access (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},
        {0x20, Common::BusRequest::Read(0x0020_addr)},
    };

    CHECK(executeAddress(cpu, ZeroPage::ops, cycles2));
    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x20);
  }
}

TEMPLATE_TEST_CASE("Zero page indexed addressing mode complete sequence", "[addressing]", ZeroPageX, ZeroPageY)
{
  constexpr auto reg = std::is_same_v<TestType, ZeroPageX> ? &State::x : &State::y;

  SECTION("Basic zero page indexed read - no wrapping")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    (cpu.*reg) = 0x05;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},
        {0x40, Common::BusRequest::Read(0x0040_addr)},
        {0x40, Common::BusRequest::Read(0x0045_addr)},
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x45);
    CHECK(cpu.hi == 0x00);
    CHECK((cpu.*reg) == 0x05);
  }

  SECTION("Zero page indexed with wrapping")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    (cpu.*reg) = 0x10;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // Request operand
        {0xF8, Common::BusRequest::Read(0x00F8_addr)},  // Read from address before adding index
        {0x99, Common::BusRequest::Read(0x0008_addr)},  // 0xF8 + 0x10 = 0x108, wraps to 0x08
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x08);
    CHECK(cpu.hi == 0x00);
    CHECK((cpu.*reg) == 0x10);
  }

  SECTION("Zero page indexed maximum wrapping")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Reads from address before adding index
        {0xBB, Common::BusRequest::Read(0x00FE_addr)},  // 0xFF + 0xFF = 0x1FE, wraps to 0xFE
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0xFE);
    CHECK(cpu.hi == 0x00);
    CHECK((cpu.*reg) == 0xFF);
  }

  SECTION("Zero page indexed with register=0 (no offset)")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    (cpu.*reg) = 0x00;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0x42, Common::BusRequest::Read(0x0042_addr)},  // Still reads from address before indexing
        {0x33, Common::BusRequest::Read(0x0042_addr)},  // Reads from address after adding index (no change)
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x42);
    CHECK(cpu.hi == 0x00);
    CHECK((cpu.*reg) == 0x00);
  }

  SECTION("Zero page indexed edge case - wrapping to start of zero page")
  {
    State cpu;
    cpu.pc = 0x8001_addr;

    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Reads from address before adding index
        {0x11, Common::BusRequest::Read(0x0000_addr)},  // 0xFF + 0x01 = 0x100, wraps to 0x00
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x00);
    CHECK(cpu.hi == 0x00);
    CHECK((cpu.*reg) == 0x01);
  }

  SECTION("Sequential zero page indexed operations")
  {
    State cpu;
    cpu.pc = 0x8000_addr;

    (cpu.*reg) = 0x02;

    // First operation
    Cycle cycles1[] = {
        {0x00, Common::BusRequest::Read(0x8000_addr)},  //
        {0x10, Common::BusRequest::Read(0x0010_addr)},  // Read from address before adding index
        {0xFF, Common::BusRequest::Read(0x0012_addr)},  // 0x10 + 0x02 = 0x12
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x12);

    // Second operation (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0x20, Common::BusRequest::Read(0x0020_addr)},  // Read from address before adding index
        {0xEF, Common::BusRequest::Read(0x0022_addr)},  // 0x20 + 0x02 = 0x22
    };

    CHECK(executeAddress(cpu, TestType::ops, cycles2));

    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x22);
    CHECK((cpu.*reg) == 0x02);
  }
}

TEST_CASE("Immediate addressing mode complete sequence", "[addressing]")
{
  SECTION("Basic immediate mode")
  {
    State cpu;
    cpu.pc = 0x8001_addr;


    Cycle cycles[] = {
        {0x42, Common::BusRequest::Read(0x8001_addr)},  // requestAddress8: receive immediate value (0x42)
    };

    CHECK(executeAddress(cpu, Immediate::ops, cycles));

    // Verify final state
    CHECK(cpu.pc == 0x8002_addr);  // PC incremented
    CHECK(cpu.hi == 0x00);  // Hi byte cleared (for zero page compatibility)
  }

  SECTION("Immediate mode with different values")
  {
    State cpu;
    cpu.pc = 0x9000_addr;


    Cycle cycles[] = {
        {0xFF, Common::BusRequest::Read(0x9000_addr)},  // Maximum 8-bit value
    };

    CHECK(executeAddress(cpu, Immediate::ops, cycles));
  }

  SECTION("Immediate mode with zero")
  {
    State cpu;
    cpu.pc = 0xA000_addr;


    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0xA000_addr)},  // Zero immediate value
    };

    CHECK(executeAddress(cpu, Immediate::ops, cycles));
  }
}

TEST_CASE("IndirectZeroPageX addressing mode", "[addressing]")
{
  State cpu;
  cpu.pc = 0x1000_addr;
  cpu.x = 0x02;

  auto request = IndirectZeroPageX::ops[0](cpu, BusResponse{});
  CHECK(request.request == BusRequest::Read(0x1000_addr));

  // Read the zero page address (from the byte after the instruction)
  request = IndirectZeroPageX::ops[1](cpu, BusResponse{0x40});

  // First read is from the unmodified zero page address
  CHECK(request.request == BusRequest::Read(0x0040_addr));

  // Second read is from the zero page address + X (0x40 + 0x02 = 0x42)
  request = IndirectZeroPageX::ops[2](cpu, BusResponse{0x99});  // random data
  CHECK(request.request == BusRequest::Read(0x0042_addr));

  // Lo byte should be set from response data
  request = request.injection(cpu, BusResponse{0x20});  // low byte of effective address
  CHECK(request.request == BusRequest::Read(0x0043_addr));  // read high byte from next address

  request = IndirectZeroPageX::ops[3](cpu, BusResponse{0x80});  // high byte of effective address
  CHECK(request.request == BusRequest::Read(0x8020_addr));  // read operand
}

TEST_CASE("IndirectZeroPageY addressing mode", "[addressing]")
{
  State cpu;
  cpu.pc = 0x1000_addr;
  cpu.y = 0x02;

  auto request = IndirectZeroPageY::ops[0](cpu, BusResponse{});
  CHECK(request.request == BusRequest::Read(0x1000_addr));

  // Read the zero page address (from the byte after the instruction)
  request = IndirectZeroPageY::ops[1](cpu, BusResponse{0x40});
  // Read the pointer's low byte from the zero page address
  CHECK(request.request == BusRequest::Read(0x0040_addr));

  // Lo byte should be set from response data
  request = request.injection(cpu, BusResponse{0x20});  // low byte of effective address
  // Next read is for the high byte of the pointer (from next zero page address)
  CHECK(request.request == BusRequest::Read(0x0041_addr));

  request = IndirectZeroPageY::ops[2](cpu, BusResponse{0x80});  // high byte of effective address
  // Final read is from the effective address + Y (0x8020 + 0x02 = 0x8022)
  CHECK(request.request == BusRequest::Read(0x8022_addr));  // read operand
}
