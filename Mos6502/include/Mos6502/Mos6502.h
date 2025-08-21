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

  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  static constexpr Byte Carry = 1 << 0;
  static constexpr Byte Zero = 1 << 1;
  static constexpr Byte Interrupt = 1 << 2;
  static constexpr Byte Decimal = 1 << 3;
  static constexpr Byte Break = 1 << 4;
  static constexpr Byte Overflow = 1 << 5;
  static constexpr Byte Negative = 1 << 6;

  //! State functions should accept a bus response from the previous clock tick and return a new bus request for the new
  //! state.
  using StateFunc = BusRequest (*)(Mos6502&, BusResponse response, Byte step);

  struct Instruction
  {
    std::string_view name;
    StateFunc addressMode;
    StateFunc operation;
  };

  Mos6502() noexcept;

  [[nodiscard]] inline Byte a() const noexcept;
  void set_a(Byte v) noexcept;

  [[nodiscard]] Byte x() const noexcept;
  void set_x(Byte v) noexcept;

  [[nodiscard]] Byte y() const noexcept;
  void set_y(Byte v) noexcept;

  [[nodiscard]] Byte sp() const noexcept;
  void set_sp(Byte v) noexcept;

  [[nodiscard]] Address pc() const noexcept;
  void set_pc(Address addr) noexcept;

  [[nodiscard]] Byte status() const noexcept;
  void set_status(Byte flags) noexcept;

  [[nodiscard]] Byte operand() const noexcept;

  [[nodiscard]] Address target() const noexcept;

  BusRequest Tick(BusResponse response) noexcept;

  //! Get the number of ticks since the last reset.
  [[nodiscard]] uint32_t tickCount() const noexcept;

  // For testing purposes, set the current instruction
  void setInstruction(const Instruction& instr) noexcept;

private:
  static constexpr Byte ExtraStepRequired = 1 << 7;

  //! Turns the given flag on or off depending on value.
  Byte SetFlag(Byte flag, bool value) noexcept;

  //! Return true if the given flag is set.
  bool HasFlag(Byte flag) const noexcept;

  // State transition functions

  // Start the addressing mode (if applicable) or the operation if addressing mode is implied.
  void DecodeNextInstruction(Byte opcode) noexcept;

  // Transition from addressing mode to operation.
  BusRequest StartOperation(BusResponse response);

  // Transition from operation to next instruction.
  BusRequest FinishOperation();

  void Log() const;

  friend struct AddressMode;
  friend struct Operations;

  const Instruction* m_instruction = nullptr;
  StateFunc m_action = nullptr;

  uint32_t m_tickCount = 0;  // Number of ticks since the last reset

  Address m_target{0};

  // Registers
  Address m_pc{0};

  Byte m_a{0};
  Byte m_x{0};
  Byte m_y{0};
  Byte m_sp{0};
  Byte m_status{0};

  // Which step of the current instruction we are in
  Byte m_step{0};

  // Scratch data for operations and logging
  Byte m_operand;

  // Last bus request
  BusRequest m_lastBusRequest;

  LogBuffer m_log;
};

inline Common::Byte Mos6502::a() const noexcept
{
  return m_a;
}

inline void Mos6502::set_a(Common::Byte v) noexcept
{
  m_a = v;
}

inline Common::Byte Mos6502::x() const noexcept
{
  return m_x;
}

inline void Mos6502::set_x(Common::Byte v) noexcept
{
  m_x = v;
}

inline Common::Byte Mos6502::y() const noexcept
{
  return m_y;
}

inline void Mos6502::set_y(Common::Byte v) noexcept
{
  m_y = v;
}

inline Common::Byte Mos6502::sp() const noexcept
{
  return m_sp;
}

inline void Mos6502::set_sp(Common::Byte v) noexcept
{
  m_sp = v;
}

inline Common::Address Mos6502::pc() const noexcept
{
  return m_pc;
}

inline void Mos6502::set_pc(Common::Address addr) noexcept
{
  m_pc = addr;
}

inline Common::Byte Mos6502::status() const noexcept
{
  return m_status;
}

inline void Mos6502::set_status(Common::Byte flags) noexcept
{
  m_status = flags;
}

inline Common::Byte Mos6502::SetFlag(Common::Byte flag, bool value) noexcept
{
  if (value)
  {
    m_status |= flag;
  }
  else
  {
    m_status &= ~flag;
  }
  return m_status;
}

inline bool Mos6502::HasFlag(Byte flag) const noexcept
{
  return (m_status & flag) != 0;
}

inline Common::Byte Mos6502::operand() const noexcept
{
  return m_operand;
}

inline Common::Address Mos6502::target() const noexcept
{
  return m_target;
}

inline uint32_t Mos6502::tickCount() const noexcept
{
  return m_tickCount;
}
