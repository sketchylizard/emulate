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
  struct State;

  //! State should the new state if the instruction is complete
  using StateFunc = State (*)(Mos6502&, Bus& bus, size_t step);

  struct State
  {
    StateFunc func = nullptr;
  };

  struct Instruction
  {
    std::string_view name;
    Byte opcode;
    uint8_t bytes;
    StateFunc addressMode;
    StateFunc operation;
  };

  enum class Index
  {
    None,
    X,
    Y
  };

  static State doReset(Mos6502& cpu, Bus& bus, size_t step, bool forceRead, Address vector);

  //! Addressing modes
  static constexpr StateFunc c_implied = nullptr;
  static State immediate(Mos6502& cpu, Bus& bus, size_t step);

  template<Index index = Index::None>
  static State zero_page(Mos6502& cpu, Bus& bus, size_t step);

  template<Index index = Index::None>
  static State absolute(Mos6502& cpu, Bus& bus, size_t step);

  template<Index index = Index::None>
  static State indirect(Mos6502& cpu, Bus& bus, size_t step);

  std::string FormatOperands(StateFunc& addressingMode, Byte byte1, Byte byte2) noexcept;

  //! Operations
  // Note: These operations will be called by the instruction execution loop and should either CurrentState() if they
  // are still executing or FinishOperation() if they have completed.

  static State brk(Mos6502& cpu, Bus& bus, size_t step);
  static State adc(Mos6502& cpu, Bus& bus, size_t step);

  static constexpr Instruction c_instructions[] = {
      {"BRK", 0x00, 1, c_implied, &Mos6502::brk},  //
      {"NOP", 0xEA, 1, c_implied, nullptr},  //
      {"ADC", 0x69, 2, &Mos6502::immediate, &Mos6502::adc},
      // Add more instructions as needed
  };

  void decodeNextInstruction(Byte opcode) noexcept;

  // State transition functions
  State CurrentState() const noexcept;
  State StartOperation() noexcept;
  State FinishOperation() noexcept;

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

  // Scratch data for operations
  Byte m_byte1{0};
  Byte m_byte2{0};
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

template<Mos6502::Index index>
Mos6502::State Mos6502::zero_page(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle zero-page X addressing mode
  switch (step)
  {
    case 0:
      // Fetch the zero-page address
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return cpu.CurrentState();  // Need another step to read the data
    case 1:
      // Read the data from zero-page address
      cpu.m_byte1 = bus.data;
      // Add our index register (if this overflows, it wraps around in zero-page, this is the desired behavior.)
      if constexpr (index == Index::X)
      {
        cpu.m_byte1 += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        cpu.m_byte1 += cpu.m_y;
      }

      // Set bus address for the next step
      bus.address = MakeAddress(cpu.m_byte1, c_ZeroPage);
      bus.control = Control::Read;
      return cpu.StartOperation();  // Start the operation
    default: assert(false && "Invalid step for zero-page index addressing mode"); return {};
  }
}

template<Mos6502::Index index>
Mos6502::State Mos6502::absolute(Mos6502& cpu, Bus& bus, size_t step)
{
  switch (step)
  {
    case 0:
      // Fetch low byte
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 1:
      // Store low byte and fetch high byte
      cpu.m_byte1 = bus.data;
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 2:
      // Fetch high byte and calculate address
      cpu.m_byte2 = bus.data;
      // Set bus address for the next step
      if constexpr (index == Index::X)
      {
        cpu.m_byte1 += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        cpu.m_byte1 += cpu.m_y;
      }
      bus.address = MakeAddress(cpu.m_byte1, cpu.m_byte2);
      bus.control = Control::Read;
      return cpu.StartOperation();  // Start the operation
    default: assert(false && "Invalid step for absolute addressing mode"); return cpu.StartOperation();
  }
}

template<Mos6502::Index index>
Mos6502::State Mos6502::indirect(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return cpu.StartOperation();  // Start the operation
}

inline Mos6502::State Mos6502::CurrentState() const noexcept
{
  return {m_action};
}

inline Mos6502::State Mos6502::StartOperation() noexcept
{
  assert(m_instruction != nullptr);
  m_step = 0;
  return {m_instruction->operation};
}
