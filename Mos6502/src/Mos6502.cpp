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

inline Address Add(Address lhs, uint16_t rhs)
{
  return Address{static_cast<uint16_t>(static_cast<uint16_t>(lhs) + rhs)};
}

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
    assert(m_byteCount == 0);

    // load new instruction
    m_bytes[m_byteCount++] = bus.data;
    DecodeNextInstruction(bus.data);
  }

  // Startup is a special case, we have no instruction or action
  if (m_instruction != nullptr)
  {
    assert(m_action);

    // Execute the current action until it returns a nullptr.
    m_action = m_action(*this, bus, m_step++).func;
  }

  // If the action is nullptr, the operation is complete
  if (!m_action)
  {
    Log();

    // Instruction complete, fetch the next instruction
    m_pcStart = m_pc;
    m_byteCount = 0;
    m_step = 0;
    bus.address = m_pc++;
    bus.control = Control::Read | Control::Sync;
  }
  return bus;
}

void Mos6502::DecodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  m_instruction = &c_instructions[static_cast<size_t>(opcode)];
  assert(m_instruction);
  m_action = m_instruction->addressMode;
  assert(m_action);
  m_step = 0;
}

Mos6502::State Mos6502::StartOperation()
{
  m_step = 0;
  return {m_instruction->operation};
}

void Mos6502::Log() const
{
#ifdef MOS6502_TRACE
  // Don't log if we don't have an instruction.
  if (!m_instruction)
    return;

  // PC at start of instruction
  std::string pcStr = std::format("{:04X}", m_pcStart);

  // Get formatted instruction text (opcode, operands, mnemonic, data)
  std::string instrStr = FormatOperands();

  // Registers
  std::string regStr = std::format("A:{:02X} X:{:02X} Y:{:02X} P:{:02X} SP:{:02X}", m_a, m_x, m_y, m_status, m_sp);

  // Output the full line
  std::cout << std::format("{}  {}  {}\n", pcStr, instrStr, regStr);

#endif
}

