#include "Mos6502/Mos6502.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include "AddressMode.h"
#include "common/Bus.h"

////////////////////////////////////////////////////////////////////////////////
// operations
////////////////////////////////////////////////////////////////////////////////

struct Operations
{

  // BRK, NMI, IRQ, and Reset operations all share similar logic for pushing the PC and status onto the stack
  // and setting the PC to the appropriate vector address. This function handles that logic.
  // It returns true when the operation is complete. If forceRead is true, it force the bus to READ rather than WRITE
  // mode and the writes to the stack will be ignored.
  template<bool ForceRead>
  static Bus brk(Mos6502& cpu, Bus bus, Byte step)
  {
    Control control = ForceRead ? Control::Read : Control{};

    switch (step)
    {
      case 0:
        // Push PC high byte
        return Bus::Write(MakeAddress(cpu.m_sp--, c_StackPage), HiByte(cpu.pc()), control);
      case 1:
        // Push PC low byte
        return Bus::Write(MakeAddress(cpu.m_sp--, c_StackPage), LoByte(cpu.pc()), control);
      case 2:
        // Push status register
        return Bus::Write(MakeAddress(cpu.m_sp--, c_StackPage), cpu.status(), control);
      case 3:
        // Fetch the low byte of the interrupt/reset vector
        return Bus::Read(Mos6502::c_brkVector);
      case 4:
        // Store the lo byte of the vector
        cpu.m_operand = bus.data;
        // Fetch the high byte of the interrupt/reset vector
        return Bus::Read(Mos6502::c_brkVector + 1);
      default:
      case 5:
        // Set PC to the interrupt/reset vector address
        cpu.m_pc = MakeAddress(cpu.m_operand, bus.data);
        return cpu.FinishOperation();  // Operation complete
    }
  }

  static Bus adc(Mos6502& cpu, Bus bus, Byte step)
  {
    // Handle ADC operation
    assert(step == 0);
    Byte operand = bus.data;
    Byte result = cpu.a() + operand + (cpu.status() & 0x01);  // Carry flag

    // Set flags
    cpu.set_status((result == 0) ? 0x02 : 0);  // Zero flag
    cpu.set_status((result & 0x80) ? 0x80 : 0);  // Negative flag
    if (result < cpu.a() || result < operand)
      cpu.set_status(cpu.status() | 0x01);  // Set carry flag
    else
      cpu.set_status(cpu.status() & ~0x01);  // Clear carry flag

    cpu.set_a(result);
    return cpu.FinishOperation();  // Operation complete
  }

  static Bus cld(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    assert(step == 0);
    cpu.SetFlag(Mos6502::Decimal, false);
    return cpu.FinishOperation();
  }

  static Bus txs(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    assert(step == 0);
    cpu.set_sp(cpu.x());
    return cpu.FinishOperation();
  }

  static Bus sta(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    if (step == 0)
    {
      return Bus::Write(cpu.m_target, cpu.a());
    }
    // This step allows the data to be written to memory. Without it, we put the
    // value to write on the bus and it would be overwritten by fetching the next
    // opcode.
    return cpu.FinishOperation();
  }

  static Bus ora(Mos6502& cpu, Bus bus, Byte step)
  {
    assert(step == 0);
    // Perform OR with accumulator
    cpu.m_a |= bus.data;
    cpu.SetFlag(Mos6502::Zero, cpu.m_a == 0);  // Set zero flag
    cpu.SetFlag(Mos6502::Negative, cpu.m_a & 0x80);  // Set negative flag
    return cpu.FinishOperation();
  }

  static Bus jmpAbsolute(Mos6502& cpu, Bus bus, Byte step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch low byte
    }
    if (step == 1)
    {
      Byte loByte = bus.data;
      cpu.m_target = MakeAddress(loByte, c_ZeroPage);
      cpu.m_log.addByte(loByte, 0);
      return Bus::Read(cpu.m_pc++);  // Fetch high byte
    }
    // Step 2
    Byte hiByte = bus.data;

    cpu.m_log.addByte(hiByte, 1);

    Byte loByte = LoByte(cpu.m_target);
    cpu.m_pc = MakeAddress(loByte, hiByte);

    char buffer[] = "$XXXX";
    std::format_to(buffer + 1, "{:04X}", cpu.m_pc);
    cpu.m_log.setOperand(buffer);

