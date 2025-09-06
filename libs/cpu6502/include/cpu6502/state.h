#pragma once

#include "common/address.h"
#include "common/bus.h"

namespace cpu6502
{

struct State
{
  enum class Flag : Common::Byte
  {
    Carry = 0x01,
    Zero = 0x02,
    Interrupt = 0x04,
    Decimal = 0x08,
    Break = 0x10,
    Unused = 0x20,  // "U" bit, set when pushed
    Overflow = 0x40,
    Negative = 0x80,
  };

  enum class AddressModeType : uint8_t
  {
    // The address mode value is designed so that the number of operand bytes can be determined by
    // simple ranges. Dividing the ordinal number by 10 gives the number of operand bytes.

    // The following modes don't read any extra bytes:
    Implied = 0,
    Accumulator,

    // The following modes read one extra byte:
    Immediate = 1 * 10,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Relative,
    IndirectZpX,
    IndirectZpY,

    // The following modes read two extra bytes:
    Absolute = 2 * 10,
    AbsoluteX,
    AbsoluteY,
    Indirect,
  };

  // Flag helpers
  [[nodiscard]] constexpr bool has(Flag f) const noexcept;
  constexpr void set(Flag f, bool v) noexcept;
  constexpr void setZN(Common::Byte v) noexcept;
  constexpr void assignP(Common::Byte v) noexcept;

  Common::Address pc{0};
  Common::Byte a{0};
  Common::Byte x{0};
  Common::Byte y{0};
  Common::Byte sp{0};
  Common::Byte p{static_cast<Common::Byte>(Flag::Unused)};  // keep U set

  // Storage for low byte of address during addressing modes
  Common::Byte lo = 0;
  // Storage for high byte of address during addressing modes
  Common::Byte hi = 0;
  // Storage for operand during instruction execution
  Common::Byte operand = 0;
};

constexpr bool State::has(Flag f) const noexcept
{
  return (p & static_cast<uint8_t>(f)) != 0;
}

constexpr void State::set(Flag f, bool v) noexcept
{
  p = v ? (p | static_cast<uint8_t>(f)) : (p & ~static_cast<uint8_t>(f));
}

constexpr void State::setZN(Common::Byte v) noexcept
{
  set(Flag::Zero, v == 0);
  set(Flag::Negative, (v & 0x80) != 0);
}

// If you ever assign p wholesale, re-assert U:
constexpr void State::assignP(Common::Byte v) noexcept
{
  p = Common::Byte((v | static_cast<uint8_t>(Flag::Unused)));
}

}  // namespace cpu6502
