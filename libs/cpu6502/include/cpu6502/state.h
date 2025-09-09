#pragma once

#include "common/address.h"
#include "common/bus.h"

namespace cpu6502
{

struct VisibleState
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
};

struct State : VisibleState
{
  // Storage for low byte of address during addressing modes
  Common::Byte lo = 0;
  // Storage for high byte of address during addressing modes
  Common::Byte hi = 0;
  // Storage for operand during instruction execution
  Common::Byte operand = 0;

  // PC at the start of the next cycle
  Common::Address next_pc{0};
};

constexpr bool VisibleState::has(Flag f) const noexcept
{
  return (p & static_cast<uint8_t>(f)) != 0;
}

constexpr void VisibleState::set(Flag f, bool v) noexcept
{
  p = v ? (p | static_cast<uint8_t>(f)) : (p & ~static_cast<uint8_t>(f));
}

constexpr void VisibleState::setZN(Common::Byte v) noexcept
{
  set(Flag::Zero, v == 0);
  set(Flag::Negative, (v & 0x80) != 0);
}

// If you ever assign p wholesale, re-assert U:
constexpr void VisibleState::assignP(Common::Byte v) noexcept
{
  p = Common::Byte((v | static_cast<uint8_t>(Flag::Unused)));
}

}  // namespace cpu6502
