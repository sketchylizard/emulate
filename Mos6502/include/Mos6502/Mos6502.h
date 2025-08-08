#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <tuple>

#include "Mos6502/Bus.h"

class Mos6502
{
public:
  static constexpr Byte c_ZeroPage{0x00};
  static constexpr Byte c_StackPage{0x01};

  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  constexpr Mos6502()
  {
    reset();
  }

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

  void reset() noexcept;

private:
  //! Action should return true if the instruction is complete
  using Action = bool (*)(Mos6502&, Bus& bus, size_t step);

  struct Instruction
  {
    std::string_view name;
    Byte opcode;
    Action addressMode;
    Action operation;
  };

  //! Addressing modes
  static constexpr Action c_implied = nullptr;
  static bool immediate(Mos6502& cpu, Bus& bus, size_t step);

  static bool zero_page(Mos6502& cpu, Bus& bus, size_t step);
  static bool zero_page_x(Mos6502& cpu, Bus& bus, size_t step);
  static bool zero_page_y(Mos6502& cpu, Bus& bus, size_t step);

  static bool absolute(Mos6502& cpu, Bus& bus, size_t step);
  static bool absolute_x(Mos6502& cpu, Bus& bus, size_t step);
  static bool absolute_y(Mos6502& cpu, Bus& bus, size_t step);

  static bool indirect(Mos6502& cpu, Bus& bus, size_t step);
  static bool indirect_x(Mos6502& cpu, Bus& bus, size_t step);
  static bool indirect_y(Mos6502& cpu, Bus& bus, size_t step);

  //! Operations
  // Note: These operations will be called by the instruction execution loop and should return true when the operation
  // is complete.

  static bool brk(Mos6502& cpu, Bus& bus, size_t step);
  static bool adc(Mos6502& cpu, Bus& bus, size_t step);

  static constexpr Instruction instructions[] = {
      {"BRK", 0x00, c_implied, &Mos6502::brk},  //
      {"NOP", 0xEA, c_implied, nullptr},  //
      {"ADC", 0x69, &Mos6502::immediate, &Mos6502::adc},
      // Add more instructions as needed
  };

  void decodeNextInstruction(Byte opcode) noexcept;

  Address m_pc;
  Byte m_a;
  Byte m_x;
  Byte m_y;
  Byte m_sp;
  Byte m_status;

  //! Number of cycles that the CPU has executed
  uint16_t m_cycles = 0;

  struct CurrentState
  {
    const Instruction* instruction = &instructions[0];  // default to BRK;
    Action action = instructions[0].addressMode;
    size_t cycle = 0;
  };

  CurrentState m_current;

  // Scratchpad for addressing calculations
  Address m_address = Address{0};
  Byte m_operand = 0;
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
