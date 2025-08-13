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

char Mos6502::trace_buffer[80];

inline Address Add(Address lhs, uint16_t rhs)
{
  return Address{static_cast<uint16_t>(static_cast<uint16_t>(lhs) + rhs)};
}

Mos6502::Mos6502() noexcept
{
  decodeNextInstruction(0x00);  // BRK/RST
}

Bus Mos6502::Tick(Bus bus) noexcept
{
  ++m_tickCount;

  // If Sync is set, we are fetching a new instruction
  if ((bus.control & Control::Sync) != Control::None)
  {
    // load new instruction
    decodeNextInstruction(bus.data);
  }

  assert(m_instruction);
  assert(m_action);

  // Execute the current action until it returns a nullptr.
  m_action = m_action(*this, bus, m_step++).func;

  // If the action is nullptr, the operation is complete
  if (!m_action)
  {
    // Instruction complete, fetch the next instruction
    bus.address = m_pc++;
    bus.control = Control::Read | Control::Sync;
  }
  return bus;
}

void Mos6502::decodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  m_instruction = &c_instructions[static_cast<size_t>(opcode)];
  assert(m_instruction);
  m_action = m_instruction->addressMode ? m_instruction->addressMode : m_instruction->operation;
  assert(m_action);
  m_step = 0;
}

Mos6502::State Mos6502::FinishOperation() noexcept
{
  // Operation complete, log the instruction
  // PC, Opcode, Byte1, Byte2, mnemonic, Address/Value, A, X, Y, SP, Status
#ifdef MOS6502_TRACE
  // Format the trace line

  // Print PC as 4 hex digits
  auto offset = std::snprintf(trace_buffer, sizeof(trace_buffer), "%04X  %-3s %02X ",  //
      (static_cast<uint16_t>(m_pc) - m_instruction->bytes),  //
      m_instruction->name.data(),  //
      m_instruction->opcode);

  // Print operand bytes or spaces for alignment
  if (m_instruction->bytes == 1)
  {
    // 1 byte operand
    offset += std::snprintf(trace_buffer + offset, sizeof(trace_buffer) - static_cast<size_t>(offset), "%02X   ", m_byte1);
  }
  else if (m_instruction->bytes == 2)
  {
    // 2 byte operand
    offset += std::snprintf(
        trace_buffer + offset, sizeof(trace_buffer) - static_cast<size_t>(offset), "%02X %02X", m_byte1, m_byte2);
  }
  else
  {
    // No operand bytes
    offset += std::snprintf(trace_buffer + offset, sizeof(trace_buffer) - static_cast<size_t>(offset), "     ");
  }

  // Print registers
  offset += std::snprintf(trace_buffer + offset, sizeof(trace_buffer) - static_cast<size_t>(offset),
      "  A:%02X X:%02X Y:%02X P:%02X SP:%02X\n", a(), x(), y(), status(), sp());

  // Output the trace line
  std::fputs(trace_buffer, stdout);
#endif

  m_step = 0;
  m_byte1 = 0;
  m_byte2 = 0;

  return {nullptr};
}

////////////////////////////////////////////////////////////////////////////////
// Addressing modes and operations
////////////////////////////////////////////////////////////////////////////////
Mos6502::State Mos6502::immediate(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle immediate addressing mode
  assert(step == 0);
  bus.address = cpu.m_pc++;
  bus.control = Control::Read;
  return cpu.StartOperation();  // Start the operation
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
      return cpu.CurrentState();
    case 1:
      // Push PC low byte
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = LoByte(cpu.pc());
      bus.control = control;
      return cpu.CurrentState();
    case 2:
      // Push status register
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = cpu.status();
      bus.control = control;
      return cpu.CurrentState();
    case 3:
      // Fetch the low byte of the interrupt/reset vector
      bus.address = vector;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 4:
      cpu.m_byte1 = bus.data;
      // Store the lo byte and fetch the high byte of the interrupt/reset vector
      bus.address = vector + 1;
      bus.control = Control::Read;
      return cpu.CurrentState();
    case 5:
      // Set PC to the interrupt/reset vector address
      cpu.m_byte2 = bus.data;
      cpu.m_pc = (MakeAddress(cpu.m_byte1, cpu.m_byte2));
      return cpu.FinishOperation();  // Operation complete

    default: assert(false && "Invalid step for BRK instruction"); return cpu.FinishOperation();
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
  return cpu.FinishOperation();  // Operation complete
}

Mos6502::State Mos6502::cld(Mos6502& cpu, Bus& /*bus*/, size_t step)
{
  assert(step == 0);
  cpu.SetFlag(Mos6502::Decimal, false);
  return cpu.FinishOperation();
}

Mos6502::State Mos6502::txs(Mos6502& cpu, Bus& /*bus*/, size_t step)
{
  assert(step == 0);
  cpu.set_sp(cpu.x());
  return cpu.FinishOperation();
}

Mos6502::State Mos6502::sta(Mos6502& cpu, Bus& bus, size_t step)
{
  assert(step == 0);
  bus.address = cpu.m_pc++;
  bus.data = cpu.a();
  bus.control = Control{} /*Write*/;
  return cpu.FinishOperation();
}

Mos6502::State Mos6502::ora(Mos6502& cpu, Bus& bus, size_t step)
{
  assert(step == 0);
  // Perform OR with accumulator
  cpu.m_a |= bus.data;
  cpu.SetFlag(Mos6502::Zero, cpu.m_a == 0);  // Set zero flag
  cpu.SetFlag(Mos6502::Negative, cpu.m_a & 0x80);  // Set negative flag
  return cpu.FinishOperation();
}
