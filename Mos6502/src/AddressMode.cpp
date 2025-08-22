#include "AddressMode.h"

Common::BusRequest AddressMode::acc(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_log.setOperand("A");
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::imp(Mos6502& cpu, Common::BusResponse response)
{
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::imm(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = &AddressMode::imm1;
  return Common::BusRequest::Read(cpu.m_pc++);
}

Common::BusRequest AddressMode::imm1(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand = response.data;

  char buffer[] = "#$XX";
  std::format_to(buffer + 2, "{:02X}", response.data);
  cpu.m_log.setOperand(buffer);
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::rel(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = &AddressMode::imm1;
  return Common::BusRequest::Read(cpu.m_pc++);
}

Common::BusRequest AddressMode::rel1(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand = response.data;

  char buffer[] = "$XX";
  std::format_to(buffer + 1, "{:02X}", cpu.m_operand);
  cpu.m_log.setOperand(buffer);

  return cpu.StartOperation(response);
}
