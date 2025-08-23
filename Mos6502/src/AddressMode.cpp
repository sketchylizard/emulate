#include "AddressMode.h"

Common::BusRequest AddressMode::acc(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_log.setOperand("A");
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imp(Mos6502& cpu, Common::BusResponse response)
{
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::imm(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = &AddressMode::imm1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::imm1(Mos6502& cpu, Common::BusResponse response)
{
  cpu.m_operand = response.data;

  char buffer[] = "#$XX";
  std::format_to(buffer + 2, "{:02X}", response.data);
  cpu.m_log.setOperand(buffer);
  return Mos6502::nextOp(cpu, response);
}

void AddressMode::logRelOperand(Mos6502& cpu, Common::Byte displacement)
{
  // 1) Log the raw byte (so your disassembly shows the encoded displacement)
  cpu.m_log.addByte(displacement, 0);

  // 2) Compute final target = (PC after reading displacement) + signed(displacement)
  const int8_t off = static_cast<int8_t>(displacement);
  const auto base = cpu.regs.pc;  // PC already incremented in rel()
  const auto target = Common::Address(static_cast<uint16_t>(static_cast<uint16_t>(base) + static_cast<int16_t>(off)));

  // 3) Show the resolved absolute target in the operand text (e.g., "BEQ $C012")
  char buf[] = "$XXXX";
  std::format_to(buf + 1, "{:04X}", target);
  cpu.m_log.setOperand(buf);

  // If you prefer showing both displacement and target, you can widen the buffer, e.g.:
  // char buf2[] = "±dd->$XXXX"; // then format signed decimal and the target
}

Common::BusRequest AddressMode::rel(Mos6502& cpu, Common::BusResponse /*response*/)
{
  // Fetch the signed 8-bit displacement, then PC will point to the next opcode.
  cpu.m_action = &AddressMode::rel1;
  return Common::BusRequest::Read(cpu.regs.pc++);
}

Common::BusRequest AddressMode::rel1(Mos6502& cpu, Common::BusResponse response)
{
  // Stash the raw displacement byte for the branch op
  cpu.m_operand = response.data;

  // Log the byte and pretty-print the final absolute target
  AddressMode::logRelOperand(cpu, response.data);

  // Hand off to the branch operation (BEQ/BNE/etc.)
  return Mos6502::nextOp(cpu, response);
}

Common::BusRequest AddressMode::Fetch(Mos6502& cpu, Common::BusResponse /*response*/)
{
  cpu.m_action = [](Mos6502& cpu, Common::BusResponse response)
  {
    cpu.m_operand = response.data;
    return Mos6502::nextOp(cpu, response);
  };
  return Common::BusRequest::Read(cpu.m_target);
}
