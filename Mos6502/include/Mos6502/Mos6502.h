#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>
#include <tuple>

#include "Mos6502/Bus.h"
#include "Mos6502/Log.h"

#define MOS6502_TRACE 1

class Mos6502
{
public:
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

  //! Bus should be a new bus state.
  using StateFunc = Bus (*)(Mos6502&, Bus bus, size_t step);

  struct Instruction
  {
    std::string_view name;
    Byte opcode;
    uint8_t bytes;
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

  Bus Tick(Bus bus) noexcept;

private:
  //! Turns the given flag on or off depending on value.
  Byte SetFlag(Byte flag, bool value) noexcept;

  // State transition functions

  // Start the addressing mode (if applicable) or the operation if addressing mode is implied.
  void DecodeNextInstruction(Byte opcode) noexcept;

  // Transition from addressing mode to operation.
  Bus StartOperation(Bus bus);

  // Transition from operation to next instruction.
  Bus FinishOperation();

  void Log() const;

  friend struct AddressModes;
  friend struct Operations;

  struct Operand
  {
    bool isAddress = false;
    Byte byte{0};
    Address address{0};
  };

  const Instruction* m_instruction = nullptr;
  StateFunc m_action = nullptr;

  uint32_t m_tickCount = 0;  // Number of ticks since the last reset

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
  Address m_pcStart{0};
  Address m_target{0};
  Byte m_operand;
  Byte m_bytes[3] = {};
  Byte m_byteCount{0};

  LogBuffer m_log;
};

inline Byte Mos6502::a() const noexcept
{
  return m_a;
}

inline void Mos6502::set_a(Byte v) noexcept
{
  m_a = v;
}

inline Byte Mos6502::x() const noexcept
{
  return m_x;
}

inline void Mos6502::set_x(Byte v) noexcept
{
  m_x = v;
}

inline Byte Mos6502::y() const noexcept
{
  return m_y;
}

inline void Mos6502::set_y(Byte v) noexcept
{
  m_y = v;
}

inline Byte Mos6502::sp() const noexcept
{
  return m_sp;
}

inline void Mos6502::set_sp(Byte v) noexcept
{
  m_sp = v;
}

inline Address Mos6502::pc() const noexcept
{
  return m_pc;
}

inline void Mos6502::set_pc(Address addr) noexcept
{
  m_pc = addr;
}

inline Byte Mos6502::status() const noexcept
{
  return m_status;
}

inline void Mos6502::set_status(Byte flags) noexcept
{
  m_status = flags;
}

inline Byte Mos6502::SetFlag(Byte flag, bool value) noexcept
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