    return cpu.FinishOperation();
  }

  static Bus jmpIndirect(Mos6502& cpu, Bus bus, Byte step)
  {
    if (step == 0)
    {
      // Fetch indirect address low byte
      return Bus::Read(cpu.m_pc++);
    }
    if (step == 1)
    {
      Byte loByte = bus.data;
      cpu.m_log.addByte(loByte, 0);

      // Fetch indirect address high byte
      return Bus::Read(cpu.m_pc++);
    }
    if (step == 2)
    {
      Byte hiByte = bus.data;
      cpu.m_log.addByte(hiByte, 1);
      cpu.m_target = MakeAddress(LoByte(cpu.m_target), hiByte);

      char buffer[] = "($XXXX)";
      std::format_to(buffer + 2, "{:04X}", cpu.m_target);
      cpu.m_log.setOperand(buffer);

      return Bus::Read(cpu.m_target);  // Read target low byte
    }
    if (step == 3)
    {
      Byte hiByte = bus.data;  // Store target hi byte

      // Page boundary bug: increment only low byte, don't carry to high byte
      Address hiByteAddr = MakeAddress(LoByte(cpu.m_target) + 1, hiByte);
      return Bus::Read(hiByteAddr);  // Read target high byte (with bug)
    }
    // Step 4
    cpu.m_pc = MakeAddress(LoByte(cpu.m_target), bus.data);
    return cpu.FinishOperation();
  }

  static Bus bne(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    static_cast<void>(step);
    assert(step == 0);

    if (!(cpu.m_status & Mos6502::Zero))
    {
      // If the zero flag is set, we do not branch
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand);
      cpu.m_pc += signedOffset;
    }
    return cpu.FinishOperation();
  }

  static Bus beq(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    static_cast<void>(step);
    assert(step == 0);

    if (cpu.m_status & Mos6502::Zero)
    {
      // If the zero flag is set, we do not branch
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand);
      cpu.m_pc += signedOffset;
    }
    return cpu.FinishOperation();
  }

  template<Index index>
  static Bus load(Mos6502& cpu, Bus bus, Byte step)
  {
    // This is a generic load operation for A, X, or Y registers.

    assert(step == 0);

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
    cpu.m_operand = bus.data;
    *reg = bus.data;
    // Check zero flag
    cpu.SetFlag(Mos6502::Zero, *reg == 0);
    // Check negative flag
    cpu.SetFlag(Mos6502::Negative, (*reg & 0x80) != 0);

    return cpu.FinishOperation();
  }

  template<Index index>
    requires(index != Index::None)
  static Bus increment(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    // Handle increment operation for X or Y registers

    assert(step == 0);
    static_cast<void>(step);
    if constexpr (index == Index::X)
    {
      cpu.m_x++;
      cpu.SetFlag(Mos6502::Zero, cpu.m_x == 0);
      cpu.SetFlag(Mos6502::Negative, (cpu.m_x & 0x80) != 0);
    }
    else if constexpr (index == Index::Y)
    {
      cpu.m_y++;
      cpu.SetFlag(Mos6502::Zero, cpu.m_y == 0);
      cpu.SetFlag(Mos6502::Negative, (cpu.m_y & 0x80) != 0);
    }
    return cpu.FinishOperation();
  }

  template<Index index>
    requires(index != Index::None)
  static Bus decrement(Mos6502& cpu, Bus /*bus*/, Byte step)
  {
    // Handle increment operation for X or Y registers
    assert(step == 0);
    static_cast<void>(step);
    if constexpr (index == Index::X)
    {
      cpu.m_x++;
      cpu.SetFlag(Mos6502::Zero, cpu.m_x == 0);
      cpu.SetFlag(Mos6502::Negative, (cpu.m_x & 0x80) != 0);
    }
    else if constexpr (index == Index::Y)
    {
      cpu.m_y++;
      cpu.SetFlag(Mos6502::Zero, cpu.m_y == 0);
      cpu.SetFlag(Mos6502::Negative, (cpu.m_y & 0x80) != 0);
    }
    return cpu.FinishOperation();
  }

};  // struct Operations

