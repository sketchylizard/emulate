#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>
#include <tuple>

#include "Mos6502/Log.h"
#include "common/Bus.h"

#define MOS6502_TRACE 1

class Mos6502
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

  static constexpr Byte Carry = 0x01;
  static constexpr Byte Zero = 0x02;
  static constexpr Byte Interrupt = 0x04;
  static constexpr Byte Decimal = 0x08;
  static constexpr Byte Break = 0x10;
  static constexpr Byte Unused = 0x20;  // "U" bit, set when pushed
  static constexpr Byte Overflow = 0x40;
  static constexpr Byte Negative = 0x80;

  struct Regs
  {
    Address pc{0};
    Byte a{0};
    Byte x{0};
    Byte y{0};
    Byte sp{0};
    Byte p{Unused};  // keep U set

    // Flag helpers
    [[nodiscard]] constexpr bool has(Byte f) const noexcept;
    constexpr void set(Byte f, bool v) noexcept;
    constexpr void setZN(Byte v) noexcept;
    constexpr void assignP(Byte v) noexcept;
  };

  //! State functions should accept a bus response from the previous clock tick and return a new bus request for the new
  //! state.
  using StateFunc = BusRequest (*)(Mos6502&, BusResponse response);

  struct Instruction
  {
    std::string_view name;
    StateFunc op[3] = {};
  };

  Mos6502() noexcept;

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
    return Common::MakeAddress(m_targetLo, m_targetHi);
  }

  // Registers
  Regs regs;

private:
  // State transition functions

  void Log() const;

  static BusRequest fetchNextOpcode(Mos6502& cpu, BusResponse response);
  static BusRequest decodeOpcode(Mos6502& cpu, BusResponse response);
  static BusRequest nextOp(Mos6502& cpu, BusResponse response);

  friend struct AddressMode;
  friend struct Operations;

  const Instruction* m_instruction = nullptr;
  StateFunc m_action = &Mos6502::fetchNextOpcode;

  uint32_t m_tickCount = 0;  // Number of ticks since the last reset

  Byte m_targetLo = 0;  // Low byte of the target address
  Byte m_targetHi = 0;  // High byte of the target address

  // Scratch data for operations and logging
  Byte m_operand;

  // Stage of the current instruction (0-2)
  Byte m_stage = 0;

  // Last bus request
  BusRequest m_lastBusRequest;

  LogBuffer m_log;
};

constexpr bool Mos6502::Regs::has(Byte f) const noexcept
{
  return (p & f) != 0;
}

constexpr void Mos6502::Regs::set(Byte f, bool v) noexcept
{
  p = v ? (p | f) : (p & ~f);
}

constexpr void Mos6502::Regs::setZN(Byte v) noexcept
{
  set(Zero, v == 0);
  set(Negative, (v & 0x80) != 0);
}

// If you ever assign p wholesale, re-assert U:
constexpr void Mos6502::Regs::assignP(Byte v) noexcept
{
  p = Byte((v | Unused));
}

inline uint32_t Mos6502::tickCount() const noexcept
{
  return m_tickCount;
}
