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

TEST_CASE("requestOpcode", "[addressing]")
{
  State cpu{.pc = 0x8000_addr};

  auto request = AddressMode::requestOpcode(cpu, BusResponse{});
  CHECK(request.address == 0x8000_addr);
  CHECK(request.isRead());
  CHECK(request.isSync());

  // Only the PC should be incremented
  CHECK(cpu == State{.pc = 0x8001_addr});
}

TEST_CASE("requestOperandLow", "[addressing]")
{
  State cpu{.pc = 0x8001_addr, .hi = 0xFF, .lo = 0xEE};  // Set hi/lo to ensure they are changed

  auto request = AddressMode::requestOperandLow(cpu, BusResponse{});
  CHECK(request.address == 0x8001_addr);
  CHECK(request.isRead());
  CHECK_FALSE(request.isSync());

  // Only the PC should be incremented
  CHECK(cpu == State{.pc = 0x8002_addr});
}

TEST_CASE("requestOperandHigh", "[addressing]")
{
  State cpu{.pc = 0x8002_addr};

  auto request = AddressMode::requestOperandHigh(cpu, BusResponse{0x34});
  CHECK(request.address == 0x8002_addr);
  CHECK(request.isRead());
  CHECK_FALSE(request.isSync());

  // Only the PC, and the lo byte should be set
  CHECK(cpu == State{.pc = 0x8003_addr, .lo = 0x34});
}

TEST_CASE("requestEffectiveAddress", "[addressing]")
{
  State cpu{.pc = 0x8003_addr, .lo = 0x34};

  auto request = AddressMode::requestEffectiveAddress(cpu, BusResponse{0x12});
  CHECK(request.address == 0x1234_addr);
  CHECK(request.isRead());
  CHECK_FALSE(request.isSync());

  // Only the PC, the lo byte, and the hi byte should be set
  CHECK(cpu == State{.pc = 0x8003_addr, .hi = 0x12, .lo = 0x34});
}

// Helper to select register for template test case
template<int N>
struct RegType
{
};
using XReg = RegType<0>;
using YReg = RegType<1>;


TEMPLATE_TEST_CASE("requestEffectiveAddressIndexed", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Basic offset - no page crossing")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x34};
    (cpu.*reg) = 0x01;

    auto request = AddressMode::requestEffectiveAddressIndexed<reg>(cpu, BusResponse{0x12});
    CHECK(request.address == 0x1235_addr);
    CHECK(request.isRead());
    CHECK_FALSE(request.isSync());

    State expected{.pc = 0x8003_addr, .hi = 0x12, .lo = 0x35};
    (expected.*reg) = 0x01;
    CHECK(cpu == expected);
  }

  SECTION("Page boundary crossing - forward overflow")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x80};
    (cpu.*reg) = 0xFF;

    auto request = AddressMode::requestEffectiveAddressIndexed<reg>(cpu, BusResponse{0x12});

    // Should read from wrong address first (0x127F instead of 0x137F)
    CHECK(request.address == 0x127F_addr);
    CHECK(request.isRead());
    CHECK_FALSE(request.isSync());

    // Hi byte should be corrected for next cycle, lo byte updated
    CHECK(cpu.hi == 0x13);  // Corrected from 0x12 to 0x13
    CHECK(cpu.lo == 0x7F);  // 0x80 + 0xFF = 0x17F, low byte = 0x7F
    CHECK(cpu.next != nullptr);  // Page boundary fix scheduled

    // Test the lambda that fixes page boundary
    auto nextRequest = cpu.next(cpu, BusResponse{0x00});  // Dummy response
    CHECK(nextRequest.address == 0x137F_addr);  // Correct address
    CHECK(nextRequest.isRead());
  }

  SECTION("Page boundary crossing - maximum offset")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0xFF};
    (cpu.*reg) = 0xFF;

    auto request = AddressMode::requestEffectiveAddressIndexed<reg>(cpu, BusResponse{0x12});

    // Should read from wrong address first (0x12FE instead of 0x13FE)
    CHECK(request.address == 0x12FE_addr);
    CHECK(request.isRead());

    // Hi byte corrected, lo byte wrapped
    CHECK(cpu.hi == 0x13);  // Corrected from 0x12 to 0x13
    CHECK(cpu.lo == 0xFE);  // 0xFF + 0xFF = 0x1FE, low byte = 0xFE
    CHECK(cpu.next != nullptr);
  }

  SECTION("No page boundary crossing - near boundary")
  {
    State cpu{.pc = 0x8003_addr, .lo = 0x80};
    (cpu.*reg) = 0x7F;

    auto request = AddressMode::requestEffectiveAddressIndexed<reg>(cpu, BusResponse{0x12});

    // Should read correct address immediately (no overflow)
    CHECK(request.address == 0x12FF_addr);
    CHECK(request.isRead());

    // No correction needed
    CHECK(cpu.hi == 0x12);  // Unchanged
    CHECK(cpu.lo == 0xFF);  // 0x80 + 0x7F = 0xFF
    CHECK(cpu.next == nullptr);  // No extra cycle needed
  }
}

