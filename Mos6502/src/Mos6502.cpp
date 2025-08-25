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
    cpu.m_operand.lo = response.data;
    cpu.m_action = &Operations::brk5<ForceRead>;
    // Fetch the high byte of the interrupt/reset vector
    return Common::BusRequest::Read(Mos6502::c_brkVector + 1);
  }
  template<bool ForceRead>
  static Common::BusRequest brk5(Mos6502& cpu, Common::BusResponse response)
  {
    // Set PC to the interrupt/reset vector address
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Mos6502::nextOp(cpu, response);  // Operation complete
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
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest cld(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.regs.set(Mos6502::Decimal, false);
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest txs(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.regs.sp = cpu.regs.x;
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest sta(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Mos6502::nextOp;
    return Common::BusRequest::Write(cpu.getEffectiveAddress(), cpu.regs.a);
  }

  static Common::BusRequest ora(Mos6502& cpu, Common::BusResponse response)
  {
    // Perform OR with accumulator
    cpu.regs.a |= response.data;
    cpu.regs.set(Mos6502::Zero, cpu.regs.a == 0);  // Set zero flag
    cpu.regs.set(Mos6502::Negative, cpu.regs.a & 0x80);  // Set negative flag
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest jmpAbsolute(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpAbsolute1;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch low byte
  }

  static Common::BusRequest jmpAbsolute1(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;

    cpu.m_action = &Operations::jmpAbsolute2;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch high byte
  }

  static Common::BusRequest jmpAbsolute2(Mos6502& cpu, Common::BusResponse response)
  {
    // Step 2
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest jmpIndirect(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpIndirect1;
    // Fetch indirect address low byte
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  static Common::BusRequest jmpIndirect1(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;
    cpu.m_action = &Operations::jmpIndirect2;
    return BusRequest::Read(cpu.regs.pc++);  // fetch ptr high
  }

  static Common::BusRequest jmpIndirect2(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand.hi = response.data;
    cpu.m_action = &Operations::jmpIndirect3;
    return BusRequest::Read(cpu.getEffectiveAddress());  // Read target low byte
  }

  static Common::BusRequest jmpIndirect3(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;

    // 6502 bug: high byte read wraps within the *same* page as ptr
    cpu.m_operand.hi = 0;  // this is wrong
    cpu.m_action = &Operations::jmpIndirect4;
    return BusRequest::Read(cpu.getEffectiveAddress());  // read target high (buggy wrap)
  }

  static Common::BusRequest jmpIndirect4(Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest bne(Mos6502& cpu, Common::BusResponse response)
  {
    if (!(cpu.regs.p & Mos6502::Zero))
    {
      // BNE assumes that two values were compared by subtracting one from the other, or, that a loop counter was
      // decremented. This means that the the zero flag would be set if they were equal, or the counter reached zero.
      // Since this is Branch if Not Equal, take the branch only if the zero flag is clear.
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand.lo);
      cpu.regs.pc += signedOffset;
    }
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest beq(Mos6502& cpu, Common::BusResponse response)
  {
    if (cpu.regs.p & Mos6502::Zero)
    {
      // BEQ assumes that two values were compared by subtracting one from the other, or, that a loop counter was
      // decremented. This means that the the zero flag would be set if they were equal, or the counter reached zero.
      // Since this is Branch if EQual, take the branch only if the zero flag is set.
      int8_t signedOffset = static_cast<int8_t>(cpu.m_operand.lo);
      cpu.regs.pc += signedOffset;
    }
    return Mos6502::nextOp(cpu, response);
  }

  template<Mos6502::Register reg>
  static Common::BusRequest load(Mos6502& cpu, Common::BusResponse response)
  {
    // This is a generic load operation for A, X, or Y registers.

    auto data = cpu.sel<reg>(cpu.regs) = response.data;
    cpu.regs.setZN(data);
    return Mos6502::nextOp(cpu, response);
  }

  template<Mos6502::Register reg>
    requires(reg != Mos6502::Register::A)
  static Common::BusRequest increment(Mos6502& cpu, Common::BusResponse response)
  {
    // Handle increment operation for X or Y registers
    auto& r = cpu.sel<reg>(cpu.regs);
    ++r;
    cpu.regs.setZN(r);
    return Mos6502::nextOp(cpu, response);
  }

  template<Mos6502::Register reg>
    requires(reg != Mos6502::Register::A)
  static Common::BusRequest decrement(Mos6502& cpu, Common::BusResponse response)
  {
    auto& r = cpu.sel<reg>(cpu.regs);
    --r;
    cpu.regs.setZN(r);
    return Mos6502::nextOp(cpu, response);
  }

  static bool subtractWithBorrow(Byte& reg, Byte value) noexcept
  {
    uint16_t diff = uint16_t(reg) - uint16_t(value);
    reg = Byte(diff & 0xFF);
    bool borrow = (diff & 0x100) != 0;
    return borrow;
  }

  template<Mos6502::Register reg>
  static Common::BusRequest compare(Mos6502& cpu, Common::BusResponse response)
  {
    auto& r = cpu.sel<reg>(cpu.regs);
    bool borrow = subtractWithBorrow(r, response.data);
    cpu.regs.set(Mos6502::Carry, !borrow);
    cpu.regs.setZN(r);
    return Mos6502::nextOp(cpu, response);
  }

  template<Mos6502::Register src, Mos6502::Register dst>
    requires(src != dst)
  static Common::BusRequest transfer(Mos6502& cpu, Common::BusResponse response)
  {
    auto s = cpu.sel<src>(cpu.regs);
    auto& d = cpu.sel<dst>(cpu.regs);
    d = s;
    cpu.regs.setZN(d);
    return Mos6502::nextOp(cpu, response);
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
  table[0xA9] = {"LDA", {&Mode::imm, &Operations::load<Mos6502::Register::A>}};
  table[0xA5] = {"LDA", {Mode::zp, &Mode::Fetch,&Operations::load<Mos6502::Register::A>}};
  table[0xB5] = {"LDA", {Mode::zpX, &Mode::Fetch,&Operations::load<Mos6502::Register::A>}};
  table[0xAD] = {"LDA", {Mode::abs, &Mode::Fetch, &Operations::load<Mos6502::Register::A>}};
  table[0xBD] = {"LDA", {Mode::absX, &Mode::Fetch, &Operations::load<Mos6502::Register::A>}};
  table[0xB9] = {"LDA", {Mode::absY, &Mode::Fetch, &Operations::load<Mos6502::Register::A>}};
  table[0xA1] = {"LDA", {&Mode::indirect<Index::X>, &Operations::load<Mos6502::Register::A>}};
  table[0xB1] = {"LDA", {&Mode::indirect<Index::Y>, &Operations::load<Mos6502::Register::A>}};

  // LDX instructions
  table[0xA2] = {"LDX", {&Mode::imm, &Operations::load<Mos6502::Register::X>}};
  table[0xA6] = {"LDX", {Mode::zp, &Mode::Fetch,&Operations::load<Mos6502::Register::X>}};
  table[0xB6] = {"LDX", {Mode::zpY, &Mode::Fetch,&Operations::load<Mos6502::Register::X>}};
  table[0xAE] = {"LDX", {Mode::abs, &Mode::Fetch, &Operations::load<Mos6502::Register::X>}};
  table[0xBE] = {"LDX", {Mode::absY, &Mode::Fetch, &Operations::load<Mos6502::Register::X>}};

  // LDY instructions
  table[0xA0] = {"LDY", {&Mode::imm, &Operations::load<Mos6502::Register::Y>}};
  table[0xA4] = {"LDY", {Mode::zp, &Mode::Fetch,&Operations::load<Mos6502::Register::Y>}};
  table[0xB4] = {"LDY", {Mode::zpX, &Mode::Fetch,&Operations::load<Mos6502::Register::Y>}};
  table[0xAC] = {"LDY", {Mode::abs, &Mode::Fetch, &Operations::load<Mos6502::Register::Y>}};
  table[0xBC] = {"LDY", {Mode::absX, &Mode::Fetch, &Operations::load<Mos6502::Register::Y>}};

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
  table[0xE8] = {"INX", {&Mode::imp  , &Operations::increment<Mos6502::Register::X>}};
  table[0xC8] = {"INY", {&Mode::imp  , &Operations::increment<Mos6502::Register::Y>}};
  table[0xCA] = {"DEX", {&Mode::imp  , &Operations::decrement<Mos6502::Register::X>}};
  table[0x88] = {"DEY", {&Mode::imp  , &Operations::decrement<Mos6502::Register::Y>}};

  // CMP — Compare Accumulator
  table[0xC9] = {"CMP", {&Mode::imm, &Operations::compare<Mos6502::Register::A>}};
  table[0xC5] = {"CMP", {Mode::zp, &Mode::Fetch,&Operations::compare<Mos6502::Register::A>}};
  table[0xD5] = {"CMP", {Mode::zpX, &Mode::Fetch,&Operations::compare<Mos6502::Register::A>}};
  table[0xCD] = {"CMP", {Mode::abs, &Mode::Fetch, &Operations::compare<Mos6502::Register::A>}};
  table[0xDD] = {"CMP", {Mode::absX, &Mode::Fetch, &Operations::compare<Mos6502::Register::A>}};
  table[0xD9] = {"CMP", {Mode::absY, &Mode::Fetch, &Operations::compare<Mos6502::Register::A>}};
  table[0xC1] = {"CMP", {&Mode::indirect<Index::X>, &Operations::compare<Mos6502::Register::A>}};
  table[0xD1] = {"CMP", {&Mode::indirect<Index::Y>, &Operations::compare<Mos6502::Register::A>}};

  // CPX — Compare X Register
  table[0xE0] = {"CPX", {&Mode::imm, &Operations::compare<Mos6502::Register::X>}};
  table[0xE4] = {"CPX", {Mode::zp, &Mode::Fetch,&Operations::compare<Mos6502::Register::X>}};
  table[0xEC] = {"CPX", {Mode::abs, &Mode::Fetch, &Operations::compare<Mos6502::Register::X>}};

  // CPY — Compare Y Register
  table[0xC0] = {"CPY", {&Mode::imm, &Operations::compare<Mos6502::Register::Y>}};
  table[0xC4] = {"CPY", {Mode::zp, &Mode::Fetch,&Operations::compare<Mos6502::Register::Y>}};
  table[0xCC] = {"CPY", {Mode::abs, &Mode::Fetch, &Operations::compare<Mos6502::Register::Y>}};

  table[0x98] = {"TYA", {Mode::imp, &Operations::transfer<Mos6502::Register::Y, Mos6502::Register::A>}};
  table[0xA8] = {"TAY", {Mode::imp, &Operations::transfer<Mos6502::Register::A, Mos6502::Register::Y>}};
  table[0x8A] = {"TXA", {Mode::imp, &Operations::transfer<Mos6502::Register::X, Mos6502::Register::A>}};
  table[0xAA] = {"TAX", {Mode::imp, &Operations::transfer<Mos6502::Register::A, Mos6502::Register::X>}};
  // table[0xBA] = {"TSX", {Mode::imp, &Operations::transfer<Mos6502::Register::S, Mos6502::Register::X>}};
  // table[0x9A] = {"TXS", {Mode::imp, &Operations::transfer<Mos6502::Register::X, Mos6502::Register::S>}};
  // Add more instructions as needed

  // clang-format on
  return table;
}();

Mos6502::Mos6502() noexcept = default;

Common::BusRequest Mos6502::Tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  assert(m_action);

  // Execute the current action until it calls nextOp.
  m_lastBusRequest = m_action(*this, response);

  return m_lastBusRequest;
}

BusRequest Mos6502::fetchNextOpcode(Mos6502& cpu, BusResponse /*response*/)
{
  cpu.m_tracer.addRegisters(cpu.regs.pc, cpu.regs.a, cpu.regs.x, cpu.regs.y, cpu.regs.p, cpu.regs.sp);

  // Fetch the next opcode
  cpu.m_action = &Mos6502::decodeOpcode;
  return BusRequest::Fetch(cpu.regs.pc++);
}

BusRequest Mos6502::decodeOpcode(Mos6502& cpu, BusResponse response)
{
  // Decode opcode
  Byte opcode = response.data;
  const auto& instruction = c_instructions[static_cast<size_t>(opcode)];
  if (instruction.op[0] == nullptr)
  {
    throw std::runtime_error(std::format("Unknown opcode: ${:02X} at PC=${:04X}\n", opcode, cpu.regs.pc - 1));
  }

  cpu.setInstruction(instruction);
  return cpu.m_action(cpu, BusResponse{});
}

void Mos6502::setInstruction(const Instruction& instr) noexcept
{
  m_instruction = &instr;

  m_tracer.addInstruction(static_cast<Byte>(std::distance(&c_instructions[0], &instr)), instr.name);

  m_stage = 0;

  assert(m_instruction);
  m_action = m_instruction->op[0];
  assert(m_action);
}

BusRequest Mos6502::nextOp(Mos6502& cpu, BusResponse response)
{
  assert(cpu.m_instruction);

  // Find the next action in the instruction's operation sequence.
  // If there are no more actions, finish the operation.
  do
  {
    ++cpu.m_stage;
  } while (cpu.m_stage < std::size(cpu.m_instruction->op) && !cpu.m_instruction->op[cpu.m_stage]);
  if (cpu.m_stage >= std::size(cpu.m_instruction->op))
  {
    // Instruction complete, log the last instruction.

    cpu.m_instruction = nullptr;
    cpu.m_action = &Mos6502::fetchNextOpcode;
    return fetchNextOpcode(cpu, BusResponse{});
  }

  cpu.m_action = cpu.m_instruction->op[cpu.m_stage];
  return cpu.m_action(cpu, response);
}
