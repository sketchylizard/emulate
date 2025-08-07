#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte
#include <cstdint>  // for std::uint8_t
#include <utility>  // for std::pair

#include "cpu/ArithmeticLogicUnit.h"

using ALU = ArithmeticLogicUnit;
using Flags = ArithmeticFlags;

constexpr std::byte b(uint8_t v)
{
  return static_cast<std::byte>(v);
}

constexpr bool operator==(const std::pair<std::byte, Flags>& lhs, const std::pair<std::byte, Flags>& rhs)
{
  return lhs.first == rhs.first && lhs.second == rhs.second;
}

TEST_CASE("ArithmeticLogicUnit: Basic Operations", "[alu]")
{
  // clang-format off

  // Test ArithmeticFlags
  static_assert(Flags{} == Flags{0b0000}, "Default flags should be zero");
  static_assert(Flags{0b0001} == CarryFlag, "Carry flag should be set correctly");
  static_assert(Flags{0b0010} == ZeroFlag, "Zero flag should be set correctly");
  static_assert(Flags{0b0100} == NegativeFlag, "Negative flag should be set correctly");
  static_assert(Flags{0b1000} == OverflowFlag, "Overflow flag should be set correctly");

  // Arithmetic tests
  static_assert(ALU::adc(b(0x01), b(0x01), false) == std::pair{b(0x02), Flags{}}, "ADC 0x01 + 0x01 = 0x02");
  static_assert(ALU::adc(b(0xFF), b(0x01), false) == std::pair{b(0x00), CarryFlag | ZeroFlag}, "ADC 0xFF + 0x01 = 0x00");
  static_assert(ALU::adc(b(0x7F), b(0x01), false) == std::pair{b(0x80), OverflowFlag | NegativeFlag}, "ADC signed overflow");
  static_assert(ALU::adc(b(0xFE), b(0x01), true) == std::pair{b(0x00), CarryFlag | ZeroFlag}, "ADC with carry-in causes overflow");

  static_assert(ALU::sbc(b(0x03), b(0x01), true) == std::pair{b(0x02), CarryFlag}, "SBC 3 - 1");
  static_assert(ALU::sbc(b(0x00), b(0x01), false) == std::pair{b(0xFE), NegativeFlag}, "SBC underflow");
  static_assert(ALU::sbc(b(0x01), b(0x01), true) == std::pair{b(0x00), CarryFlag | ZeroFlag}, "SBC equal operands");
  static_assert(ALU::sbc(b(0x01), b(0x02), true) == std::pair{b(0xFF), NegativeFlag}, "SBC with borrow");

  // Logic tests
  static_assert(ALU::bitwise_and(b(0xF0), b(0x0F)) == std::pair{b(0x00), ZeroFlag}, "AND clears");
  static_assert(ALU::bitwise_or(b(0xF0), b(0x0F)) == std::pair{b(0xFF), NegativeFlag}, "OR sets all");
  static_assert(ALU::bitwise_xor(b(0xFF), b(0x0F)) == std::pair{b(0xF0), NegativeFlag}, "XOR pattern");
  static_assert(ALU::bitwise_xor(b(0x55), b(0x55)) == std::pair{b(0x00), ZeroFlag}, "XOR same values");

  // Shift tests
  static_assert(ALU::asl(b(0x40)) == std::pair{b(0x80), NegativeFlag}, "ASL shift left");
  static_assert(ALU::lsr(b(0x01)) == std::pair{b(0x00), ZeroFlag | CarryFlag}, "LSR shift to 0");
  static_assert(ALU::rol(b(0x80), false) == std::pair{b(0x00), ZeroFlag | CarryFlag}, "ROL high bit to carry");
  static_assert(ALU::ror(b(0x01), false) == std::pair{b(0x00), ZeroFlag | CarryFlag}, "ROR low bit to carry");
  static_assert(ALU::rol(b(0x00), true) == std::pair{b(0x01), Flags{}}, "ROL 0x00 with carry in");
  static_assert(ALU::ror(b(0x00), true) == std::pair{b(0x80), NegativeFlag}, "ROR 0x00 with carry in");

  // clang-format on
}

TEST_CASE("ArithmeticLogicUnit: Overflow Detection", "[alu][overflow]")
{
  // clang-format off

  static_assert(ALU::adc(b(0x50), b(0x10), false) == std::pair{b(0x60), Flags{}}, "0x50 + 0x10 → 0x60, no flags");
  static_assert(ALU::adc(b(0x50), b(0x50), false) == std::pair{b(0xA0), OverflowFlag | NegativeFlag}, "0x50 + 0x50 → 0xA0, overflow + negative");
  static_assert(ALU::adc(b(0x90), b(0x90), false) == std::pair{b(0x20), OverflowFlag | CarryFlag}, "0x90 + 0x90 → 0x20, overflow + carry");  // but not zero or negative
  static_assert(ALU::adc(b(0x90), b(0x10), false) == std::pair{b(0xA0), NegativeFlag}, "0x90 + 0x10 → 0xA0, negative only");
  static_assert(ALU::adc(b(0xFF), b(0x01), false) == std::pair{b(0x00), CarryFlag | ZeroFlag}, "0xFF + 0x01 → 0x00, carry + zero");
  static_assert(ALU::adc(b(0x7F), b(0x01), false) == std::pair{b(0x80), OverflowFlag | NegativeFlag}, "0x7F + 0x01 → 0x80, overflow + negative");
  static_assert(ALU::adc(b(0x80), b(0xFF), false) == std::pair{b(0x7F), OverflowFlag | CarryFlag}, "0x80 + 0xFF → 0x7F, overflow + carry");
  static_assert(ALU::adc(b(0x00), b(0x00), false) == std::pair{b(0x00), ZeroFlag}, "0x00 + 0x00 → 0x00, zero only");

  // clang-format on
}