////////////////////////////////////////////////////////////////////////////////
// CPU implementation
////////////////////////////////////////////////////////////////////////////////
static constexpr std::array<Mos6502::Instruction, 256> c_instructions = []
{
  std::array<Mos6502::Instruction, 256> table{};
  // Fill with default NOPs or empty instructions
  for (size_t i = 0; i < 256; ++i)
  {
    table[i] = {"???", nullptr, nullptr};
  }
  // clang-format off

  using Mode = AddressMode;

  // Insert actual instructions by opcode
  table[0x00] = {"BRK", &Mode::implied, &Operations::brk<false>};
  table[0xEA] = {"NOP", &Mode::implied, nullptr};

  // ADC instructions
  table[0x69] = {"ADC", &Mode::immediate, &Operations::adc};                        // Immediate
  table[0x65] = {"ADC", &Mode::zero_page<Index::None>, &Operations::adc};  // Zero Page
  table[0x75] = {"ADC", &Mode::zero_page<Index::X>, &Operations::adc};     // Zero Page,X
  table[0x6D] = {"ADC", &Mode::absoluteRead<Index::None>, &Operations::adc};   // Absolute
  table[0x7D] = {"ADC", &Mode::absoluteRead<Index::X>, &Operations::adc};      // Absolute,X
  table[0x79] = {"ADC", &Mode::absoluteRead<Index::Y>, &Operations::adc};      // Absolute,Y
  table[0x61] = {"ADC", &Mode::indirect<Index::X>, &Operations::adc};      // (Indirect,X)
  table[0x71] = {"ADC", &Mode::indirect<Index::Y>, &Operations::adc};      // (Indirect),Y

  // LDA instructions
  table[0xA9] = {"LDA", &Mode::immediate, &Operations::load<Index::None>};  // Immediate
  table[0xA5] = {"LDA", &Mode::zero_page<Index::None>, &Operations::load<Index::None>};  // Zero Page
  table[0xB5] = {"LDA", &Mode::zero_page<Index::X>, &Operations::load<Index::None>};  // Zero Page,X
  table[0xAD] = {"LDA", &Mode::absoluteRead<Index::None>, &Operations::load<Index::None>};  // Absolute
  table[0xBD] = {"LDA", &Mode::absoluteRead<Index::X>, &Operations::load<Index::None>};  // Absolute,X
  table[0xB9] = {"LDA", &Mode::absoluteRead<Index::Y>, &Operations::load<Index::None>};  // Absolute,Y
  table[0xA1] = {"LDA", &Mode::indirect<Index::X>, &Operations::load<Index::None>};  // (Indirect,X)
  table[0xB1] = {"LDA", &Mode::indirect<Index::Y>, &Operations::load<Index::None>};  // (Indirect),Y

  // LDX instructions
  table[0xA2] = {"LDX", &Mode::immediate, &Operations::load<Index::X>};  // Immediate
  table[0xA6] = {"LDX", &Mode::zero_page<Index::None>, &Operations::load<Index::X>};  // Zero Page
  table[0xB6] = {"LDX", &Mode::zero_page<Index::Y>, &Operations::load<Index::X>};  // Zero Page,Y
  table[0xAE] = {"LDX", &Mode::absoluteRead<Index::None>, &Operations::load<Index::X>};  // Absolute
  table[0xBE] = {"LDX", &Mode::absoluteRead<Index::Y>, &Operations::load<Index::X>};  // Absolute,Y

  // LDY instructions
  table[0xA0] = {"LDY", &Mode::immediate, &Operations::load<Index::Y>};  // Immediate
  table[0xA4] = {"LDY", &Mode::zero_page<Index::None>, &Operations::load<Index::Y>};  // Zero Page
  table[0xB4] = {"LDY", &Mode::zero_page<Index::X>, &Operations::load<Index::Y>};  // Zero Page,X
  table[0xAC] = {"LDY", &Mode::absoluteRead<Index::None>, &Operations::load<Index::Y>};  // Absolute
  table[0xBC] = {"LDY", &Mode::absoluteRead<Index::X>, &Operations::load<Index::Y>};  // Absolute,X

  table[0xD8] = {"CLD", &Mode::implied, &Operations::cld};
  table[0x9A] = {"TXS", &Mode::implied, &Operations::txs};

  // STA variations
  table[0x85] = {"STA", &Mode::zero_page<Index::None>, &Operations::sta};
  table[0x95] = {"STA", &Mode::zero_page<Index::X>,    &Operations::sta};
  table[0x8D] = {"STA", &Mode::absoluteWrite<Index::None>,  &Operations::sta};
  table[0x9D] = {"STA", &Mode::absoluteWrite<Index::X>,     &Operations::sta};
  table[0x99] = {"STA", &Mode::absoluteWrite<Index::Y>,     &Operations::sta};
  table[0x81] = {"STA", &Mode::indirect<Index::X>,     &Operations::sta};
  table[0x91] = {"STA", &Mode::indirect<Index::Y>,     &Operations::sta};

  // ORA variations
  table[0x01] = {"ORA", &Mode::indirect<Index::X>,     &Operations::ora};
  table[0x05] = {"ORA", &Mode::zero_page<Index::None>, &Operations::ora};
  table[0x0D] = {"ORA", &Mode::absoluteRead<Index::None>,  &Operations::ora};
  table[0x11] = {"ORA", &Mode::indirect<Index::Y>,    &Operations::ora};
  table[0x15] = {"ORA", &Mode::zero_page<Index::X>,   &Operations::ora};
  table[0x19] = {"ORA", &Mode::absoluteRead<Index::Y>,    &Operations::ora};
  table[0x1D] = {"ORA", &Mode::absoluteRead<Index::X>,    &Operations::ora};

  // JMP Absolute and JMP Indirect
  table[0x4C] = {"JMP", nullptr, &Operations::jmpAbsolute};
  table[0x6C] = {"JMP", nullptr, &Operations::jmpIndirect};

  // Branch instructions
  table[0xD0] = {"BNE", &Mode::relative  , &Operations::bne};
  table[0xF0] = {"BEQ", &Mode::relative  , &Operations::beq};

  // Increment and Decrement instructions
  table[0xE8] = {"INX", &Mode::implied  , &Operations::increment<Index::X>};
  table[0xC8] = {"INY", &Mode::implied  , &Operations::increment<Index::Y>};
  table[0xCA] = {"DEX", &Mode::implied  , &Operations::decrement<Index::X>};
  table[0x88] = {"DEY", &Mode::implied  , &Operations::decrement<Index::Y>};

  #if 0
  // CMP — Compare Accumulator
  table[0xC9] = {"CMP", &Mode::immediate, &Operations::compare<Index::None>};
  table[0xC5] = {"CMP", &Mode::zero_page<Index::None>, &Operations::compare<Index::None>};
  table[0xD5] = {"CMP", &Mode::zero_page<Index::X>, &Operations::compare<Index::X>};
  table[0xCD] = {"CMP", &Mode::absoluteRead<Index::None>, &Operations::compare<Index::None>};
  table[0xDD] = {"CMP", &Mode::absoluteRead<Index::X>, &Operations::compare<Index::X>};
  table[0xD9] = {"CMP", &Mode::absoluteRead<Index::Y>, &Operations::compare<Index::Y>};
  table[0xC1] = {"CMP", &Mode::indirect<Index::X>, &Operations::compare<Index::X>};
  table[0xD1] = {"CMP", &Mode::indirect<Index::Y>, &Operations::compare<Index::Y>};

  // CPX — Compare X Register
  table[0xE0] = {"CPX", &Mode::immediate, &Operations::compare<Index::X>};
  table[0xE4] = {"CPX", &Mode::zero_page<>, &Operations::compare<Index::X>};
  table[0xEC] = {"CPX", &Mode::absoluteRead<>, &Operations::compare<Index::X>};

  // CPY — Compare Y Register
  table[0xC0] = {"CPY", &Mode::immediate, &Operations::compare<Index::Y>};
  table[0xC4] = {"CPY", &Mode::zero_page<>, &Operations::compare<Index::Y>};
  table[0xCC] = {"CPY", &Mode::absoluteRead<>, &Operations::compare<Index::Y>};
#endif

  // Add more instructions as needed

  // clang-format on
  return table;
}();