TEMPLATE_TEST_CASE("absolute-indexed", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  const auto& absoluteIndexed = std::is_same_v<TestType, XReg> ? AddressMode::absoluteX : AddressMode::absoluteY;

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
    CHECK(execute(cpu, absoluteIndexed, cycles));
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
    CHECK(execute(cpu, absoluteIndexed, cycles));
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
    CHECK(execute(cpu, absoluteIndexed, cycles));
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
    CHECK(execute(cpu, absoluteIndexed, cycles));
  }
}

TEST_CASE("requestZeroPageAddress", "[addressing]")
{
  State cpu{.hi = 0x00};  // Hi byte should already be 0 from requestOperandLow
  auto request = AddressMode::requestZeroPageAddress(cpu, BusResponse{0x42});
  CHECK(request.address == 0x0042_addr);  // Zero page address formed from response
  CHECK(request.isRead());
  CHECK_FALSE(request.isSync());

  // Lo byte should be set from response data
  CHECK(cpu == State{.hi = 0x00, .lo = 0x42});
}

TEMPLATE_TEST_CASE("requestZeroPageAddressIndexed", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  SECTION("Normal addition within zero page")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x10;

    auto request = AddressMode::requestZeroPageAddressIndexed<reg>(cpu, BusResponse{0x80});
    CHECK(request.address == 0x0090_addr);  // $80 + $10 = $90
    CHECK(request.isRead());
    CHECK_FALSE(request.isSync());

    State expected{.hi = 0x00, .lo = 0x90};
    (expected.*reg) = 0x10;
    CHECK(cpu == expected);
  }

  SECTION("Overflow wraps within zero page")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x10;

    auto request = AddressMode::requestZeroPageAddressIndexed<reg>(cpu, BusResponse{0xF8});
    CHECK(request.address == 0x0008_addr);  // $F8 + $10 = $108, wraps to $08
    CHECK(request.isRead());
    CHECK_FALSE(request.isSync());

    State expected{.hi = 0x00, .lo = 0x08};
    (expected.*reg) = 0x10;
    CHECK(cpu == expected);
  }

  SECTION("Maximum overflow")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0xFF;

    auto request = AddressMode::requestZeroPageAddressIndexed<reg>(cpu, BusResponse{0xFF});
    CHECK(request.address == 0x00FE_addr);  // $FF + $FF = $1FE, wraps to $FE
    CHECK(request.isRead());
    CHECK_FALSE(request.isSync());

    State expected{.hi = 0x00, .lo = 0xFE};
    (expected.*reg) = 0xFF;
    CHECK(cpu == expected);
  }

  SECTION("No offset - register is zero")
  {
    State cpu{.hi = 0x00};
    (cpu.*reg) = 0x00;

    auto request = AddressMode::requestZeroPageAddressIndexed<reg>(cpu, BusResponse{0x42});
    CHECK(request.address == 0x0042_addr);  // $42 + $00 = $42
    CHECK(request.isRead());

    State expected{.hi = 0x00, .lo = 0x42};
    (expected.*reg) = 0x00;
    CHECK(cpu == expected);
  }
}

