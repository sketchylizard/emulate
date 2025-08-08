#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

class Mos6502
{
public:
  using Byte = uint8_t;
  enum class Address
  {
  };

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

  [[nodiscard]] Byte read_memory(Address address) const noexcept;
  void write_memory(Address address, Byte value) noexcept;
  void write_memory(Address address, std::span<const Byte> bytes) noexcept;

  void step() noexcept;

  void reset() noexcept;

private:
  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  //! Action should return true if the instruction is complete
  using Action = bool (*)(Mos6502&, size_t step);

  struct Instruction
  {
    std::string_view name;
    Byte opcode;
    Action addressMode;
    Action operation;
  };

  //! Addressing modes
  static bool implied(Mos6502&, size_t step);
  static bool immediate(Mos6502& cpu, size_t step);

  static bool zero_page(Mos6502& cpu, size_t step);
  static bool zero_page_x(Mos6502& cpu, size_t step);
  static bool zero_page_y(Mos6502& cpu, size_t step);

  static bool absolute(Mos6502& cpu, size_t step);
  static bool absolute_x(Mos6502& cpu, size_t step);
  static bool absolute_y(Mos6502& cpu, size_t step);

  static bool indirect(Mos6502& cpu, size_t step);
  static bool indirect_x(Mos6502& cpu, size_t step);
  static bool indirect_y(Mos6502& cpu, size_t step);

  //! Operations
  // Note: These operations will be called by the instruction execution loop and should return true when the operation
  // is complete.
  static bool adc(Mos6502& cpu, size_t step);

  static constexpr Instruction instructions[] = {
      {"NOP", 0xEA, &Mos6502::implied, nullptr},  //
      {"ADC", 0x69, &Mos6502::immediate, &Mos6502::adc},
      // Add more instructions as needed
  };

  Byte fetch() noexcept;

  void decodeNextInstruction() noexcept;

  Address m_pc;
  Byte m_aRegister;
  Byte m_xRegister;
  Byte m_yRegister;
  Byte m_sp;
  Byte m_status;

  //! Number of cycles that the CPU has executed
  uint16_t m_cycles = 0;

  struct CurrentState
  {
    const Instruction* instruction = nullptr;
    Action action = nullptr;
    size_t cycle = 0;
  };

  CurrentState m_current;

  // Scratchpad for addressing calculations
  Address m_address = Address{0};
  Byte m_operand = 0;

  // Memory is just a placeholder — inject later
  std::array<Byte, 65536> m_memory{};
};

inline Mos6502::Byte Mos6502::a() const noexcept
{
  return m_aRegister;
}

inline void Mos6502::set_a(Byte v) noexcept
{
  m_aRegister = v;
}

inline Mos6502::Byte Mos6502::x() const noexcept
{
  return m_xRegister;
}

inline void Mos6502::set_x(Byte v) noexcept
{
  m_xRegister = v;
}

inline Mos6502::Byte Mos6502::y() const noexcept
{
  return m_yRegister;
}

inline void Mos6502::set_y(Byte v) noexcept
{
  m_yRegister = v;
}

inline Mos6502::Byte Mos6502::sp() const noexcept
{
  return m_sp;
}

inline void Mos6502::set_sp(Byte v) noexcept
{
  m_sp = v;
}

inline Mos6502::Address Mos6502::pc() const noexcept
{
  return m_pc;
}

inline void Mos6502::set_pc(Address addr) noexcept
{
  m_pc = addr;
}

inline Mos6502::Byte Mos6502::status() const noexcept
{
  return m_status;
}

inline void Mos6502::set_status(Byte flags) noexcept
{
  m_status = flags;
}
