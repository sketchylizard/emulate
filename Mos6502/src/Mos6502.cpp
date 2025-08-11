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
  ++m_current.tickCount;

  // If Sync is set, we are fetching a new instruction
  if ((bus.control & Control::Sync) != Control::None)
  {
    // load new instruction
    decodeNextInstruction(bus.data);
  }

  assert(m_current.instruction);
  assert(m_current.action);

  if (m_current.action(*this, bus, m_current.step++))
  {
    // Step complete
    if (m_current.action == m_current.instruction->addressMode)
    {
      // Move to operation
      m_current.action = m_current.instruction->operation;
      m_current.step = 0;
    }
    else
    {
      // Operation complete, log the instruction
      // PC, Opcode, Byte1, Byte2, mnemonic, Address/Value, A, X, Y, SP, Status
      std::cout << std::format("{:04X}  {:02X} {:02X} {:02X}  {}     "
                               "A:{:02X} X:{:02X} Y:{:02X} P:{:02X} SP:{:02X} CYC: {}\n",
          m_current.pc, m_current.instruction->opcode, m_current.byte1, m_current.byte2, m_current.instruction->name,
          a(), x(), y(), status(), sp(), m_current.tickCount);
      // Instruction complete, fetch the next instruction
      m_current.byte1 = 0;
      m_current.byte2 = 0;
      bus.address = m_current.pc++;
      bus.control = Control::Read | Control::Sync;
    }
  }
  return bus;
}

