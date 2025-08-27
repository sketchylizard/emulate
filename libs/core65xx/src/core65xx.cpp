#include "core65xx/core65xx.h"

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

#include "common/Bus.h"
#include "core65xx/address_mode.h"

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
  static Common::BusRequest brk(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};

    cpu.m_action = &Operations::brk1<ForceRead>;
    // Push PC high byte
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), Common::HiByte(cpu.regs.pc), control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk1(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};
    cpu.m_action = &Operations::brk2<ForceRead>;
    // Push PC low byte
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), Common::LoByte(cpu.regs.pc), control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk2(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    Control control = ForceRead ? Control::Read : Control{};
    cpu.m_action = &Operations::brk3<ForceRead>;
    // Push status register
    return Common::BusRequest::Write(Common::MakeAddress(cpu.regs.sp--, c_StackPage), cpu.regs.p, control);
  }
  template<bool ForceRead>
  static Common::BusRequest brk3(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    // Fetch the low byte of the interrupt/reset vector
    cpu.m_action = &Operations::brk4<ForceRead>;
    return Common::BusRequest::Read(Core65xx::c_brkVector);
  }
  template<bool ForceRead>
  static Common::BusRequest brk4(Core65xx& cpu, Common::BusResponse response)
  {
    // Store the lo byte of the vector
    cpu.m_operand.lo = response.data;
    cpu.m_action = &Operations::brk5<ForceRead>;
    // Fetch the high byte of the interrupt/reset vector
    return Common::BusRequest::Read(Core65xx::c_brkVector + 1);
  }
  template<bool ForceRead>
  static Common::BusRequest brk5(Core65xx& cpu, Common::BusResponse response)
  {
    // Set PC to the interrupt/reset vector address
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Core65xx::nextOp(cpu, response);  // Operation complete
  }

  static Common::BusRequest adc(Core65xx& cpu, Common::BusResponse response)
  {
    assert(cpu.regs.has(Core65xx::Flag::Decimal) == false);  // BCD mode not supported

    const Byte a = cpu.regs.a;
    const Byte m = response.data;
    const Byte c = cpu.regs.has(Core65xx::Flag::Carry) ? 1 : 0;

    const uint16_t sum = uint16_t(a) + uint16_t(m) + uint16_t(c);
    const Byte result = Byte(sum & 0xFF);

    cpu.regs.set(Core65xx::Flag::Carry, (sum & 0x100) != 0);
    cpu.regs.set(Core65xx::Flag::Overflow, ((~(a ^ m) & (a ^ result)) & 0x80) != 0);
    cpu.regs.setZN(result);

    cpu.regs.a = result;
    return Core65xx::nextOp(cpu, response);
  }

  template<Core65xx::Flag Flag, bool Set>
  static Common::BusRequest flagOp(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.regs.set(Flag, Set);
    return Core65xx::nextOp(cpu, response);
  }


  static Common::BusRequest txs(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.regs.sp = cpu.regs.x;
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest sta(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Core65xx::nextOp;
    return Common::BusRequest::Write(cpu.getEffectiveAddress(), cpu.regs.a);
  }

  static Common::BusRequest ora(Core65xx& cpu, Common::BusResponse response)
  {
    // Perform OR with accumulator
    cpu.regs.a |= response.data;
    cpu.regs.set(Core65xx::Flag::Zero, cpu.regs.a == 0);  // Set zero flag
    cpu.regs.set(Core65xx::Flag::Negative, cpu.regs.a & 0x80);  // Set negative flag
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest jmpAbsolute(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpAbsolute1;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch low byte
  }

  static Common::BusRequest jmpAbsolute1(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;

    cpu.m_action = &Operations::jmpAbsolute2;
    return Common::BusRequest::Read(cpu.regs.pc++);  // Fetch high byte
  }

  static Common::BusRequest jmpAbsolute2(Core65xx& cpu, Common::BusResponse response)
  {
    // Step 2
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest jmpIndirect(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &Operations::jmpIndirect1;
    // Fetch indirect address low byte
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  static Common::BusRequest jmpIndirect1(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;
    cpu.m_action = &Operations::jmpIndirect2;
    return BusRequest::Read(cpu.regs.pc++);  // fetch ptr high
  }

  static Common::BusRequest jmpIndirect2(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.m_operand.hi = response.data;
    cpu.m_action = &Operations::jmpIndirect3;
    return BusRequest::Read(cpu.getEffectiveAddress());  // Read target low byte
  }

  static Common::BusRequest jmpIndirect3(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.m_operand.lo = response.data;

    // 6502 bug: high byte read wraps within the *same* page as ptr
    cpu.m_operand.hi = 0;  // this is wrong
    cpu.m_action = &Operations::jmpIndirect4;
    return BusRequest::Read(cpu.getEffectiveAddress());  // read target high (buggy wrap)
  }

  static Common::BusRequest jmpIndirect4(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.m_operand.hi = response.data;
    cpu.regs.pc = cpu.getEffectiveAddress();
    return Core65xx::nextOp(cpu, response);
  }

  // Branch step 0: Evaluate condition and decide whether to branch
  template<Core65xx::Flag flag, bool condition>
  static Common::BusRequest branch0(Core65xx& cpu, Common::BusResponse response)
  {
    // Check if the flag matches the desired condition
    bool flagSet = cpu.regs.has(flag);
    bool shouldBranch = (flagSet == condition);

    if (!shouldBranch)
    {
      // Branch not taken - 2 cycles total, we're done
      return Core65xx::nextOp(cpu, response);
    }
    else
    {
      // Branch taken - continue to step 1
      cpu.m_action = &Operations::branch1;
      return BusRequest::Read(cpu.regs.pc);
    }
  }

  // Branch step 1: Add offset to PC low byte, check for page crossing
  static Common::BusRequest branch1(Core65xx& cpu, Common::BusResponse response)
  {
    int8_t offset = static_cast<int8_t>(cpu.m_operand.lo);

    // Add offset to PC low byte
    auto tmp = static_cast<int16_t>(static_cast<uint16_t>(cpu.regs.pc) & 0xFF) + offset;
    bool carry = tmp < 0 || tmp > 0xFF;  // Detect if page boundary is crossed

    // Update PC with new low byte (keep high byte for now)
    cpu.regs.pc = MakeAddress(static_cast<uint8_t>(tmp), HiByte(cpu.regs.pc));

    if (!carry)
    {
      // No page boundary crossed - 3 cycles total, we're done
      // PC is already correct
      return Core65xx::nextOp(cpu, response);
    }
    else
    {
      // Page boundary crossed - need step 2 for fixup
      cpu.m_action = &Operations::branch2;
      return Common::BusRequest::Read(cpu.regs.pc);  // Dummy read to consume cycle
    }
  }

  // Branch step 2: Fix up PC high byte after page boundary crossing
  static Common::BusRequest branch2(Core65xx& cpu, Common::BusResponse response)
  {
    // Apply the carry/borrow to the high byte
    if (cpu.m_operand.lo & 0x80)
    {
      // Negative offset - decrement high byte
      cpu.regs.pc -= 0x100;
    }
    else
    {
      // Positive offset - increment high byte
      cpu.regs.pc += 0x100;
    }
    // 4 cycles total, we're done
    return Core65xx::nextOp(cpu, response);
  }

  // Specific branch instruction implementations
  static Common::BusRequest bpl(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Negative, false>(cpu, response);  // Branch if N=0
  }

  static Common::BusRequest bmi(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Negative, true>(cpu, response);  // Branch if N=1
  }

  static Common::BusRequest bcc(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Carry, false>(cpu, response);  // Branch if C=0
  }

  static Common::BusRequest bcs(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Carry, true>(cpu, response);  // Branch if C=1
  }

  static Common::BusRequest bne(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Zero, false>(cpu, response);  // Branch if Z=0
  }

  static Common::BusRequest beq(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Zero, true>(cpu, response);  // Branch if Z=1
  }

  static Common::BusRequest bvc(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Overflow, false>(cpu, response);  // Branch if V=0
  }

  static Common::BusRequest bvs(Core65xx& cpu, Common::BusResponse response)
  {
    return branch0<Core65xx::Flag::Overflow, true>(cpu, response);  // Branch if V=1
  }

  template<Core65xx::Register reg>
  static Common::BusRequest load(Core65xx& cpu, Common::BusResponse response)
  {
    // This is a generic load operation for A, X, or Y registers.

    auto data = cpu.sel<reg>(cpu.regs) = response.data;
    cpu.regs.setZN(data);
    return Core65xx::nextOp(cpu, response);
  }

  template<Core65xx::Register reg>
    requires(reg != Core65xx::Register::A)
  static Common::BusRequest increment(Core65xx& cpu, Common::BusResponse response)
  {
    // Handle increment operation for X or Y registers
    auto& r = cpu.sel<reg>(cpu.regs);
    ++r;
    cpu.regs.setZN(r);
    return Core65xx::nextOp(cpu, response);
  }

  template<Core65xx::Register reg>
    requires(reg != Core65xx::Register::A)
  static Common::BusRequest decrement(Core65xx& cpu, Common::BusResponse response)
  {
    auto& r = cpu.sel<reg>(cpu.regs);
    --r;
    cpu.regs.setZN(r);
    return Core65xx::nextOp(cpu, response);
  }

  static bool subtractWithBorrow(Byte& reg, Byte value) noexcept
  {
    uint16_t diff = uint16_t(reg) - uint16_t(value);
    reg = Byte(diff & 0xFF);
    bool borrow = (diff & 0x100) != 0;
    return borrow;
  }

  template<Core65xx::Register reg>
  static Common::BusRequest compare(Core65xx& cpu, Common::BusResponse response)
  {
    auto r = cpu.sel<reg>(cpu.regs);
    bool borrow = subtractWithBorrow(r, response.data);
    cpu.regs.set(Core65xx::Flag::Carry, !borrow);
    cpu.regs.setZN(r);
    return Core65xx::nextOp(cpu, response);
  }

  template<Core65xx::Register src, Core65xx::Register dst>
    requires(src != dst)
  static Common::BusRequest transfer(Core65xx& cpu, Common::BusResponse response)
  {
    auto s = cpu.sel<src>(cpu.regs);
    auto& d = cpu.sel<dst>(cpu.regs);
    d = s;
    cpu.regs.setZN(d);
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest eor(Core65xx& cpu, Common::BusResponse response)
  {
    cpu.regs.a ^= response.data;  // A ← A ⊕ M
    cpu.regs.setZN(cpu.regs.a);
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest pha(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = [](Core65xx& cpu1, Common::BusResponse /*response1*/)
    {
      cpu1.m_action = &Core65xx::nextOp;
      cpu1.m_operand.lo = cpu1.regs.sp--;
      cpu1.m_operand.hi = c_StackPage;
      return Common::BusRequest::Write(cpu1.getEffectiveAddress(), cpu1.regs.a);
    };
    return Common::BusRequest::Read(cpu.regs.pc);  // Dummy read to consume cycle
  }

  static Common::BusRequest pla(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = [](Core65xx& cpu1, Common::BusResponse /*response1*/)
    {
      cpu1.m_action = [](Core65xx& cpu2, Common::BusResponse response2)
      {
        cpu2.regs.a = response2.data;
        cpu2.regs.setZN(cpu2.regs.a);
        return Core65xx::nextOp(cpu2, response2);
      };
      cpu1.m_operand.lo = cpu1.regs.sp;
      cpu1.m_operand.hi = c_StackPage;
      return Common::BusRequest::Write(cpu1.getEffectiveAddress(), cpu1.regs.a);
    };
    cpu.regs.sp++;
    return Common::BusRequest::Read(cpu.regs.pc);  // Dummy read to consume cycle
  }

};  // struct Operations

////////////////////////////////////////////////////////////////////////////////
// CPU implementation
////////////////////////////////////////////////////////////////////////////////
static constexpr std::array<Core65xx::Instruction, 256> c_instructions = []
{
  std::array<Core65xx::Instruction, 256> table{};
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
  table[0xA9] = {"LDA", {&Mode::imm, &Operations::load<Core65xx::Register::A>}};
  table[0xA5] = {"LDA", {Mode::zp, &Mode::Fetch,&Operations::load<Core65xx::Register::A>}};
  table[0xB5] = {"LDA", {Mode::zpX, &Mode::Fetch,&Operations::load<Core65xx::Register::A>}};
  table[0xAD] = {"LDA", {Mode::abs, &Mode::Fetch, &Operations::load<Core65xx::Register::A>}};
  table[0xBD] = {"LDA", {Mode::absX, &Mode::Fetch, &Operations::load<Core65xx::Register::A>}};
  table[0xB9] = {"LDA", {Mode::absY, &Mode::Fetch, &Operations::load<Core65xx::Register::A>}};
  table[0xA1] = {"LDA", {&Mode::indirect<Index::X>, &Operations::load<Core65xx::Register::A>}};
  table[0xB1] = {"LDA", {&Mode::indirect<Index::Y>, &Operations::load<Core65xx::Register::A>}};

  // LDX instructions
  table[0xA2] = {"LDX", {&Mode::imm, &Operations::load<Core65xx::Register::X>}};
  table[0xA6] = {"LDX", {Mode::zp, &Mode::Fetch,&Operations::load<Core65xx::Register::X>}};
  table[0xB6] = {"LDX", {Mode::zpY, &Mode::Fetch,&Operations::load<Core65xx::Register::X>}};
  table[0xAE] = {"LDX", {Mode::abs, &Mode::Fetch, &Operations::load<Core65xx::Register::X>}};
  table[0xBE] = {"LDX", {Mode::absY, &Mode::Fetch, &Operations::load<Core65xx::Register::X>}};

  // LDY instructions
  table[0xA0] = {"LDY", {&Mode::imm, &Operations::load<Core65xx::Register::Y>}};
  table[0xA4] = {"LDY", {Mode::zp, &Mode::Fetch,&Operations::load<Core65xx::Register::Y>}};
  table[0xB4] = {"LDY", {Mode::zpX, &Mode::Fetch,&Operations::load<Core65xx::Register::Y>}};
  table[0xAC] = {"LDY", {Mode::abs, &Mode::Fetch, &Operations::load<Core65xx::Register::Y>}};
  table[0xBC] = {"LDY", {Mode::absX, &Mode::Fetch, &Operations::load<Core65xx::Register::Y>}};

  // Flag operations
  table[0x18] = {"CLC", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Carry,     false>}};
  table[0x38] = {"SEC", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Carry,      true>}};
  table[0x58] = {"CLI", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Interrupt, false>}};
  table[0x78] = {"SEI", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Interrupt,  true>}};
  table[0xB8] = {"CLV", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Overflow,  false>}};
  table[0xD8] = {"CLD", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Decimal,   false>}};
  table[0xF8] = {"SED", {&Mode::imp, &Operations::flagOp<Core65xx::Flag::Decimal,    true>}};

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
  table[0x10] = {"BPL", {&Mode::rel  , &Operations::bpl}};
  table[0x30] = {"BMI", {&Mode::rel  , &Operations::bmi}};
  table[0x90] = {"BCC", {&Mode::rel  , &Operations::bcc}};
  table[0xB0] = {"BCS", {&Mode::rel  , &Operations::bcs}};
  table[0x50] = {"BVC", {&Mode::rel  , &Operations::bvc}};
  table[0x70] = {"BVS", {&Mode::rel  , &Operations::bvs}};

  // Increment and Decrement instructions
  table[0xE8] = {"INX", {&Mode::imp  , &Operations::increment<Core65xx::Register::X>}};
  table[0xC8] = {"INY", {&Mode::imp  , &Operations::increment<Core65xx::Register::Y>}};
  table[0xCA] = {"DEX", {&Mode::imp  , &Operations::decrement<Core65xx::Register::X>}};
  table[0x88] = {"DEY", {&Mode::imp  , &Operations::decrement<Core65xx::Register::Y>}};

  // CMP — Compare Accumulator
  table[0xC9] = {"CMP", {&Mode::imm, &Operations::compare<Core65xx::Register::A>}};
  table[0xC5] = {"CMP", {Mode::zp, &Mode::Fetch,&Operations::compare<Core65xx::Register::A>}};
  table[0xD5] = {"CMP", {Mode::zpX, &Mode::Fetch,&Operations::compare<Core65xx::Register::A>}};
  table[0xCD] = {"CMP", {Mode::abs, &Mode::Fetch, &Operations::compare<Core65xx::Register::A>}};
  table[0xDD] = {"CMP", {Mode::absX, &Mode::Fetch, &Operations::compare<Core65xx::Register::A>}};
  table[0xD9] = {"CMP", {Mode::absY, &Mode::Fetch, &Operations::compare<Core65xx::Register::A>}};
  table[0xC1] = {"CMP", {&Mode::indirect<Index::X>, &Operations::compare<Core65xx::Register::A>}};
  table[0xD1] = {"CMP", {&Mode::indirect<Index::Y>, &Operations::compare<Core65xx::Register::A>}};

  // CPX — Compare X Register
  table[0xE0] = {"CPX", {&Mode::imm, &Operations::compare<Core65xx::Register::X>}};
  table[0xE4] = {"CPX", {Mode::zp, &Mode::Fetch,&Operations::compare<Core65xx::Register::X>}};
  table[0xEC] = {"CPX", {Mode::abs, &Mode::Fetch, &Operations::compare<Core65xx::Register::X>}};

  // CPY — Compare Y Register
  table[0xC0] = {"CPY", {&Mode::imm, &Operations::compare<Core65xx::Register::Y>}};
  table[0xC4] = {"CPY", {Mode::zp, &Mode::Fetch,&Operations::compare<Core65xx::Register::Y>}};
  table[0xCC] = {"CPY", {Mode::abs, &Mode::Fetch, &Operations::compare<Core65xx::Register::Y>}};

  table[0x98] = {"TYA", {Mode::imp, &Operations::transfer<Core65xx::Register::Y, Core65xx::Register::A>}};
  table[0xA8] = {"TAY", {Mode::imp, &Operations::transfer<Core65xx::Register::A, Core65xx::Register::Y>}};
  table[0x8A] = {"TXA", {Mode::imp, &Operations::transfer<Core65xx::Register::X, Core65xx::Register::A>}};
  table[0xAA] = {"TAX", {Mode::imp, &Operations::transfer<Core65xx::Register::A, Core65xx::Register::X>}};
  table[0x9A] = {"TXS", {&Mode::imp, &Operations::txs}};
  // table[0xBA] = {"TSX", {Mode::imp, &Operations::transfer<Core65xx::Register::S, Core65xx::Register::X>}};

  table[0x49] = {"EOR", {&Mode::imm,  &Operations::eor}};
  table[0x45] = {"EOR", {Mode::zp,    &Mode::Fetch, &Operations::eor}};
  table[0x55] = {"EOR", {Mode::zpX,   &Mode::Fetch, &Operations::eor}};
  table[0x4D] = {"EOR", {Mode::abs,   &Mode::Fetch, &Operations::eor}};
  table[0x5D] = {"EOR", {Mode::absX,  &Mode::Fetch, &Operations::eor}};
  table[0x59] = {"EOR", {Mode::absY,  &Mode::Fetch, &Operations::eor}};
  table[0x41] = {"EOR", {&Mode::indirect<Index::X>, &Operations::eor}};
  table[0x51] = {"EOR", {&Mode::indirect<Index::Y>, &Operations::eor}};

  table[0x48] = {"PHA", {&Mode::imp, &Operations::pha}};
  table[0x68] = {"PLA", {&Mode::imp, &Operations::pla}};

  // Add more instructions as needed

  // clang-format on
  return table;
}();

Core65xx::Core65xx() noexcept = default;

Common::BusRequest Core65xx::Tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  assert(m_action);

  // Execute the current action until it calls nextOp.
  m_lastBusRequest = m_action(*this, response);

  return m_lastBusRequest;
}

BusRequest Core65xx::fetchNextOpcode(Core65xx& cpu, BusResponse /*response*/)
{
  // Fetch the next opcode
  cpu.m_action = &Core65xx::decodeOpcode;
  return BusRequest::Fetch(cpu.regs.pc++);
}

BusRequest Core65xx::decodeOpcode(Core65xx& cpu, BusResponse response)
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

void Core65xx::setInstruction(const Instruction& instr) noexcept
{
  m_instruction = &instr;

  m_stage = 0;

  assert(m_instruction);
  m_action = m_instruction->op[0];
  assert(m_action);
}

BusRequest Core65xx::nextOp(Core65xx& cpu, BusResponse response)
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
    cpu.m_action = &Core65xx::fetchNextOpcode;
    return fetchNextOpcode(cpu, BusResponse{});
  }

  cpu.m_action = cpu.m_instruction->op[cpu.m_stage];
  return cpu.m_action(cpu, response);
}
