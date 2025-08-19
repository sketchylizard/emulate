#include "AddressMode.h"

Common::Bus AddressMode::accumulator(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  assert(step == 0);
  cpu.m_log.setOperand("A");
  return cpu.StartOperation(bus);
}

Common::Bus AddressMode::implied(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  assert(step == 0);
  return cpu.StartOperation(bus);
}

Common::Bus AddressMode::immediate(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    return Common::Bus::Read(cpu.m_pc++);
  }

  cpu.m_operand = bus.data;

  char buffer[] = "#$XX";
  std::format_to(buffer + 2, "{:02X}", bus.data);
  cpu.m_log.setOperand(buffer);
  return cpu.StartOperation(bus);
}

Common::Bus AddressMode::relative(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    return Common::Bus::Read(cpu.m_pc++);
  }

  cpu.m_operand = bus.data;

  char buffer[] = "$XX";
  std::format_to(buffer + 1, "{:02X}", cpu.m_operand);
  cpu.m_log.setOperand(buffer);

  return cpu.StartOperation(bus);
}
