#include "cpu6502/address_mode.h"

namespace cpu6502
{

Common::BusRequest AddressMode::requestOpcode(State& cpu, Common::BusResponse /*response*/)
{
  // Fetch the next opcode
  return Common::BusRequest::Fetch(cpu.pc++);
}

Common::BusRequest AddressMode::requestOperandLow(State& cpu, Common::BusResponse /*response*/)
{
  // We don't care about the response; just fetch the low byte of the address
  cpu.lo = cpu.hi = 0;  // clear hi/lo to ensure they are set
  return Common::BusRequest::Read(cpu.pc++);
}

Common::BusRequest AddressMode::requestOperandHigh(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the low byte of the address
  cpu.lo = response.data;
  return Common::BusRequest::Read(cpu.pc++);
}

Common::BusRequest AddressMode::requestEffectiveAddress(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the high byte of the address
  cpu.hi = response.data;
  return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi));
}

Common::BusRequest AddressMode::requestZeroPageAddress(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the low byte of the address (the high byte is always 0)
  cpu.lo = response.data;
  return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00));
}

#if 0

Common::BusRequest AddressMode::acc(Core65xx& cpu, Common::BusResponse response)
{
  cpu.m_operand.type = Core65xx::Operand::Type::Acc;
  return Core65xx::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imp(Core65xx& cpu, Common::BusResponse response)
{
  cpu.m_operand.type = Core65xx::Operand::Type::Impl;
  return Core65xx::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imm(Core65xx& cpu, Common::BusResponse /*response*/)
{
  cpu.m_operand.type = Core65xx::Operand::Type::Imm8;
  cpu.m_action = &AddressMode::imm1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::imm1(Core65xx& cpu, Common::BusResponse response)
{
  cpu.m_operand.lo = response.data;
  return Core65xx::nextOp(cpu, response);
}

Common::BusRequest AddressMode::rel(Core65xx& cpu, Common::BusResponse /*response*/)
{
  cpu.m_operand.type = Core65xx::Operand::Type::Rel;
  // Fetch the signed 8-bit displacement, then PC will point to the next opcode.
  cpu.m_action = &AddressMode::rel1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::rel1(Core65xx& cpu, Common::BusResponse response)
{
  // Stash the raw displacement byte for the branch op
  cpu.m_operand.lo = response.data;
  cpu.m_operand.hi = 0;
  // Hand off to the branch operation (BEQ/BNE/etc.)
  return Core65xx::nextOp(cpu, response);
}

Common::BusRequest AddressMode::Fetch(Core65xx& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = [](Core65xx& cpu1, Common::BusResponse response1)
  {
    cpu1.m_operand.lo = response1.data;
    return Core65xx::nextOp(cpu1, response1);
  };
  return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi));
}
#endif

}  // namespace cpu6502
