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

#define MOS6502_TRACE 1

class Mos6502
{
public:
  static constexpr Byte c_ZeroPage{0x00};
  static constexpr Byte c_StackPage{0x01};

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

  //! Turns the given flag on or off depending on value.
  Byte SetFlag(Byte flag, bool value) noexcept;

  //! Operations
  // Note: These operations will be called by the instruction execution loop and should either CurrentState() if
  // they are still executing or FinishOperation() if they have completed.

  static State brk(Mos6502& cpu, Bus& bus, size_t step);
  static State adc(Mos6502& cpu, Bus& bus, size_t step);

  template<Index index = Index::None>
  static State load(Mos6502& cpu, Bus& bus, size_t step);

  static State cld(Mos6502& cpu, Bus& bus, size_t step);
  static State txs(Mos6502& cpu, Bus& bus, size_t step);

  static State sta(Mos6502& cpu, Bus& bus, size_t step);
  static State ora(Mos6502& cpu, Bus& bus, size_t step);

  static constexpr std::array<Instruction, 256> c_instructions = []
  {
    std::array<Instruction, 256> table{};
    // Fill with default NOPs or empty instructions
    for (size_t i = 0; i < 256; ++i)
    {
      table[i] = {"???", static_cast<Byte>(i), 1, c_implied, nullptr};
    }
    // clang-format off

    // Insert actual instructions by opcode
    table[0x00] = {"BRK", 0x00, 1, c_implied, &Mos6502::brk};
    table[0xEA] = {"NOP", 0xEA, 1, c_implied, nullptr};

    // ADC instructions
    table[0x69] = {"ADC", 0x69, 2, &Mos6502::immediate, &Mos6502::adc};                        // Immediate
    table[0x65] = {"ADC", 0x65, 2, &Mos6502::zero_page<Mos6502::Index::None>, &Mos6502::adc};  // Zero Page
    table[0x75] = {"ADC", 0x75, 2, &Mos6502::zero_page<Mos6502::Index::X>, &Mos6502::adc};     // Zero Page,X
    table[0x6D] = {"ADC", 0x6D, 3, &Mos6502::absolute<Mos6502::Index::None>, &Mos6502::adc};   // Absolute
    table[0x7D] = {"ADC", 0x7D, 3, &Mos6502::absolute<Mos6502::Index::X>, &Mos6502::adc};      // Absolute,X
    table[0x79] = {"ADC", 0x79, 3, &Mos6502::absolute<Mos6502::Index::Y>, &Mos6502::adc};      // Absolute,Y
    table[0x61] = {"ADC", 0x61, 2, &Mos6502::indirect<Mos6502::Index::X>, &Mos6502::adc};      // (Indirect,X)
    table[0x71] = {"ADC", 0x71, 2, &Mos6502::indirect<Mos6502::Index::Y>, &Mos6502::adc};      // (Indirect),Y

    // LDA instructions
    table[0xA9] = {"LDA", 0xA9, 2, &Mos6502::immediate, &Mos6502::load<Mos6502::Index::None>};  // Immediate
    table[0xA5] = {"LDA", 0xA5, 2, &Mos6502::zero_page<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::None>};  // Zero Page
    table[0xB5] = {"LDA", 0xB5, 2, &Mos6502::zero_page<Mos6502::Index::X>, &Mos6502::load<Mos6502::Index::None>};  // Zero Page,X
    table[0xAD] = {"LDA", 0xAD, 3, &Mos6502::absolute<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::None>};  // Absolute
    table[0xBD] = {"LDA", 0xBD, 3, &Mos6502::absolute<Mos6502::Index::X>, &Mos6502::load<Mos6502::Index::None>};  // Absolute,X
    table[0xB9] = {"LDA", 0xB9, 3, &Mos6502::absolute<Mos6502::Index::Y>, &Mos6502::load<Mos6502::Index::None>};  // Absolute,Y
    table[0xA1] = {"LDA", 0xA1, 2, &Mos6502::indirect<Mos6502::Index::X>, &Mos6502::load<Mos6502::Index::None>};  // (Indirect,X)
    table[0xB1] = {"LDA", 0xB1, 2, &Mos6502::indirect<Mos6502::Index::Y>, &Mos6502::load<Mos6502::Index::None>};  // (Indirect),Y

    // LDX instructions
    table[0xA2] = {"LDX", 0xA2, 2, &Mos6502::immediate, &Mos6502::load<Mos6502::Index::X>};  // Immediate
    table[0xA6] = {"LDX", 0xA6, 2, &Mos6502::zero_page<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::X>};  // Zero Page
    table[0xB6] = {"LDX", 0xB6, 2, &Mos6502::zero_page<Mos6502::Index::Y>, &Mos6502::load<Mos6502::Index::X>};  // Zero Page,Y
    table[0xAE] = {"LDX", 0xAE, 3, &Mos6502::absolute<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::X>};  // Absolute
    table[0xBE] = {"LDX", 0xBE, 3, &Mos6502::absolute<Mos6502::Index::Y>, &Mos6502::load<Mos6502::Index::X>};  // Absolute,Y

    // LDY instructions
    table[0xA0] = {"LDY", 0xA0, 2, &Mos6502::immediate, &Mos6502::load<Mos6502::Index::Y>};  // Immediate
    table[0xA4] = {"LDY", 0xA4, 2, &Mos6502::zero_page<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::Y>};  // Zero Page
    table[0xB4] = {"LDY", 0xB4, 2, &Mos6502::zero_page<Mos6502::Index::X>, &Mos6502::load<Mos6502::Index::Y>};  // Zero Page,X
    table[0xAC] = {"LDY", 0xAC, 3, &Mos6502::absolute<Mos6502::Index::None>, &Mos6502::load<Mos6502::Index::Y>};  // Absolute
    table[0xBC] = {"LDY", 0xBC, 3, &Mos6502::absolute<Mos6502::Index::X>, &Mos6502::load<Mos6502::Index::Y>};  // Absolute,X

    table[0xD8] = {"CLD", 0xD8, 1,c_implied, &Mos6502::cld};
    table[0x9A] = {"TXS", 0x9A, 1, c_implied, &Mos6502::txs};

    // STA variations
    table[0x85] = {"STA", 0x85, 2, &Mos6502::zero_page<Index::None>, &Mos6502::sta};
    table[0x95] = {"STA", 0x95, 2, &Mos6502::zero_page<Index::X>,    &Mos6502::sta};
    table[0x8D] = {"STA", 0x8D, 3, &Mos6502::absolute<Index::None>,  &Mos6502::sta};
    table[0x9D] = {"STA", 0x9D, 3, &Mos6502::absolute<Index::X>,     &Mos6502::sta};
    table[0x99] = {"STA", 0x99, 3, &Mos6502::absolute<Index::Y>,     &Mos6502::sta};
    table[0x81] = {"STA", 0x81, 2, &Mos6502::indirect<Index::X>,     &Mos6502::sta};
    table[0x91] = {"STA", 0x91, 2, &Mos6502::indirect<Index::Y>,     &Mos6502::sta};

    // ORA variations
    table[0x01] = {"ORA", 0x01, 2, &Mos6502::indirect<Index::X>    , &Mos6502::ora};
    table[0x05] = {"ORA", 0x05, 2, &Mos6502::zero_page<Index::None>, &Mos6502::ora};
    table[0x0D] = {"ORA", 0x0D, 3, &Mos6502::absolute<Index::None> , &Mos6502::ora};
    table[0x11] = {"ORA", 0x11, 2, &Mos6502::indirect<Index::Y>    , &Mos6502::ora};
    table[0x15] = {"ORA", 0x15, 2, &Mos6502::zero_page<Index::X>   , &Mos6502::ora};
    table[0x19] = {"ORA", 0x19, 3, &Mos6502::absolute<Index::Y>    , &Mos6502::ora};
    table[0x1D] = {"ORA", 0x1D, 3, &Mos6502::absolute<Index::X>    , &Mos6502::ora};

    // Add more instructions as needed

    // clang-format off
    return table;
  }();

