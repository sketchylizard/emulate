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
  auto it = std::find_if(std::begin(c_instructions), std::end(c_instructions),
      [opcode](const Instruction& instr) { return instr.opcode == opcode; });
  if (it != std::end(c_instructions))
  {
    auto action = it->addressMode ? it->addressMode : it->operation;
    m_instruction = &*it;
    m_action = action;
    m_step = 0;
  }
}

Mos6502::State Mos6502::FinishOperation() noexcept
{
  // Operation complete, log the instruction
  // PC, Opcode, Byte1, Byte2, mnemonic, Address/Value, A, X, Y, SP, Status
  std::cout << std::format("{:04X}  {:02X} {:02X} {:02X}  {}     "
                           "A:{:02X} X:{:02X} Y:{:02X} P:{:02X} SP:{:02X} CYC: {}\n",
      m_pc, m_instruction->opcode, m_byte1, m_byte2, m_instruction->name, a(), x(), y(), status(), sp(), m_tickCount);

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

std::string Mos6502::FormatOperands(StateFunc& addressingMode, Byte byte1, Byte byte2) noexcept
{
  if (addressingMode == &Mos6502::immediate)
    return std::format("#${:02X}", byte1);
  if (addressingMode == &Mos6502::zero_page<Index::None>)
    return std::format("${:02X}", byte1);
  if (addressingMode == &Mos6502::zero_page<Index::X>)
    return std::format("${:02X},X", byte1);
  if (addressingMode == &Mos6502::zero_page<Index::Y>)
    return std::format("${:02X},Y", byte1);
  if (addressingMode == &Mos6502::absolute<Index::None>)
    return std::format("${:02X}{:02X}", byte2, byte1);
  if (addressingMode == &Mos6502::absolute<Index::X>)
    return std::format("${:02X}{:02X},X", byte2, byte1);
  if (addressingMode == &Mos6502::absolute<Index::Y>)
    return std::format("${:02X}{:02X},Y", byte2, byte1);
  if (addressingMode == &Mos6502::indirect<Index::None>)
    return std::format("(${:02X}{:02X})", byte2, byte1);
  if (addressingMode == &Mos6502::indirect<Index::X>)
    return std::format("(${:02X}{:02X},X)", byte2, byte1);
  if (addressingMode == &Mos6502::indirect<Index::Y>)
    return std::format("(${:02X}{:02X},Y)", byte2, byte1);
  assert(false && "Unknown addressing mode");
  return {};
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
