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

#include "Mos6502/Bus.h"

namespace
{

//! Enum to be used as a template for addressing modes that work with indexing
enum class Index
{
  None,
  X,
  Y
};

inline constexpr Byte c_ZeroPage{0x00};
inline constexpr Byte c_StackPage{0x01};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Addressing modes
////////////////////////////////////////////////////////////////////////////////

struct AddressModes
{
  static Bus accumulator(Mos6502& cpu, Bus bus, size_t step)
  {
    // Handle accumulator addressing mode
    assert(step == 0);
    cpu.m_log.setOperand("A");
    // Since there are no operands, we should go straight into processing the operation.
    return cpu.StartOperation(bus);
  }

  static Bus implied(Mos6502& cpu, Bus bus, size_t step)
  {
    // Handle implied addressing mode
    assert(step == 0);
    // Since there are no operands, we should go straight into processing the operation.
    return cpu.StartOperation(bus);
  }

  static Bus immediate(Mos6502& cpu, Bus bus, size_t step)
  {
    // Handle immediate addressing mode
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);
    }

    // Get the data off the bus & switch to the operation:
    cpu.m_operand = bus.data;

    char buffer[] = "#$XX";
    std::format_to(buffer + 2, "{:02X}", bus.data);
    cpu.m_log.setOperand(buffer);
    return cpu.StartOperation(bus);
  }

  static Bus relative(Mos6502& cpu, Bus bus, size_t step)
  {
    // Handle relative addressing mode
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);
    }

    // Get the data off the bus & switch to the operation:
    cpu.m_operand = bus.data;

    return cpu.StartOperation(bus);
  }

  template<Index index>
  static Bus zero_page(Mos6502& cpu, Bus bus, size_t step)
  {
    // Handle zero-page X addressing mode

    if (step == 0)
    {
      // Fetch the zero-page address
      return Bus::Read(cpu.m_pc++);
    }
    if (step == 1)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_log.addByte(bus.data, 0);

      char buffer[] = "<$XX>";
      std::format_to(buffer + 2, "{:02X}", bus.data);
      cpu.m_log.setOperand(buffer);

      Byte loByte = bus.data;

      // Add our index register (if this overflows, it wraps around in zero-page, this is the desired behavior.)
      if constexpr (index == Index::X)
      {
        loByte += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        loByte += cpu.m_y;
      }

      return Bus::Read(MakeAddress(loByte, c_ZeroPage));
    }

    cpu.m_operand = bus.data;
    return cpu.StartOperation(bus);
  }

  template<Index index>
  static Bus absoluteRead(Mos6502& cpu, Bus bus, size_t step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch low byte
    }
    if (step == 1)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_log.addByte(bus.data, 0);

      return Bus::Read(cpu.m_pc++);  // Fetch high byte
    }
    if (step == 2)
    {
      // Get the high byte off the bus
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_log.addByte(bus.data, 1);

      // Calculate target address
      cpu.m_target = MakeAddress(cpu.m_bytes[0], cpu.m_bytes[1]);

      char buffer[] = "$XXXX";
      std::format_to(buffer + 2, "{:02X}", cpu.m_target);
      cpu.m_log.setOperand(buffer);

      // Check for a page crossing
      Byte loByte = cpu.m_bytes[0];
      if constexpr (index == Index::X)
      {
        loByte += cpu.m_x;
        cpu.m_target += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        loByte += cpu.m_y;
        cpu.m_target += cpu.m_y;
      }

      if constexpr (index != Index::None)
      {
        if (cpu.m_bytes[0] > loByte)
        {  // Page boundary crossed
          // Read with wrong high byte first
          Address wrongAddr = MakeAddress(loByte, cpu.m_bytes[1]);
          // Correct target already calculated above
          return Bus::Read(wrongAddr);
        }
      }

      // No page crossing (or non-indexed) - read correct address
      return Bus::Read(cpu.m_target);
    }
    if (step == 3)
    {
      // This is the page-crossing fixup read for indexed modes
      if constexpr (index != Index::None)
      {
        return Bus::Read(cpu.m_target);
      }
      else
      {
        // Non-indexed shouldn't reach step 3
        assert(false);
      }
    }

    // Final step - assign operand and start operation
    cpu.m_operand = bus.data;
    return cpu.StartOperation(bus);
  }

  template<Index index>
  static Bus absoluteWrite(Mos6502& cpu, Bus bus, size_t step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch low byte
    }
    if (step == 1)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      return Bus::Read(cpu.m_pc++);  // Fetch high byte
    }
    if (step == 2)
    {
      auto baseAddr = MakeAddress(cpu.m_bytes[0], bus.data);
      cpu.m_target = baseAddr;

      if constexpr (index == Index::X)
      {
        cpu.m_target += cpu.m_x;
      }
      else if constexpr (index == Index::Y)
      {
        cpu.m_target += cpu.m_y;
      }

      if constexpr (index != Index::None)
      {
        if (!IsSamePage(baseAddr, cpu.m_target))
        {
          // Page boundary crossed - dummy read with wrong high byte
          Address wrongAddr = MakeAddress(LoByte(cpu.m_target), HiByte(baseAddr));
          return Bus::Read(wrongAddr);
        }
      }

      // No page crossing or non-indexed - go straight to operation
      return cpu.StartOperation(bus);
    }
    if (step == 3)
    {
      // Page-crossing fixup for indexed writes
      return cpu.StartOperation(bus);
    }

    assert(false);
    return bus;
  }

  template<Index index>
    requires(index != Index::None)
  static Bus indirect(Mos6502& cpu, Bus bus, size_t step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch zero page address
    }
    if (step == 1)
    {
      // Store zero page base address
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;

      if constexpr (index == Index::X)
      {
        // For (zp,X) mode - add X to zero page address first
        cpu.m_bytes[0] += cpu.m_x;
      }

      // Read low byte of target address
      return Bus::Read(Address{MakeAddress(cpu.m_bytes[0], c_ZeroPage)});
    }
    if (step == 2)
    {
      // Store low byte of target address
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;

      // Read high byte of target address
      // Note: Zero page wrapping - if lo byte is $FF, this reads from $00
      Byte hiByteAddr = cpu.m_bytes[0] + 1;
      return Bus::Read(MakeAddress(hiByteAddr, c_ZeroPage));
    }
    if (step == 3)
    {
      // Complete the target address
      cpu.m_target = MakeAddress(cpu.m_bytes[0], bus.data);

      if constexpr (index == Index::Y)
      {
        // For (zp),Y mode - add Y to the final target address
        Address baseAddr = cpu.m_target;
        cpu.m_target += cpu.m_y;

        // Check for page boundary crossing
        if (!IsSamePage(baseAddr, cpu.m_target))
        {
          // Page boundary crossed - dummy read with wrong high byte
          Address wrongAddr = MakeAddress(LoByte(cpu.m_target), HiByte(baseAddr));
          return Bus::Read(wrongAddr);
        }
      }

      // Read from the final target address
      return Bus::Read(cpu.m_target);
    }
    if (step == 4)
    {
      // Page crossing fixup for (zp),Y mode
      if constexpr (index == Index::Y)
      {
        return Bus::Read(cpu.m_target);
      }
      else
      {
        assert(false);  // Should not reach here for other modes
      }
    }

    // Final step - assign operand and start operation
    cpu.m_operand = bus.data;
    return cpu.StartOperation(bus);
  }

};  // struct AddressModes

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
  static Bus brk(Mos6502& cpu, Bus bus, size_t step)
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

  static Bus adc(Mos6502& cpu, Bus bus, size_t step)
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

  static Bus cld(Mos6502& cpu, Bus /*bus*/, size_t step)
  {
    assert(step == 0);
    cpu.SetFlag(Mos6502::Decimal, false);
    return cpu.FinishOperation();
  }

  static Bus txs(Mos6502& cpu, Bus /*bus*/, size_t step)
  {
    assert(step == 0);
    cpu.set_sp(cpu.x());
    return cpu.FinishOperation();
  }

  static Bus sta(Mos6502& cpu, Bus /*bus*/, size_t step)
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

  static Bus ora(Mos6502& cpu, Bus bus, size_t step)
  {
    assert(step == 0);
    // Perform OR with accumulator
    cpu.m_a |= bus.data;
    cpu.SetFlag(Mos6502::Zero, cpu.m_a == 0);  // Set zero flag
    cpu.SetFlag(Mos6502::Negative, cpu.m_a & 0x80);  // Set negative flag
    return cpu.FinishOperation();
  }

  static Bus jmpAbsolute(Mos6502& cpu, Bus bus, size_t step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch low byte
    }
    if (step == 1)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      return Bus::Read(cpu.m_pc++);  // Fetch high byte
    }
    // Step 2
    cpu.m_bytes[cpu.m_byteCount++] = bus.data;
    cpu.m_pc = MakeAddress(cpu.m_bytes[0], bus.data);
    return cpu.FinishOperation();
  }

  static Bus jmpIndirect(Mos6502& cpu, Bus bus, size_t step)
  {
    if (step == 0)
    {
      return Bus::Read(cpu.m_pc++);  // Fetch indirect address low byte
    }
    if (step == 1)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      return Bus::Read(cpu.m_pc++);  // Fetch indirect address high byte
    }
    if (step == 2)
    {
      cpu.m_target = MakeAddress(cpu.m_bytes[0], bus.data);
      return Bus::Read(cpu.m_target);  // Read target low byte
    }
    if (step == 3)
    {
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;  // Store target low byte

      // Page boundary bug: increment only low byte, don't carry to high byte
      Address hiByteAddr = MakeAddress(LoByte(cpu.m_target) + 1, HiByte(cpu.m_target));
      return Bus::Read(hiByteAddr);  // Read target high byte (with bug)
    }
    // Step 4
    cpu.m_pc = MakeAddress(cpu.m_bytes[0], bus.data);
    return cpu.FinishOperation();
  }

  static Bus bne(Mos6502& cpu, Bus bus, size_t step)
  {
    static_cast<void>(step);
    assert(step == 0);

    cpu.m_bytes[cpu.m_byteCount++] = bus.data;
    if (!(cpu.m_status & Mos6502::Zero))
    {
      // If the zero flag is set, we do not branch
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_pc += static_cast<int8_t>(bus.data);
    }
    return cpu.FinishOperation();
  }

  static Bus beq(Mos6502& cpu, Bus bus, size_t step)
  {
    static_cast<void>(step);
    assert(step == 0);

    cpu.m_bytes[cpu.m_byteCount++] = bus.data;
    if (cpu.m_status & Mos6502::Zero)
    {
      // If the zero flag is set, we do not branch
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_pc += static_cast<int8_t>(bus.data);
    }
    return cpu.FinishOperation();
  }

  template<Index index>
  static Bus load(Mos6502& cpu, Bus bus, size_t step)
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
  static Bus increment(Mos6502& cpu, Bus /*bus*/, size_t step)
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
  static Bus decrement(Mos6502& cpu, Bus /*bus*/, size_t step)
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
    table[i] = {"???", static_cast<Byte>(i), 1, nullptr, nullptr};
  }
  // clang-format off

  // Insert actual instructions by opcode
  table[0x00] = {"BRK", 0x00, 1, &AddressModes::implied, &Operations::brk<false>};
  table[0xEA] = {"NOP", 0xEA, 1, &AddressModes::implied, nullptr};

  // ADC instructions
  table[0x69] = {"ADC", 0x69, 2, &AddressModes::immediate, &Operations::adc};                        // Immediate
  table[0x65] = {"ADC", 0x65, 2, &AddressModes::zero_page<Index::None>, &Operations::adc};  // Zero Page
  table[0x75] = {"ADC", 0x75, 2, &AddressModes::zero_page<Index::X>, &Operations::adc};     // Zero Page,X
  table[0x6D] = {"ADC", 0x6D, 3, &AddressModes::absoluteRead<Index::None>, &Operations::adc};   // Absolute
  table[0x7D] = {"ADC", 0x7D, 3, &AddressModes::absoluteRead<Index::X>, &Operations::adc};      // Absolute,X
  table[0x79] = {"ADC", 0x79, 3, &AddressModes::absoluteRead<Index::Y>, &Operations::adc};      // Absolute,Y
  table[0x61] = {"ADC", 0x61, 2, &AddressModes::indirect<Index::X>, &Operations::adc};      // (Indirect,X)
  table[0x71] = {"ADC", 0x71, 2, &AddressModes::indirect<Index::Y>, &Operations::adc};      // (Indirect),Y

  // LDA instructions
  table[0xA9] = {"LDA", 0xA9, 2, &AddressModes::immediate, &Operations::load<Index::None>};  // Immediate
  table[0xA5] = {"LDA", 0xA5, 2, &AddressModes::zero_page<Index::None>, &Operations::load<Index::None>};  // Zero Page
  table[0xB5] = {"LDA", 0xB5, 2, &AddressModes::zero_page<Index::X>, &Operations::load<Index::None>};  // Zero Page,X
  table[0xAD] = {"LDA", 0xAD, 3, &AddressModes::absoluteRead<Index::None>, &Operations::load<Index::None>};  // Absolute
  table[0xBD] = {"LDA", 0xBD, 3, &AddressModes::absoluteRead<Index::X>, &Operations::load<Index::None>};  // Absolute,X
  table[0xB9] = {"LDA", 0xB9, 3, &AddressModes::absoluteRead<Index::Y>, &Operations::load<Index::None>};  // Absolute,Y
  table[0xA1] = {"LDA", 0xA1, 2, &AddressModes::indirect<Index::X>, &Operations::load<Index::None>};  // (Indirect,X)
  table[0xB1] = {"LDA", 0xB1, 2, &AddressModes::indirect<Index::Y>, &Operations::load<Index::None>};  // (Indirect),Y

  // LDX instructions
  table[0xA2] = {"LDX", 0xA2, 2, &AddressModes::immediate, &Operations::load<Index::X>};  // Immediate
  table[0xA6] = {"LDX", 0xA6, 2, &AddressModes::zero_page<Index::None>, &Operations::load<Index::X>};  // Zero Page
  table[0xB6] = {"LDX", 0xB6, 2, &AddressModes::zero_page<Index::Y>, &Operations::load<Index::X>};  // Zero Page,Y
  table[0xAE] = {"LDX", 0xAE, 3, &AddressModes::absoluteRead<Index::None>, &Operations::load<Index::X>};  // Absolute
  table[0xBE] = {"LDX", 0xBE, 3, &AddressModes::absoluteRead<Index::Y>, &Operations::load<Index::X>};  // Absolute,Y

  // LDY instructions
  table[0xA0] = {"LDY", 0xA0, 2, &AddressModes::immediate, &Operations::load<Index::Y>};  // Immediate
  table[0xA4] = {"LDY", 0xA4, 2, &AddressModes::zero_page<Index::None>, &Operations::load<Index::Y>};  // Zero Page
  table[0xB4] = {"LDY", 0xB4, 2, &AddressModes::zero_page<Index::X>, &Operations::load<Index::Y>};  // Zero Page,X
  table[0xAC] = {"LDY", 0xAC, 3, &AddressModes::absoluteRead<Index::None>, &Operations::load<Index::Y>};  // Absolute
  table[0xBC] = {"LDY", 0xBC, 3, &AddressModes::absoluteRead<Index::X>, &Operations::load<Index::Y>};  // Absolute,X

  table[0xD8] = {"CLD", 0xD8, 1, &AddressModes::implied, &Operations::cld};
  table[0x9A] = {"TXS", 0x9A, 1, &AddressModes::implied, &Operations::txs};

  // STA variations
  table[0x85] = {"STA", 0x85, 2, &AddressModes::zero_page<Index::None>, &Operations::sta};
  table[0x95] = {"STA", 0x95, 2, &AddressModes::zero_page<Index::X>,    &Operations::sta};
  table[0x8D] = {"STA", 0x8D, 3, &AddressModes::absoluteWrite<Index::None>,  &Operations::sta};
  table[0x9D] = {"STA", 0x9D, 3, &AddressModes::absoluteWrite<Index::X>,     &Operations::sta};
  table[0x99] = {"STA", 0x99, 3, &AddressModes::absoluteWrite<Index::Y>,     &Operations::sta};
  table[0x81] = {"STA", 0x81, 2, &AddressModes::indirect<Index::X>,     &Operations::sta};
  table[0x91] = {"STA", 0x91, 2, &AddressModes::indirect<Index::Y>,     &Operations::sta};

  // ORA variations
  table[0x01] = {"ORA", 0x01, 2, &AddressModes::indirect<Index::X>,     &Operations::ora};
  table[0x05] = {"ORA", 0x05, 2, &AddressModes::zero_page<Index::None>, &Operations::ora};
  table[0x0D] = {"ORA", 0x0D, 3, &AddressModes::absoluteRead<Index::None>,  &Operations::ora};
  table[0x11] = {"ORA", 0x11, 2, &AddressModes::indirect<Index::Y>,    &Operations::ora};
  table[0x15] = {"ORA", 0x15, 2, &AddressModes::zero_page<Index::X>,   &Operations::ora};
  table[0x19] = {"ORA", 0x19, 3, &AddressModes::absoluteRead<Index::Y>,    &Operations::ora};
  table[0x1D] = {"ORA", 0x1D, 3, &AddressModes::absoluteRead<Index::X>,    &Operations::ora};

  // JMP Absolute and JMP Indirect
  table[0x4C] = {"JMP", 0x4C, 3, nullptr, &Operations::jmpAbsolute};
  table[0x6C] = {"JMP", 0x6C, 3, nullptr, &Operations::jmpIndirect};

  // Branch instructions
  table[0xD0] = {"BNE", 0xD0, 2, &AddressModes::relative  , &Operations::bne};
  table[0xF0] = {"BEQ", 0xF0, 2, &AddressModes::relative  , &Operations::beq};

  // Increment and Decrement instructions
  table[0xE8] = {"INX", 0xE8, 2, &AddressModes::implied  , &Operations::increment<Index::X>};
  table[0xC8] = {"INY", 0xC8, 2, &AddressModes::implied  , &Operations::increment<Index::Y>};
  table[0xCA] = {"DEX", 0xCA, 2, &AddressModes::implied  , &Operations::decrement<Index::X>};
  table[0x88] = {"DEY", 0x88, 2, &AddressModes::implied  , &Operations::decrement<Index::Y>};

  #if 0
  // CMP — Compare Accumulator
  table[0xC9] = {"CMP", 0xC9, 2, &AddressModes::immediate, &Operations::compare<Index::None>};
  table[0xC5] = {"CMP", 0xC5, 2, &AddressModes::zero_page<Index::None>, &Operations::compare<Index::None>};
  table[0xD5] = {"CMP", 0xD5, 2, &AddressModes::zero_page<Index::X>, &Operations::compare<Index::X>};
  table[0xCD] = {"CMP", 0xCD, 3, &AddressModes::absoluteRead<Index::None>, &Operations::compare<Index::None>};
  table[0xDD] = {"CMP", 0xDD, 3, &AddressModes::absoluteRead<Index::X>, &Operations::compare<Index::X>};
  table[0xD9] = {"CMP", 0xD9, 3, &AddressModes::absoluteRead<Index::Y>, &Operations::compare<Index::Y>};
  table[0xC1] = {"CMP", 0xC1, 2, &AddressModes::indirect<Index::X>, &Operations::compare<Index::X>};
  table[0xD1] = {"CMP", 0xD1, 2, &AddressModes::indirect<Index::Y>, &Operations::compare<Index::Y>};

  // CPX — Compare X Register
  table[0xE0] = {"CPX", 0xE0, 2, &AddressModes::immediate, &Operations::compare<Index::X>};
  table[0xE4] = {"CPX", 0xE4, 2, &AddressModes::zero_page<>, &Operations::compare<Index::X>};
  table[0xEC] = {"CPX", 0xEC, 3, &AddressModes::absoluteRead<>, &Operations::compare<Index::X>};

  // CPY — Compare Y Register
  table[0xC0] = {"CPY", 0xC0, 2, &AddressModes::immediate, &Operations::compare<Index::Y>};
  table[0xC4] = {"CPY", 0xC4, 2, &AddressModes::zero_page<>, &Operations::compare<Index::Y>};
  table[0xCC] = {"CPY", 0xCC, 3, &AddressModes::absoluteRead<>, &Operations::compare<Index::Y>};
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

    // Execute the current action until it returns a nullptr.
    bus = m_action(*this, bus, m_step++);
  }

  return bus;
}

void Mos6502::DecodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  m_instruction = &c_instructions[static_cast<size_t>(opcode)];
  assert(m_instruction);
  m_action = m_instruction->addressMode ? m_instruction->addressMode : m_instruction->operation;
  assert(m_action);
  m_step = 0;

  m_log.setInstruction(opcode, m_instruction->name);
  m_log.setRegisters(m_a, m_x, m_y, m_status, m_sp);
}

Bus Mos6502::StartOperation(Bus bus)
{
  m_step = 0;
  m_action = m_instruction->operation;
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
  return Bus::Fetch(m_pc++);
}
