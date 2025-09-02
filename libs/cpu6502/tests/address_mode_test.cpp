#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <format>
#include <fstream>
#include <span>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "cpu6502/address_mode.h"
#include "cpu6502/state.h"
#include "helpers.h"

using namespace Common;
using namespace cpu6502;

static bool operator==(const State& lhs, const State& rhs) noexcept
{
  return lhs.pc == rhs.pc && lhs.a == rhs.a && lhs.x == rhs.x && lhs.y == rhs.y && lhs.sp == rhs.sp && lhs.p == rhs.p &&
         lhs.hi == rhs.hi && lhs.lo == rhs.lo;
}

TEST_CASE("requestOperandLow", "[addressing]")
{
  State cpu{.pc = 0x8001_addr, .hi = 0xFF, .lo = 0xEE};  // Set hi/lo to ensure they are changed

  auto request = AddressMode::requestOperandLow(cpu, BusResponse{});
  CHECK(request.request == BusRequest::Read(0x8001_addr));

  // Only the PC should be incremented
  CHECK((cpu == State{.pc = 0x8002_addr}));
}

TEST_CASE("requestOperandHigh", "[addressing]")
{
  State cpu{.pc = 0x8002_addr};

  auto request = AddressMode::requestOperandHigh(cpu, BusResponse{0x34});
  CHECK(request.request == BusRequest::Read(0x8002_addr));

  // Only the PC, and the lo byte should be set
  CHECK((cpu == State{.pc = 0x8003_addr, .lo = 0x34}));
}

TEST_CASE("requestAddress", "[addressing]")
{
  State cpu{.pc = 0x8003_addr, .lo = 0x34};

  auto request = Absolute<>::requestAddress(cpu, BusResponse{0x12});
  CHECK(request.request == BusRequest::Read(0x1234_addr));

  // Only the PC, the lo byte, and the hi byte should be set
  CHECK((cpu == State{.pc = 0x8003_addr, .hi = 0x12, .lo = 0x34}));
}

// Helper to select register for template test case
template<int N>
struct RegType
{
};
using XReg = RegType<0>;
using YReg = RegType<1>;


TEMPLATE_TEST_CASE("Absolute::requestAddress", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Basic offset - no page crossing")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x34};
    (cpu.*reg) = 0x01;

    auto request = Absolute<reg>::requestAddress(cpu, BusResponse{0x12});
    CHECK(request.request == BusRequest::Read(0x1235_addr));

    State expected{.pc = 0x8003_addr, .hi = 0x12, .lo = 0x35};
    (expected.*reg) = 0x01;
    CHECK((cpu == expected));
  }

  SECTION("Page boundary crossing - forward overflow")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x80};
    (cpu.*reg) = 0xFF;

    auto request = Absolute<reg>::requestAddress(cpu, BusResponse{0x12});

    // Should read from wrong address first (0x127F instead of 0x137F)
    CHECK(request.request == BusRequest::Read(0x127F_addr));
    CHECK(request.injection != nullptr);  // Page boundary fix scheduled

    // Hi byte should be corrected for next cycle, lo byte updated
    CHECK(cpu.hi == 0x13);  // Corrected from 0x12 to 0x13
    CHECK(cpu.lo == 0x7F);  // 0x80 + 0xFF = 0x17F, low byte = 0x7F

    // Test the lambda that fixes page boundary
    auto nextRequest = request.injection(cpu, BusResponse{0x00});  // Dummy response
    CHECK(nextRequest.request == BusRequest::Read(0x137F_addr));  // Correct address
  }

  SECTION("Page boundary crossing - maximum offset")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0xFF};
    (cpu.*reg) = 0xFF;

    auto request = Absolute<reg>::requestAddress(cpu, BusResponse{0x12});

    // Should read from wrong address first (0x12FE instead of 0x13FE)
    CHECK(request.request == BusRequest::Read(0x12FE_addr));
    CHECK(request.injection != nullptr);  // Page boundary fix scheduled

    // Hi byte corrected, lo byte wrapped
    CHECK(cpu.hi == 0x13);  // Corrected from 0x12 to 0x13
    CHECK(cpu.lo == 0xFE);  // 0xFF + 0xFF = 0x1FE, low byte = 0xFE
  }

  SECTION("No page boundary crossing - near boundary")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x80};
    (cpu.*reg) = 0x7F;

    auto request = Absolute<reg>::requestAddress(cpu, BusResponse{0x12});

    // Should read correct address immediately (no overflow)
    CHECK(request.request == BusRequest::Read(0x12FF_addr));
    CHECK(request.injection == nullptr);

    // No correction needed
    CHECK(cpu.hi == 0x12);  // Unchanged
    CHECK(cpu.lo == 0xFF);  // 0x80 + 0x7F = 0xFF
  }
}

