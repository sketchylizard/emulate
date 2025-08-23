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

using namespace Common;

////////////////////////////////////////////////////////////////////////////////
// operations
////////////////////////////////////////////////////////////////////////////////

struct Operations
{

  // Can be called after a write operation so that the data is actually written to memory before the next operation.
  static BusRequest finishAfterWrite(Mos6502& cpu, BusResponse)
  {
    return cpu.FinishOperation();
  }

  // BRK, NMI, IRQ, and Reset operations all share similar logic for pushing the PC and status onto the stack
  // and setting the PC to the appropriate vector address. This function handles that logic.
  // It returns true when the operation is complete. If forceRead is true, it force the BusRequest to READ rather than
  // WRITE mode and the writes to the stack will be ignored.
  template<bool ForceRead>
  static Common::BusRequest brk(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};

    cpu.m_action = &Operations::brk1<ForceRead>;
    // Push PC high byte
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), Common::HiByte(cpu.regs.pc), control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk1(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};
    cpu.m_action = &Operations::brk2<ForceRead>;
    // Push PC low byte
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), Common::LoByte(cpu.regs.pc), control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk2(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};
    cpu.m_action = &Operations::brk3<ForceRead>;
    // Push status register
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), cpu.regs.p, control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk3(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    // Fetch the low byte of the interrupt/reset vector
    cpu.m_action = &Operations::brk4<ForceRead>;
    return Common::BusRequest::Read(Mos6502::c_brkVector);
  }
  template<bool ForceRead>
  static Common::BusRequest brk4(Mos6502& cpu, Common::BusResponse response)
  {
    // Store the lo byte of the vector
    cpu.m_operand = response.data;
    cpu.m_action = &Operations::brk5<ForceRead>;
    // Fetch the high byte of the interrupt/reset vector
    return Common::BusRequest::Read(Mos6502::c_brkVector + 1);
  }
  template<bool ForceRead>
  static Common::BusRequest brk5(Mos6502& cpu, Common::BusResponse response)
  {
    // Set PC to the interrupt/reset vector address
    cpu.regs.pc = Common::MakeAddress(cpu.m_operand, response.data);
    return cpu.FinishOperation();  // Operation complete
  }

  static Common::BusRequest adc(Mos6502& cpu, Common::BusResponse response)
  {
    assert(cpu.regs.has(Mos6502::Decimal) == false);  // BCD mode not supported

    const Byte a = cpu.regs.a;
    const Byte m = response.data;
    const Byte c = cpu.regs.has(Mos6502::Carry) ? 1 : 0;

    const uint16_t sum = uint16_t(a) + uint16_t(m) + uint16_t(c);
    const Byte result = Byte(sum & 0xFF);

    cpu.regs.set(Mos6502::Carry, (sum & 0x100) != 0);
    cpu.regs.set(Mos6502::Overflow, ((~(a ^ m) & (a ^ result)) & 0x80) != 0);
    cpu.regs.setZN(result);

    cpu.regs.a = result;
    return cpu.FinishOperation();
  }

  static Common::BusRequest cld(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.regs.set(Mos6502::Decimal, false);
    return cpu.FinishOperation();
  }

  static Common::BusRequest txs(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.regs.sp = cpu.regs.x;
    return cpu.FinishOperation();
  }

  static Common::BusRequest sta(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &finishAfterWrite;
    return Common::BusRequest::Write(cpu.m_target, cpu.regs.a);
  }

  static Common::BusRequest ora(Mos6502& cpu, Common::BusResponse response)
  {
    // Perform OR with accumulator
    cpu.regs.a |= response.data;
    cpu.regs.set(Mos6502::Zero, cpu.regs.a == 0);  // Set zero flag
    cpu.regs.set(Mos6502::Negative, cpu.regs.a & 0x80);  // Set negative flag
    return cpu.FinishOperation();
  }

  static Common::BusRequest jmpAbsolute(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpAbsolute1;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch low byte
  }
  static Common::BusRequest jmpAbsolute1(Mos6502& cpu, Common::BusResponse response)
  {
    Common::Byte loByte = response.data;
    cpu.m_target = Common::MakeAddress(loByte, c_ZeroPage);
    cpu.m_log.addByte(loByte, 0);

    cpu.m_action = &Operations::jmpAbsolute2;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch high byte
  }

  static Common::BusRequest jmpAbsolute2(Mos6502& cpu, Common::BusResponse response)
  {
    // Step 2
    Common::Byte hiByte = response.data;

    cpu.m_log.addByte(hiByte, 1);

    Common::Byte loByte = Common::LoByte(cpu.m_target);
    cpu.regs.pc = Common::MakeAddress(loByte, hiByte);

    char buffer[] = "$XXXX";
    std::format_to(buffer + 1, "{:04X}", cpu.regs.pc);
    cpu.m_log.setOperand(buffer);

    return cpu.FinishOperation();
  }

  static Common::BusRequest jmpIndirect(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpIndirect1;
    // Fetch indirect address low byte
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  static Common::BusRequest jmpIndirect1(Mos6502& cpu, Common::BusResponse response)
  {
    Common::Byte loByte = response.data;
    cpu.m_log.addByte(loByte, 0);

    cpu.m_target = MakeAddress(loByte, c_ZeroPage);  // store low into m_target temporarily
    cpu.m_action = &Operations::jmpIndirect2;
    return BusRequest::Read(cpu.regs.pc++);  // fetch ptr high
  }

  static Common::BusRequest jmpIndirect2(Mos6502& cpu, Common::BusResponse response)
  {
    Common::Byte hiByte = response.data;
    cpu.m_log.addByte(hiByte, 1);
    cpu.m_target = Common::MakeAddress(LoByte(cpu.m_target), hiByte);

    char buffer[] = "($XXXX)";
    std::format_to(buffer + 2, "{:04X}", cpu.m_target);
    cpu.m_log.setOperand(buffer);

    cpu.m_action = &Operations::jmpIndirect3;
    return BusRequest::Read(cpu.m_target);  // Read target low byte
  }

  static Common::BusRequest jmpIndirect3(Mos6502& cpu, Common::BusResponse response)
  {
    const Byte targetLo = response.data;

    // 6502 bug: high byte read wraps within the *same* page as ptr
    const Address hiAddr = MakeAddress(Byte(LoByte(cpu.m_target) + 1), HiByte(cpu.m_target));

    cpu.m_action = &Operations::jmpIndirect4;
    cpu.m_operand = targetLo;  // stash low
    return BusRequest::Read(hiAddr);  // read target high (buggy wrap)
  }

  static Common::BusRequest jmpIndirect4(Mos6502& cpu, Common::BusResponse response)
  {
    const Byte targetHi = response.data;
    cpu.regs.pc = MakeAddress(cpu.m_operand, targetHi);
    return cpu.FinishOperation();
  }

  static Common::BusRequest bne(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    if (!(cpu.regs.p & Mos6502::Zero))
    {
      // BNE assumes that two values were compared by subtracting one from the other, or, that a loop counter was
      // decremented. This means that the the zero flag would be set if they were equal, or the counter reached zero.
      // Since this is Branch if Not Equal, take the branch only if the zero flag is clear.
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand);
      cpu.regs.pc += signedOffset;
    }
    return cpu.FinishOperation();
  }

  static Common::BusRequest beq(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    if (cpu.regs.p & Mos6502::Zero)
    {
      // BEQ assumes that two values were compared by subtracting one from the other, or, that a loop counter was
      // decremented. This means that the the zero flag would be set if they were equal, or the counter reached zero.
      // Since this is Branch if EQual, take the branch only if the zero flag is set.
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand);
      cpu.regs.pc += signedOffset;
    }
    return cpu.FinishOperation();
  }

  template<Index index>
  static Common::BusRequest load(Mos6502& cpu, Common::BusResponse response)
  {
    // This is a generic load operation for A, X, or Y registers.

    Common::Byte* reg = nullptr;
    if constexpr (index == Index::None)
    {
      reg = &cpu.regs.a;
    }
    else if constexpr (index == Index::X)
    {
      reg = &cpu.regs.x;
    }
    else if constexpr (index == Index::Y)
    {
      reg = &cpu.regs.y;
    }
    assert(reg != nullptr);
    cpu.m_operand = response.data;
    *reg = response.data;
    // Check zero flag
    cpu.regs.set(Mos6502::Zero, *reg == 0);
    // Check negative flag
    cpu.regs.set(Mos6502::Negative, (*reg & 0x80) != 0);

    return cpu.FinishOperation();
  }

  template<Index index>
    requires(index != Index::None)
  static Common::BusRequest increment(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    // Handle increment operation for X or Y registers

    if constexpr (index == Index::X)
    {
      cpu.regs.x++;
      cpu.regs.set(Mos6502::Zero, cpu.regs.x == 0);
      cpu.regs.set(Mos6502::Negative, (cpu.regs.x & 0x80) != 0);
    }
    else if constexpr (index == Index::Y)
    {
      cpu.regs.y++;
      cpu.regs.set(Mos6502::Zero, cpu.regs.y == 0);
      cpu.regs.set(Mos6502::Negative, (cpu.regs.y & 0x80) != 0);
    }
    return cpu.FinishOperation();
  }

  template<Index index>
    requires(index != Index::None)
  static Common::BusRequest decrement(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    // Handle increment operation for X or Y registers
    if constexpr (index == Index::X)
    {
      --cpu.regs.x;
      cpu.regs.set(Mos6502::Zero, cpu.regs.x == 0);
      cpu.regs.set(Mos6502::Negative, (cpu.regs.x & 0x80) != 0);
    }
    else if constexpr (index == Index::Y)
    {
      --cpu.regs.y;
      cpu.regs.set(Mos6502::Zero, cpu.regs.y == 0);
      cpu.regs.set(Mos6502::Negative, (cpu.regs.y & 0x80) != 0);
    }
    return cpu.FinishOperation();
  }

  template<Index index>
  static Common::BusRequest compare(Mos6502& cpu, Common::BusResponse response)
  {
    Common::Byte valueToCompare = 0;
    if constexpr (index == Index::None)
    {
      valueToCompare = cpu.regs.a;
    }
    else if constexpr (index == Index::X)
    {
      valueToCompare = cpu.regs.x;
    }
    else if constexpr (index == Index::Y)
    {
      valueToCompare = cpu.regs.y;
    }

    Common::Byte result = valueToCompare - response.data;
    cpu.regs.set(Mos6502::Carry, valueToCompare >= response.data);
    cpu.regs.set(Mos6502::Zero, result == 0);
    cpu.regs.set(Mos6502::Negative, (result & 0x80) != 0);

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
    table[i] = {"???", {}};
  }
  // clang-format off

  using Mode = AddressMode;

  // Insert actual instructions by opcode
  table[0x00] = {"BRK", {&Mode::imp, &Operations::brk<false>}};
  table[0xEA] = {"NOP", {&Mode::imp}};

  // ADC instructions
  table[0x69] = {"ADC", {&Mode::imm, &Operations::adc}};
  table[0x65] = {"ADC", {Mode::zp, &Mode::Fetch,&Operations::adc}};
  table[0x75] = {"ADC", {Mode::zpX, &Mode::Fetch,&Operations::adc}};
  table[0x6D] = {"ADC", {Mode::abs, &Mode::Fetch, &Operations::adc}};
  table[0x7D] = {"ADC", {Mode::absX, &Mode::Fetch, &Operations::adc}};
  table[0x79] = {"ADC", {Mode::absY, &Mode::Fetch, &Operations::adc}};
  table[0x61] = {"ADC", {&Mode::indirect<Index::X>, &Operations::adc}};
  table[0x71] = {"ADC", {&Mode::indirect<Index::Y>, &Operations::adc}};

  // LDA instructions
  table[0xA9] = {"LDA", {&Mode::imm, &Operations::load<Index::None>}};
  table[0xA5] = {"LDA", {Mode::zp, &Mode::Fetch,&Operations::load<Index::None>}};
  table[0xB5] = {"LDA", {Mode::zpX, &Mode::Fetch,&Operations::load<Index::None>}};
  table[0xAD] = {"LDA", {Mode::abs, &Mode::Fetch, &Operations::load<Index::None>}};
  table[0xBD] = {"LDA", {Mode::absX, &Mode::Fetch, &Operations::load<Index::None>}};
  table[0xB9] = {"LDA", {Mode::absY, &Mode::Fetch, &Operations::load<Index::None>}};
  table[0xA1] = {"LDA", {&Mode::indirect<Index::X>, &Operations::load<Index::None>}};
  table[0xB1] = {"LDA", {&Mode::indirect<Index::Y>, &Operations::load<Index::None>}};

  // LDX instructions
  table[0xA2] = {"LDX", {&Mode::imm, &Operations::load<Index::X>}};
  table[0xA6] = {"LDX", {Mode::zp, &Mode::Fetch,&Operations::load<Index::X>}};
  table[0xB6] = {"LDX", {Mode::zpY, &Mode::Fetch,&Operations::load<Index::X>}};
  table[0xAE] = {"LDX", {Mode::abs, &Mode::Fetch, &Operations::load<Index::X>}};
  table[0xBE] = {"LDX", {Mode::absY, &Mode::Fetch, &Operations::load<Index::X>}};

  // LDY instructions
  table[0xA0] = {"LDY", {&Mode::imm, &Operations::load<Index::Y>}};
  table[0xA4] = {"LDY", {Mode::zp, &Mode::Fetch,&Operations::load<Index::Y>}};
  table[0xB4] = {"LDY", {Mode::zpX, &Mode::Fetch,&Operations::load<Index::Y>}};
  table[0xAC] = {"LDY", {Mode::abs, &Mode::Fetch, &Operations::load<Index::Y>}};
  table[0xBC] = {"LDY", {Mode::absX, &Mode::Fetch, &Operations::load<Index::Y>}};

  table[0xD8] = {"CLD", {&Mode::imp, &Operations::cld}};
  table[0x9A] = {"TXS", {&Mode::imp, &Operations::txs}};

  // STA variations
  table[0x85] = {"STA", {Mode::zp, &Mode::Fetch,&Operations::sta}};
  table[0x95] = {"STA", {Mode::zpX, &Mode::Fetch,   &Operations::sta}};
  table[0x8D] = {"STA", {Mode::abs,  &Operations::sta}};
  table[0x9D] = {"STA", {Mode::absX,     &Operations::sta}};
  table[0x99] = {"STA", {Mode::absY,     &Operations::sta}};
  table[0x81] = {"STA", {&Mode::indirect<Index::X>,     &Operations::sta}};
  table[0x91] = {"STA", {&Mode::indirect<Index::Y>,     &Operations::sta}};

  // ORA variations
  table[0x01] = {"ORA", {&Mode::indirect<Index::X>,     &Operations::ora}};
  table[0x05] = {"ORA", {Mode::zp, &Mode::Fetch,&Operations::ora}};
  table[0x0D] = {"ORA", {Mode::abs, &Mode::Fetch,  &Operations::ora}};
  table[0x11] = {"ORA", {&Mode::indirect<Index::Y>,    &Operations::ora}};
  table[0x15] = {"ORA", {Mode::zpX, &Mode::Fetch,  &Operations::ora}};
  table[0x19] = {"ORA", {Mode::absY, &Mode::Fetch,    &Operations::ora}};
  table[0x1D] = {"ORA", {Mode::absX, &Mode::Fetch,    &Operations::ora}};

  // JMP Absolute and JMP Indirect
  table[0x4C] = {"JMP", {&Operations::jmpAbsolute}};
  table[0x6C] = {"JMP", {&Operations::jmpIndirect}};

  // Branch instructions
  table[0xD0] = {"BNE", {&Mode::rel  , &Operations::bne}};
  table[0xF0] = {"BEQ", {&Mode::rel  , &Operations::beq}};

  // Increment and Decrement instructions
  table[0xE8] = {"INX", {&Mode::imp  , &Operations::increment<Index::X>}};
  table[0xC8] = {"INY", {&Mode::imp  , &Operations::increment<Index::Y>}};
  table[0xCA] = {"DEX", {&Mode::imp  , &Operations::decrement<Index::X>}};
  table[0x88] = {"DEY", {&Mode::imp  , &Operations::decrement<Index::Y>}};

  // CMP — Compare Accumulator
  table[0xC9] = {"CMP", {&Mode::imm, &Operations::compare<Index::None>}};
  table[0xC5] = {"CMP", {Mode::zp, &Mode::Fetch,&Operations::compare<Index::None>}};
  table[0xD5] = {"CMP", {Mode::zpX, &Mode::Fetch,&Operations::compare<Index::None>}};
  table[0xCD] = {"CMP", {Mode::abs, &Mode::Fetch, &Operations::compare<Index::None>}};
  table[0xDD] = {"CMP", {Mode::absX, &Mode::Fetch, &Operations::compare<Index::None>}};
  table[0xD9] = {"CMP", {Mode::absY, &Mode::Fetch, &Operations::compare<Index::None>}};
  table[0xC1] = {"CMP", {&Mode::indirect<Index::X>, &Operations::compare<Index::None>}};
  table[0xD1] = {"CMP", {&Mode::indirect<Index::Y>, &Operations::compare<Index::None>}};

  // CPX — Compare X Register
  table[0xE0] = {"CPX", {&Mode::imm, &Operations::compare<Index::X>}};
  table[0xE4] = {"CPX", {Mode::zp, &Mode::Fetch,&Operations::compare<Index::X>}};
  table[0xEC] = {"CPX", {Mode::abs, &Mode::Fetch, &Operations::compare<Index::X>}};

  // CPY — Compare Y Register
  table[0xC0] = {"CPY", {&Mode::imm, &Operations::compare<Index::Y>}};
  table[0xC4] = {"CPY", {Mode::zp, &Mode::Fetch,&Operations::compare<Index::Y>}};
  table[0xCC] = {"CPY", {Mode::abs, &Mode::Fetch, &Operations::compare<Index::Y>}};

  // Add more instructions as needed

  // clang-format on
  return table;
}();

