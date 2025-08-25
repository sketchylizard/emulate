#include "AddressMode.h"

Common::BusRequest AddressMode::acc(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand.type = Mos6502::Operand::Type::Acc;
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imp(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand.type = Mos6502::Operand::Type::Impl;
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imm(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_operand.type = Mos6502::Operand::Type::Imm8;
  cpu.m_action = &AddressMode::imm1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::imm1(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand.lo = response.data;
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::rel(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_operand.type = Mos6502::Operand::Type::Rel;
  // Fetch the signed 8-bit displacement, then PC will point to the next opcode.
  cpu.m_action = &AddressMode::rel1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::rel1(Mos6502& cpu, Common::BusResponse response)
{
  // Stash the raw displacement byte for the branch op
  cpu.m_operand.lo = response.data;
  cpu.m_operand.hi = 0;
  // Hand off to the branch operation (BEQ/BNE/etc.)
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::Fetch(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = [](Mos6502& cpu1, Common::BusResponse response1)
  {
    cpu1.m_operand.lo = response1.data;
    return Mos6502::nextOp(cpu1, response1);
  };
  return Common::BusRequest::Read(cpu.getEffectiveAddress());
}