TEMPLATE_TEST_CASE("absolute-indexed", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Basic offset - no page crossing")
  {
    State cpu{.pc = 0x0800_addr};
    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x34, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x1235_addr)},
    };
    CHECK(execute(cpu, Absolute<reg>::ops, cycles));
  }

  SECTION("Page boundary crossing - forward overflow")
  {
    State cpu{.pc = 0x800_addr};
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
    CHECK(execute(cpu, Absolute<reg>::ops, cycles));
  }

  SECTION("Page boundary crossing - maximum offset")
  {
    State cpu{.pc = 0x0800_addr};
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
    CHECK(execute(cpu, Absolute<reg>::ops, cycles));
  }

  SECTION("No page boundary crossing - near boundary")
  {
    State cpu{.pc = 0x0800_addr};
    (cpu.*reg) = 0x7F;

    Cycle cycles[] = {
        // Step 0: Read low byte of address
        {0x00, BusRequest::Read(0x0800_addr)},
        // Step 1: Read high byte of address
        {0x80, BusRequest::Read(0x0801_addr)},
        // Step 2: Add index register to low byte and read from effective address
        {0x12, BusRequest::Read(0x12FF_addr)},
    };
    CHECK(execute(cpu, Absolute<reg>::ops, cycles));
  }
}

TEST_CASE("requestZeroPageAddress", "[addressing]")
{
  State cpu{.hi = 0x00};  // Hi byte should already be 0 from requestOperandLow
  auto request = ZeroPage<>::requestAddress(cpu, BusResponse{0x42});
  CHECK(request.request == BusRequest::Read(0x0042_addr));  // Zero page address formed from response

  // Lo byte should be set from response data
  CHECK((cpu == State{.hi = 0x00, .lo = 0x42}));
}

TEMPLATE_TEST_CASE("requestZeroPageAddressIndexed", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Normal addition within zero page")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x10;

    auto request = ZeroPage<reg>::requestAddress(cpu, BusResponse{0x80});
    // first read is done without adding the index
    CHECK(request.request == BusRequest::Read(0x0080_addr));

    // Second read should be correct, $80 + $10 = $90
    request = ZeroPage<reg>::requestAddressAfterIndexing(cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x0090_addr));

    State expected{.hi = 0x00, .lo = 0x90};
    (expected.*reg) = 0x10;
    CHECK((cpu == expected));
  }

  SECTION("Overflow wraps within zero page")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x10;

    // first read is done without adding the index
    auto request = ZeroPage<reg>::requestAddress(cpu, BusResponse{0xF8});
    CHECK(request.request == BusRequest::Read(0x00F8_addr));

    // Second read should wrap around, $F8 + $10 = $108, wraps to $08
    request = ZeroPage<reg>::requestAddressAfterIndexing(cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x0008_addr));

    State expected{.hi = 0x00, .lo = 0x08};
    (expected.*reg) = 0x10;
    CHECK((cpu == expected));
  }

  SECTION("Maximum overflow")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0xFF;

    // first read is done without adding the index
    auto request = ZeroPage<reg>::requestAddress(cpu, BusResponse{0xFF});
    CHECK(request.request == BusRequest::Read(0x00FF_addr));

    // Second read should wrap around, $FF + $FF = $1FE, wraps to $FE
    request = ZeroPage<reg>::requestAddressAfterIndexing(cpu, BusResponse{0x80});
    CHECK(request.request == BusRequest::Read(0x00FE_addr));

    State expected{.hi = 0x00, .lo = 0xFE};
    (expected.*reg) = 0xFF;
    CHECK((cpu == expected));
  }

  SECTION("No offset - register is zero")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x00;

    auto request = ZeroPage<reg>::requestAddress(cpu, BusResponse{0x42});
    CHECK(request.request == BusRequest::Read(0x0042_addr));  // $42 + $00 = $42

    State expected{.hi = 0x00, .lo = 0x42};
    (expected.*reg) = 0x00;
    CHECK((cpu == expected));
  }
}

TEST_CASE("Zero page addressing mode complete sequence", "[addressing]")
{
  SECTION("Basic zero page read")
  {
    State cpu{.pc = 0x8001_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // requestOperandLow: dummy input, request operand, set hi=0
        {0x42, Common::BusRequest::Read(0x0042_addr)},  // requestZeroPageAddress: receive ZP addr (0x42), request
                                                        // from $0042
    };

    CHECK(execute(cpu, ZeroPage<>::ops, cycles));

    // Verify final state after addressing mode complete
    CHECK(cpu.pc == 0x8002_addr);  // PC incremented by requestOperandLow
    CHECK(cpu.hi == 0x00);  // High byte set to 0 for zero page
    CHECK(cpu.lo == 0x42);  // Low byte contains zero page address
  }

  SECTION("Zero page address 0x00")
  {
    State cpu{.pc = 0x9000_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x9000_addr)},  // Request operand
        {0x00, Common::BusRequest::Read(0x0000_addr)},  // Request from address $00
    };

    CHECK(execute(cpu, ZeroPage<>::ops, cycles));

    CHECK(cpu.pc == 0x9001_addr);
    CHECK(cpu.hi == 0x00);
    CHECK(cpu.lo == 0x00);
  }

  SECTION("Zero page address 0xFF")
  {
    State cpu{.pc = 0xA000_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0xA000_addr)},  // Request operand
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Request from address $FF
    };

    CHECK(execute(cpu, ZeroPage<>::ops, cycles));

    CHECK(cpu.pc == 0xA001_addr);
    CHECK(cpu.hi == 0x00);
    CHECK(cpu.lo == 0xFF);
  }

  SECTION("Multiple sequential zero page operations")
  {
    State cpu{.pc = 0x8000_addr};

    // First zero page access
    Cycle cycles1[] = {
        {0x00, Common::BusRequest::Read(0x8000_addr)},
        {0x10, Common::BusRequest::Read(0x0010_addr)},
    };

    CHECK(execute(cpu, ZeroPage<>::ops, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x10);

    // Second zero page access (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},
        {0x20, Common::BusRequest::Read(0x0020_addr)},
    };

    CHECK(execute(cpu, ZeroPage<>::ops, cycles2));
    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x20);
  }
}

