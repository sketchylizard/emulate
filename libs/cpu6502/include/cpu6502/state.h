#pragma once

#include "common/address.h"

namespace Common
{
struct BusRequest;
struct BusResponse;
}  // namespace Common

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

  //! State functions should accept a bus response from the previous clock tick and return a new bus
  //! request for the new state. It can insert extra cycles into the instruction stream by setting
  //! the 'next' member to another function. If 'next' is null, the operation is complete and the
  //! next instruction will be taken from the instruction stream, or a new instruction will be
  //! fetched.
  using Microcode = Common::BusRequest (*)(State&, Common::BusResponse response);

  // clang-format off
  enum class AddressModeType : uint8_t {
    Implied, Accumulator, Immediate, 
    ZeroPage, ZeroPageX, ZeroPageY, 
    Absolute, AbsoluteX, AbsoluteY,
    Indirect, IndirectZpX, IndirectZpY,
    Relative
  };
  // clang-format on

  struct Instruction
  {
    Common::Byte opcode = 0;
    const char* mnemonic = "???";
    AddressModeType addressMode = AddressModeType::Implied;
    Microcode ops[7] = {};  // sequence of microcode functions to execute
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

  Common::Byte hi = 0;
  Common::Byte lo = 0;

  Microcode next = nullptr;  // next microcode function to execute, nullptr indicates "move to next instruction"

  bool operator==(const State& other) const noexcept = default;
};

using Instruction = State::Instruction;

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