std::string Mos6502::FormatOperands() const
{
  std::string result;

  // print raw bytes first (there could be as many as 4 bytes, fill space if less)
  for (size_t i = 0; i != c_maxBytes; ++i)
  {
    if (i < m_byteCount)
      result += std::format("{:02X} ", m_bytes[i]);
    else
      result += "   ";
  }

  result.append(m_instruction->name);
  result.append(" ");

  // Now print operands (and data) based on the addressing mode.

  // Implied / accumulator instructions
  if (m_instruction->addressMode == &Mos6502::implied)
  {
    // nothing to do
  }
  else if (m_instruction->addressMode == &Mos6502::accumulator)
  {
    result.append("A");
  }
  // Immediate
  else if (m_instruction->addressMode == &Mos6502::immediate)
  {
    assert(m_byteCount == 2);  // opcode + operand
    result += std::format("#${:02X}", m_bytes[1]);
  }
  // Zero Page
  else if (m_instruction->addressMode == &Mos6502::zero_page<>)
  {
    assert(m_byteCount == 3);  // opcode + operand + data
    result += std::format("${:02X}   = {:02X}", m_bytes[1], m_bytes[2]);
  }
  else if (m_instruction->addressMode == &Mos6502::zero_page<Index::X>)
  {
    assert(m_byteCount == 3);  // opcode + operand + data
    result += std::format("${:02X},X = {:02X}", m_bytes[1], m_bytes[2]);
  }
  else if (m_instruction->addressMode == &Mos6502::zero_page<Index::Y>)
  {
    assert(m_byteCount == 3);  // opcode + operand + data
    result += std::format("${:02X},Y = {:02X}", m_bytes[1], m_bytes[2]);
  }
  // Absolute
  else if (m_instruction->addressMode == &Mos6502::absolute<>)
  {
    assert(m_byteCount == 3);  // opcode + lo byte + hi byte + data
    result += std::format("${:02X}{:02X}", m_bytes[2], m_bytes[1]);
  }
  else if (m_instruction->addressMode == &Mos6502::absolute<Index::X>)
  {
    assert(m_byteCount == 4);  // opcode + lo byte + hi byte + data
    result += std::format("${:02X}{:02X},X = {:02X}", m_bytes[2], m_bytes[1], m_bytes[3]);
  }
  else if (m_instruction->addressMode == &Mos6502::absolute<Index::Y>)
  {
    assert(m_byteCount == 4);  // opcode + lo byte + hi byte + data
    result += std::format("${:02X}{:02X},Y = {:02X}", m_bytes[2], m_bytes[1], m_bytes[3]);
  }

  // Pad to ~36 chars for nestest alignment
  if (result.size() < 36)
    result.resize(36, ' ');

  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Addressing modes and operations
////////////////////////////////////////////////////////////////////////////////

Mos6502::State Mos6502::accumulator(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle accumulator addressing mode
  assert(step == 0);
  // Since there are no operands, we should go straight into processing the operation.
  return cpu.m_instruction->operation(cpu, bus, step);
}

Mos6502::State Mos6502::implied(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle implied addressing mode
  assert(step == 0);
  // Since there are no operands, we should go straight into processing the operation.
  return cpu.m_instruction->operation(cpu, bus, step);
}

Mos6502::State Mos6502::immediate(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle immediate addressing mode
  assert(step == 0);
  bus.address = cpu.m_pc++;
  bus.control = Control::Read;

  return cpu.StartOperation();
}

// BRK, NMI, IRQ, and Reset operations all share similar logic for pushing the PC and status onto the stack
// and setting the PC to the appropriate vector address. This function handles that logic.
// It returns true when the operation is complete. If forceRead is true, it force the bus to READ rather than WRITE mode
// and the writes to the stack will be ignored.
Mos6502::State Mos6502::doReset(Mos6502& cpu, Bus& bus, size_t step, bool forceRead, Address vector)
{
  Control control = forceRead ? Control::Read : Control{};

  switch (step)
  {
    case 0:
      // Push PC high byte
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = HiByte(cpu.pc());
      bus.control = control;
      return {cpu.m_action};
    case 1:
      // Push PC low byte
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = LoByte(cpu.pc());
      bus.control = control;
      return {cpu.m_action};
    case 2:
      // Push status register
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = cpu.status();
      bus.control = control;
      return {cpu.m_action};
    case 3:
      // Fetch the low byte of the interrupt/reset vector
      bus.address = vector;
      bus.control = Control::Read;
      return {cpu.m_action};
    case 4:
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      // Store the lo byte and fetch the high byte of the interrupt/reset vector
      bus.address = vector + 1;
      bus.control = Control::Read;
      return {cpu.m_action};
    case 5:
      // Set PC to the interrupt/reset vector address
      cpu.m_bytes[cpu.m_byteCount++] = bus.data;
      cpu.m_pc = (MakeAddress(cpu.m_bytes[1], cpu.m_bytes[2]));
      return {nullptr};  // Operation complete

    default: assert(false && "Invalid step for BRK instruction"); return {nullptr};
  }
}

Mos6502::State Mos6502::brk(Mos6502& cpu, Bus& bus, size_t step)
{
  return doReset(cpu, bus, step, false, c_brkVector);
}

Mos6502::State Mos6502::adc(Mos6502& cpu, Bus& bus, size_t step)
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
  return {nullptr};  // Operation complete
}

Mos6502::State Mos6502::cld(Mos6502& cpu, Bus& /*bus*/, size_t step)
{
  assert(step == 0);
  cpu.SetFlag(Mos6502::Decimal, false);
  return {nullptr};
}

Mos6502::State Mos6502::txs(Mos6502& cpu, Bus& /*bus*/, size_t step)
{
  assert(step == 0);
  cpu.set_sp(cpu.x());
  return {nullptr};
}

Mos6502::State Mos6502::sta(Mos6502& cpu, Bus& bus, size_t step)
{
  if (step == 0)
  {
    bus.address = cpu.m_effectiveAddress;
    bus.data = cpu.a();
    bus.control = Control{} /*Write*/;
    return {&Mos6502::sta};
  }
  return {nullptr};
}

Mos6502::State Mos6502::ora(Mos6502& cpu, Bus& bus, size_t step)
{
  assert(step == 0);
  // Perform OR with accumulator
  cpu.m_a |= bus.data;
  cpu.SetFlag(Mos6502::Zero, cpu.m_a == 0);  // Set zero flag
  cpu.SetFlag(Mos6502::Negative, cpu.m_a & 0x80);  // Set negative flag
  return {nullptr};
}