TEMPLATE_TEST_CASE("Zero page indexed addressing mode complete sequence", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Basic zero page indexed read - no wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x05;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // requestOperandLow: request operand, set hi=0
        {0x40, Common::BusRequest::Read(0x0040_addr)},  // requestAddress: receive 0x40, request from $40
        {0x40, Common::BusRequest::Read(0x0045_addr)},  // requestAddressAfterIndexing: receive 0x40, add reg (0x05),
                                                        // request from $45
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x45};
    (expected.*reg) = 0x05;
    CHECK((cpu == expected));
  }

  SECTION("Zero page indexed with wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x10;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // Request operand
        {0xF8, Common::BusRequest::Read(0x00F8_addr)},  // Read from address before adding index
        {0x99, Common::BusRequest::Read(0x0008_addr)},  // 0xF8 + 0x10 = 0x108, wraps to 0x08
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x08};
    (expected.*reg) = 0x10;
    CHECK((cpu == expected));
  }

  SECTION("Zero page indexed maximum wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Reads from address before adding index
        {0xBB, Common::BusRequest::Read(0x00FE_addr)},  // 0xFF + 0xFF = 0x1FE, wraps to 0xFE
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0xFE};
    (expected.*reg) = 0xFF;
    CHECK((cpu == expected));
  }

  SECTION("Zero page indexed with register=0 (no offset)")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x00;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0x42, Common::BusRequest::Read(0x0042_addr)},  // Still reads from address before indexing
        {0x33, Common::BusRequest::Read(0x0042_addr)},  // Reads from address after adding index (no change)
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x42};
    (expected.*reg) = 0x00;
    CHECK((cpu == expected));
  }

  SECTION("Zero page indexed edge case - wrapping to start of zero page")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0xFF, Common::BusRequest::Read(0x00FF_addr)},  // Reads from address before adding index
        {0x11, Common::BusRequest::Read(0x0000_addr)},  // 0xFF + 0x01 = 0x100, wraps to 0x00
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x00};
    (expected.*reg) = 0x01;
    CHECK((cpu == expected));
  }

  SECTION("Sequential zero page indexed operations")
  {
    State cpu{.pc = 0x8000_addr};
    (cpu.*reg) = 0x02;

    // First operation
    Cycle cycles1[] = {
        {0x00, Common::BusRequest::Read(0x8000_addr)},  //
        {0x10, Common::BusRequest::Read(0x0010_addr)},  // Read from address before adding index
        {0xFF, Common::BusRequest::Read(0x0012_addr)},  // 0x10 + 0x02 = 0x12
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x12);

    // Second operation (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  //
        {0x20, Common::BusRequest::Read(0x0020_addr)},  // Read from address before adding index
        {0xEF, Common::BusRequest::Read(0x0022_addr)},  // 0x20 + 0x02 = 0x22
    };

    CHECK(execute(cpu, ZeroPage<reg>::ops, cycles2));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x22};
    (expected.*reg) = 0x02;
    CHECK((cpu == expected));
  }
}

TEST_CASE("Immediate addressing mode complete sequence", "[addressing]")
{
  SECTION("Basic immediate mode")
  {
    State cpu{.pc = 0x8001_addr};

    Cycle cycles[] = {
        {0x42, Common::BusRequest::Read(0x8001_addr)},  // requestOperandLow: receive immediate value (0x42)
    };

    CHECK(execute(cpu, Immediate::ops, cycles));

    // Verify final state
    CHECK(cpu.pc == 0x8002_addr);  // PC incremented
    CHECK(cpu.hi == 0x00);  // Hi byte cleared (for zero page compatibility)
  }

  SECTION("Immediate mode with different values")
  {
    State cpu{.pc = 0x9000_addr};

    Cycle cycles[] = {
        {0xFF, Common::BusRequest::Read(0x9000_addr)},  // Maximum 8-bit value
    };

    CHECK(execute(cpu, Immediate::ops, cycles));
  }

  SECTION("Immediate mode with zero")
  {
    State cpu{.pc = 0xA000_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0xA000_addr)},  // Zero immediate value
    };

    CHECK(execute(cpu, Immediate::ops, cycles));
  }
}