TEST_CASE("Zero page addressing mode complete sequence", "[addressing]")
{
  SECTION("Basic zero page read")
  {
    State cpu{.pc = 0x8001_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // requestOperandLow: dummy input, request operand, set hi=0
        {0x42, Common::BusRequest::Read(0x0042_addr)},  // requestZeroPageAddress: receive ZP addr (0x42), request from
                                                        // $0042
    };

    CHECK(execute(cpu, AddressMode::zeroPage, cycles));

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

    CHECK(execute(cpu, AddressMode::zeroPage, cycles));

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

    CHECK(execute(cpu, AddressMode::zeroPage, cycles));

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

    CHECK(execute(cpu, AddressMode::zeroPage, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x10);

    // Second zero page access (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},
        {0x20, Common::BusRequest::Read(0x0020_addr)},
    };

    CHECK(execute(cpu, AddressMode::zeroPage, cycles2));
    CHECK(cpu.pc == 0x8002_addr);
    CHECK(cpu.lo == 0x20);
  }
}

TEMPLATE_TEST_CASE("Zero page indexed addressing mode complete sequence", "[addressing]", XReg, YReg)
{
  constexpr auto reg = std::is_same_v<TestType, XReg> ? &State::x : &State::y;

  const auto& zeroPageIndexed = std::is_same_v<TestType, XReg> ? AddressMode::zeroPageX : AddressMode::zeroPageY;

  SECTION("Basic zero page indexed read - no wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x05;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // requestOperandLow: request operand, set hi=0
        {0x40, Common::BusRequest::Read(0x0045_addr)},  // addZeroPageOffset: receive 0x40, add reg (0x05), request from
                                                        // $45
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x45};
    (expected.*reg) = 0x05;
    CHECK(cpu == expected);
  }

  SECTION("Zero page indexed with wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x10;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)},  // Request operand
        {0xF8, Common::BusRequest::Read(0x0008_addr)},  // 0xF8 + 0x10 = 0x108, wraps to 0x08
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x08};
    (expected.*reg) = 0x10;
    CHECK(cpu == expected);
  }

  SECTION("Zero page indexed maximum wrapping")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0xFF;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)}, {0xFF, Common::BusRequest::Read(0x00FE_addr)},  // 0xFF + 0xFF =
                                                                                                       // 0x1FE, wraps
                                                                                                       // to 0xFE
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0xFE};
    (expected.*reg) = 0xFF;
    CHECK(cpu == expected);
  }

  SECTION("Zero page indexed with register=0 (no offset)")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x00;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)}, {0x42, Common::BusRequest::Read(0x0042_addr)},  // 0x42 + 0x00 =
                                                                                                       // 0x42
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x42};
    (expected.*reg) = 0x00;
    CHECK(cpu == expected);
  }

  SECTION("Zero page indexed edge case - wrapping to start of zero page")
  {
    State cpu{.pc = 0x8001_addr};
    (cpu.*reg) = 0x01;

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)}, {0xFF, Common::BusRequest::Read(0x0000_addr)},  // 0xFF + 0x01 =
                                                                                                       // 0x100, wraps
                                                                                                       // to 0x00
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x00};
    (expected.*reg) = 0x01;
    CHECK(cpu == expected);
  }

  SECTION("Sequential zero page indexed operations")
  {
    State cpu{.pc = 0x8000_addr};
    (cpu.*reg) = 0x02;

    // First operation
    Cycle cycles1[] = {
        {0x00, Common::BusRequest::Read(0x8000_addr)}, {0x10, Common::BusRequest::Read(0x0012_addr)},  // 0x10 + 0x02 =
                                                                                                       // 0x12
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles1));
    CHECK(cpu.pc == 0x8001_addr);
    CHECK(cpu.lo == 0x12);

    // Second operation (PC should continue from where it left off)
    Cycle cycles2[] = {
        {0x00, Common::BusRequest::Read(0x8001_addr)}, {0x20, Common::BusRequest::Read(0x0022_addr)},  // 0x20 + 0x02 =
                                                                                                       // 0x22
    };

    CHECK(execute(cpu, zeroPageIndexed, cycles2));

    State expected{.pc = 0x8002_addr, .hi = 0x00, .lo = 0x22};
    (expected.*reg) = 0x02;
    CHECK(cpu == expected);
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

    CHECK(execute(cpu, AddressMode::immediate, cycles));

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

    CHECK(execute(cpu, AddressMode::immediate, cycles));
  }

  SECTION("Immediate mode with zero")
  {
    State cpu{.pc = 0xA000_addr};

    Cycle cycles[] = {
        {0x00, Common::BusRequest::Read(0xA000_addr)},  // Zero immediate value
    };

    CHECK(execute(cpu, AddressMode::immediate, cycles));
  }
}