Mos6502::Mos6502() noexcept = default;

Common::BusRequest Mos6502::Tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  assert(m_action);

  // Execute the current action until it calls StartOperation or FinishOperation.
  m_lastBusRequest = m_action(*this, response);

  return m_lastBusRequest;
}

BusRequest Mos6502::fetchNextOpcode(Mos6502& cpu, BusResponse /*response*/)
{
  cpu.m_action = &Mos6502::decodeOpcode;
  return BusRequest::Fetch(cpu.regs.pc++);
}

BusRequest Mos6502::decodeOpcode(Mos6502& cpu, BusResponse response)
{
  // Decode opcode
  Byte opcode = response.data;
  cpu.setInstruction(c_instructions[static_cast<size_t>(opcode)]);

  cpu.m_log.setInstruction(opcode, cpu.m_instruction->name);
  cpu.m_log.setRegisters(cpu.regs.a, cpu.regs.x, cpu.regs.y, cpu.regs.p, cpu.regs.sp);

  return cpu.m_action(cpu, BusResponse{});
}

void Mos6502::setInstruction(const Instruction& instr) noexcept
{
  m_instruction = &instr;
  m_stage = 0;

  assert(m_instruction);
  m_action = m_instruction->op[0];
  assert(m_action);
}

Common::BusRequest Mos6502::StartOperation(Common::BusResponse response)
{
  assert(m_instruction);

  // Find the next action in the instruction's operation sequence.
  // If there are no more actions, finish the operation.
  do
  {
    ++m_stage;
  } while (m_stage < std::size(m_instruction->op) && !m_instruction->op[m_stage]);
  if (m_stage >= std::size(m_instruction->op))
  {
    return FinishOperation();
  }

  m_action = m_instruction->op[m_stage];
  return m_action(*this, response);
}

Common::BusRequest Mos6502::FinishOperation()
{
  // Instruction complete, log the last instruction.
  m_log.print();

  // Start the log for the next instruction.
  m_log.reset(regs.pc);

  m_instruction = nullptr;
  m_action = &Mos6502::fetchNextOpcode;
  return fetchNextOpcode(*this, BusResponse{});
}