Mos6502::Mos6502() noexcept
{
  m_instruction = nullptr;
  m_action = nullptr;
}

Bus Mos6502::Tick(Bus bus) noexcept
{
  ++m_tickCount;

  // If Sync is set, we are fetching a new instruction
  if ((bus.control & Control::Sync) != Control::None)
  {
    // load new instruction
    DecodeNextInstruction(bus.data);
  }

  // Default the bus to read
  bus.control = Control::Read;

  // Startup is a special case, we have no instruction or action
  if (!m_instruction)
  {
    m_log.reset(m_pc);
    return Bus::Fetch(m_pc++);
  }
  if (m_instruction != nullptr)
  {
    assert(m_action);

    // Execute the current action until it calls StartOperation or FinishOperation.
    bus = m_action(*this, bus, m_step++);
  }

  return bus;
}

void Mos6502::DecodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  setInstruction(c_instructions[static_cast<size_t>(opcode)]);

  m_log.setInstruction(opcode, m_instruction->name);
  m_log.setRegisters(m_a, m_x, m_y, m_status, m_sp);
}

void Mos6502::setInstruction(const Instruction& instr) noexcept
{
  m_instruction = &instr;

  assert(m_instruction);
  m_action = m_instruction->addressMode ? m_instruction->addressMode : m_instruction->operation;
  assert(m_action);
  m_step = 0;
}

Bus Mos6502::StartOperation(Bus bus)
{
  assert(m_instruction);

  m_step = 0;
  m_action = m_instruction->operation;
  SetFlag(Mos6502::ExtraStepRequired, false);
  return m_action(*this, bus, m_step++);
}

Bus Mos6502::FinishOperation()
{
  // Instruction complete, log the last instruction.
  m_log.print();

  // Start the log for the next instruction.
  m_log.reset(m_pc);

  m_step = 0;
  m_instruction = nullptr;
  m_action = nullptr;
  SetFlag(Mos6502::ExtraStepRequired, false);
  return Bus::Fetch(m_pc++);
}
