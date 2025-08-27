#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>
#include <tuple>

#include "common/Bus.h"

class Core65xx
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;
  using BusRequest = Common::BusRequest;
  using BusResponse = Common::BusResponse;

  enum class Register
  {
    A,
    X,
    Y
  };

  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  enum class Flag : Byte
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

  struct Regs
  {
    Address pc{0};
    Byte a{0};
    Byte x{0};
    Byte y{0};
    Byte sp{0};
    Byte p{static_cast<Byte>(Flag::Unused)};  // keep U set

    // Flag helpers
    [[nodiscard]] constexpr bool has(Flag f) const noexcept;
    constexpr void set(Flag f, bool v) noexcept;
    constexpr void setZN(Byte v) noexcept;
    constexpr void assignP(Byte v) noexcept;
  };

  //! State functions should accept a bus response from the previous clock tick and return a new bus request for the new
  //! state.
  using StateFunc = BusRequest (*)(Core65xx&, BusResponse response);

  struct Instruction
  {
    std::string_view name;
    StateFunc op[3] = {};
  };

  struct Operand
  {
    // clang-format off
    enum class Type : uint8_t {
      Impl, Acc, Imm8, 
      Zp, ZpX, ZpY, 
      Abs, AbsX, AbsY,
      Ind, IndZpX, IndZpY,
      Rel
    };
    // clang-format on
    Type type = Type::Impl;
    Byte lo = 0;
    Byte hi = 0;
  };

  Core65xx() noexcept;

  [[nodiscard]] BusRequest Tick(BusResponse response) noexcept;

  //! Get the number of ticks since the last reset.
  [[nodiscard]] uint32_t tickCount() const noexcept;

  // For testing purposes, set the current instruction
  void setInstruction(const Instruction& instr) noexcept;

  // Select A/X/Y by Register at compile time
  template<Register R>
  static constexpr Byte& sel(Regs& r) noexcept
  {
    if constexpr (R == Register::A)
      return r.a;
    else if constexpr (R == Register::X)
      return r.x;
    else /* R == Register::Y */
      return r.y;
  }

  Address getEffectiveAddress() const noexcept
  {
    return Common::MakeAddress(m_operand.lo, m_operand.hi);
  }

  // Registers
  Regs regs;

private:
  // State transition functions

  static BusRequest fetchNextOpcode(Core65xx& cpu, BusResponse response);
  static BusRequest decodeOpcode(Core65xx& cpu, BusResponse response);
  static BusRequest nextOp(Core65xx& cpu, BusResponse response);

  friend struct AddressMode;
  friend struct Operations;

  const Instruction* m_instruction = nullptr;
  StateFunc m_action = &Core65xx::fetchNextOpcode;

  uint32_t m_tickCount = 0;  // Number of ticks since the last reset

  Operand m_operand;  // Value of the current operand (if any)

  // Stage of the current instruction (0-2)
  Byte m_stage = 0;

  // Last bus request
  BusRequest m_lastBusRequest;
};

constexpr bool Core65xx::Regs::has(Flag f) const noexcept
{
  return (p & static_cast<uint8_t>(f)) != 0;
}

constexpr void Core65xx::Regs::set(Flag f, bool v) noexcept
{
  p = v ? (p | static_cast<uint8_t>(f)) : (p & ~static_cast<uint8_t>(f));
}

constexpr void Core65xx::Regs::setZN(Byte v) noexcept
{
  set(Flag::Zero, v == 0);
  set(Flag::Negative, (v & 0x80) != 0);
}

// If you ever assign p wholesale, re-assert U:
constexpr void Core65xx::Regs::assignP(Byte v) noexcept
{
  p = Byte((v | static_cast<uint8_t>(Flag::Unused)));
}

inline uint32_t Core65xx::tickCount() const noexcept
{
  return m_tickCount;
}