void Mos6502::decodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  auto it = std::find_if(std::begin(instructions), std::end(instructions),
      [opcode](const Instruction& instr) { return instr.opcode == opcode; });
  if (it != std::end(instructions))
  {
    auto action = it->addressMode ? it->addressMode : it->operation;
    m_current.instruction = &*it;
    m_current.action = action;
    m_current.step = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Addressing modes and operations
////////////////////////////////////////////////////////////////////////////////
bool Mos6502::immediate(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle immediate addressing mode
  assert(step == 0);
  bus.address = cpu.m_current.pc++;
  bus.control = Control::Read;
  return true;
}

bool Mos6502::zero_page(Mos6502& cpu, Bus& bus, size_t step)
{
  return zero_page_indexed(cpu, bus, step, 0);
}

bool Mos6502::zero_page_x(Mos6502& cpu, Bus& bus, size_t step)
{
  return zero_page_indexed(cpu, bus, step, cpu.m_current.x);
}

bool Mos6502::zero_page_y(Mos6502& cpu, Bus& bus, size_t step)
{
  return zero_page_indexed(cpu, bus, step, cpu.m_current.y);
}

bool Mos6502::zero_page_indexed(Mos6502& cpu, Bus& bus, size_t step, Byte index)
{
  // Handle zero-page X addressing mode
  switch (step)
  {
    case 0:
      // Fetch the zero-page address
      bus.address = cpu.m_current.pc++;
      bus.control = Control::Read;
      return false;  // Need another step to read the data
    case 1:
      // Read the data from zero-page address
      cpu.m_current.byte1 = bus.data;
      // Add our index register (if this overflows, it wraps around in zero-page, this is the desired behavior.)
      cpu.m_current.byte2 = static_cast<Byte>(bus.data + static_cast<int8_t>(index));
      // Set bus address for the next step
      bus.address = MakeAddress(cpu.m_current.byte2, c_ZeroPage);
      bus.control = Control::Read;
      return false;
    default: assert(false && "Invalid step for zero-page index addressing mode"); return true;
  }
}

bool Mos6502::absolute_indexed(Mos6502& cpu, Bus& bus, size_t step, Byte index)
{
  switch (step)
  {
    case 0:
      // Fetch low byte
      bus.address = cpu.m_current.pc++;
      bus.control = Control::Read;
      return false;
    case 1:
      // Store low byte and fetch high byte
      cpu.m_current.byte1 = bus.data;
      bus.address = cpu.m_current.pc++;
      bus.control = Control::Read;
      return false;
    case 2:
      // Fetch high byte and calculate address
      cpu.m_current.byte2 = bus.data;
      // Set bus address for the next step
      bus.address = MakeAddress(cpu.m_current.byte1 + index, cpu.m_current.byte2);
      bus.control = Control::Read;
      return true;
    default: assert(false && "Invalid step for absolute addressing mode"); return true;
  }
}

bool Mos6502::absolute(Mos6502& cpu, Bus& bus, size_t step)
{
  return absolute_indexed(cpu, bus, step, 0);
}

bool Mos6502::absolute_x(Mos6502& cpu, Bus& bus, size_t step)
{
  return absolute_indexed(cpu, bus, step, cpu.m_current.x);
}

bool Mos6502::absolute_y(Mos6502& cpu, Bus& bus, size_t step)
{
  return absolute_indexed(cpu, bus, step, cpu.m_current.y);
}

bool Mos6502::indirect(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return true;
}

bool Mos6502::indirect_x(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return true;
}

bool Mos6502::indirect_y(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return true;
}

std::string Mos6502::FormatOperands(Action& addressingMode, Byte byte1, Byte byte2) noexcept
{
  if (addressingMode == &Mos6502::immediate)
    return std::format("#${:02X}", byte1);
  if (addressingMode == &Mos6502::zero_page)
    return std::format("${:02X}", byte1);
  if (addressingMode == &Mos6502::zero_page_x)
    return std::format("${:02X},X", byte1);
  if (addressingMode == &Mos6502::zero_page_y)
    return std::format("${:02X},Y", byte1);
  if (addressingMode == &Mos6502::absolute)
    return std::format("${:02X}{:02X}", byte2, byte1);
  if (addressingMode == &Mos6502::absolute_x)
    return std::format("${:02X}{:02X},X", byte2, byte1);
  if (addressingMode == &Mos6502::absolute_y)
    return std::format("${:02X}{:02X},Y", byte2, byte1);
  if (addressingMode == &Mos6502::indirect)
    return std::format("(${:02X}{:02X})", byte2, byte1);
  if (addressingMode == &Mos6502::indirect_x)
    return std::format("(${:02X}{:02X},X)", byte2, byte1);
  if (addressingMode == &Mos6502::indirect_y)
    return std::format("(${:02X}{:02X},Y)", byte2, byte1);
  assert(false && "Unknown addressing mode");
  return {};
}

// BRK, NMI, IRQ, and Reset operations all share similar logic for pushing the PC and status onto the stack
// and setting the PC to the appropriate vector address. This function handles that logic.
// It returns true when the operation is complete. If forceRead is true, it force the bus to READ rather than WRITE mode
// and the writes to the stack will be ignored.
bool Mos6502::doReset(Mos6502& cpu, Bus& bus, size_t step, bool forceRead, Address vector)
{
  Control control = forceRead ? Control::Read : Control{};

  switch (step)
  {
    case 0:
      // Push PC high byte
      bus.address = MakeAddress(cpu.m_current.sp--, c_StackPage);
      bus.data = HiByte(cpu.pc());
      bus.control = control;
      return false;  // Need another step to push low byte
    case 1:
      // Push PC low byte
      bus.address = MakeAddress(cpu.m_current.sp--, c_StackPage);
      bus.data = LoByte(cpu.pc());
      bus.control = control;
      return false;  // Need another step to push status
    case 2:
      // Push status register
      bus.address = MakeAddress(cpu.m_current.sp--, c_StackPage);
      bus.data = cpu.status();
      bus.control = control;
      return false;  // Need another step to set PC to reset vector
    case 3:
      // Fetch the low byte of the interrupt/reset vector
      bus.address = vector;
      bus.control = Control::Read;
      return false;  // Need another step to fetch high byte
    case 4:
      cpu.m_current.byte1 = bus.data;
      // Store the lo byte and fetch the high byte of the interrupt/reset vector
      bus.address = vector + 1;
      bus.control = Control::Read;
      return false;  // Need another step to set PC
    case 5:
      // Set PC to the interrupt/reset vector address
      cpu.m_current.byte2 = bus.data;
      cpu.m_current.pc = (MakeAddress(cpu.m_current.byte1, cpu.m_current.byte2));
      return true;  // BRK instruction complete
    default: assert(false && "Invalid step for BRK instruction"); return true;
  }
}

bool Mos6502::brk(Mos6502& cpu, Bus& bus, size_t step)
{
  return doReset(cpu, bus, step, false, c_brkVector);
}

bool Mos6502::adc(Mos6502& cpu, Bus& bus, size_t step)
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
  return true;
}