  void decodeNextInstruction(Byte opcode) noexcept;

  // State transition functions
  State CurrentState() const noexcept;
  State FinishOperation() noexcept;

  std::string FormatOperands() const;

  // This pseudostate will handle common operations, like logging.
  static State StartOperation(Mos6502& cpu, Bus& bus, size_t step);

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

  // Maximum number of bytes for an instruction (opcode + up to 2 operands + data)
  static constexpr size_t c_maxBytes = 4;

  // Scratch data for operations and logging
  Address m_pcStart{0};
  Byte m_bytes[c_maxBytes];
  Byte m_byteCount = 0;
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
  Byte modifiedOffset = 0;

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
      modifiedOffset = cpu.m_bytes[cpu.m_byteCount++] = bus.data;
          
      // Add our index register (if this overflows, it wraps around in zero-page, this is the desired behavior.)
      if constexpr (index == Index::X)
      {
        modifiedOffset += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        modifiedOffset += cpu.m_y;
      }

      // Set bus address for the next step
      bus.address = MakeAddress(modifiedOffset, c_ZeroPage);
      bus.control = Control::Read;
      return {&Mos6502::StartOperation};
    default: assert(false && "Invalid step for zero-page index addressing mode"); return {};
  }
}

template<Mos6502::Index index>
Mos6502::State Mos6502::absolute(Mos6502& cpu, Bus& bus, size_t step)
{
  Address address{0};

  switch (step)
  {
    case 0:
      // Fetch low byte
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 1:
      // Store low byte and fetch high byte
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 2:
      // Fetch high byte and calculate address
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      address = MakeAddress(cpu.m_bytes[1], cpu.m_bytes[2]);
      // Set bus address for the next step
      if constexpr (index == Index::X)
      {
        address += static_cast<int8_t>(cpu.m_x);
      }
      else if constexpr (index == Index::Y)
      {
        address += static_cast<int8_t>(cpu.m_y);
      }
      bus.address = address;
      bus.control = Control::Read;
      return {&Mos6502::StartOperation};
    default: assert(false && "Invalid step for absolute addressing mode"); return {&Mos6502::StartOperation};
  }
}

template<Mos6502::Index index>
Mos6502::State Mos6502::indirect(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return {&Mos6502::StartOperation};
}

inline Mos6502::State Mos6502::CurrentState() const noexcept
{
  return {m_action};
}

template<Mos6502::Index index>
Mos6502::State Mos6502::load(Mos6502& cpu, Bus& bus, size_t /*step*/)
{
  // Addressing modes have already been applied, and the resulting data is in bus.data.
  Byte* reg = nullptr;
  if constexpr (index == Index::None)
  {
    reg = &cpu.m_a;
  }
  else if constexpr (index == Index::X)
  {
    reg = &cpu.m_x;
  }
  else if constexpr (index == Index::Y)
  {
    reg = &cpu.m_y;
  }
  assert(reg != nullptr);
  *reg = bus.data;
  // Check zero flag
  cpu.SetFlag(Mos6502::Zero, *reg == 0);
  // Check negative flag
  cpu.SetFlag(Mos6502::Negative, (*reg & 0x80) != 0);

  return cpu.FinishOperation();
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
