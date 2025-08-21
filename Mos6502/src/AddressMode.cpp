#include "AddressMode.h"

Common::BusRequest AddressMode::accumulator(Mos6502& cpu, Common::BusResponse response, Common::Byte step)
{
  assert(step == 0);
  cpu.m_log.setOperand("A");
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::implied(Mos6502& cpu, Common::BusResponse response, Common::Byte step)
{
  assert(step == 0);
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::immediate(Mos6502& cpu, Common::BusResponse response, Common::Byte step)
{
  if (step == 0)
  {
    return Common::BusRequest::Read(cpu.m_pc++);
  }

  cpu.m_operand = response.data;

  char buffer[] = "#$XX";
  std::format_to(buffer + 2, "{:02X}", response.data);
  cpu.m_log.setOperand(buffer);
  return cpu.StartOperation(response);
}

Common::BusRequest AddressMode::relative(Mos6502& cpu, Common::BusResponse response, Common::Byte step)
{
  if (step == 0)
  {
    return Common::BusRequest::Read(cpu.m_pc++);
  }

  cpu.m_operand = response.data;

  char buffer[] = "$XX";
  std::format_to(buffer + 1, "{:02X}", cpu.m_operand);
  cpu.m_log.setOperand(buffer);

  return cpu.StartOperation(response);
}
