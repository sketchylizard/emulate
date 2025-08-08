#include "Mos6502/Mos6502.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include "Mos6502/Bus.h"

inline Address Add(Address lhs, uint16_t rhs)
{
  return Address{static_cast<uint16_t>(static_cast<uint16_t>(lhs) + rhs)};
}

Bus Mos6502::Tick(Bus bus) noexcept
{
  ++m_cycles;

  // If Sync is set, we are fetching a new instruction
  if ((bus.control & Control::Sync) != Control::None)
  {
    // load new instruction
    decodeNextInstruction(bus.data);
  }

  assert(m_current.instruction);
  assert(m_current.action);

  if (m_current.action(*this, bus, m_current.cycle++))
  {
    // Step complete
    if (m_current.action == m_current.instruction->addressMode)
    {
      // Move to operation
      m_current.action = m_current.instruction->operation;
      m_current.cycle = 0;
    }
    else
    {
      // Instruction complete, fetch the next instruction
      bus.address = m_pc++;
      bus.control = Control::Read | Control::Sync;
    }
  }
  return bus;
}

void Mos6502::reset() noexcept {}

void Mos6502::decodeNextInstruction(Byte opcode) noexcept
{
  // Decode opcode
  auto it = std::find_if(std::begin(instructions), std::end(instructions),
      [opcode](const Instruction& instr) { return instr.opcode == opcode; });
  if (it != std::end(instructions))
  {
    auto action = it->addressMode ? it->addressMode : it->operation;
    m_current = {&*it, action, 0};
  }
}

////////////////////////////////////////////////////////////////////////////////
// Addressing modes and operations
////////////////////////////////////////////////////////////////////////////////
bool Mos6502::immediate(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle immediate addressing mode
  assert(step == 0);
  bus.address = cpu.m_pc++;
  bus.control = Control::Read;
  return true;
}

bool Mos6502::zero_page(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle zero-page addressing mode
  switch (step)
  {
    case 0:
      // Fetch the zero-page address
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;  // Need another step to read the data
    case 1:
      // Read the data from zero-page address
      cpu.m_address = MakeAddress(bus.data, 0);
      // Set bus address for the next step
      bus.address = cpu.m_address;
      bus.control = Control::Read;
      return true;
    default: assert(false && "Invalid step for zero-page addressing mode"); return true;
  }
  return true;
}

bool Mos6502::zero_page_x(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle zero-page X addressing mode
  switch (step)
  {
    case 0:
      // Fetch the zero-page address
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;  // Need another step to read the data
    case 1:
      // Read the data from zero-page address
      cpu.m_address = MakeAddress(bus.data, 0) + static_cast<int8_t>(cpu.m_x);
      // Set bus address for the next step
      bus.address = cpu.m_address;
      bus.control = Control::Read;
      return false;
    case 2: cpu.m_operand = bus.data; return true;
    default: assert(false && "Invalid step for zero-page X addressing mode"); return true;
  }
}

bool Mos6502::zero_page_y(Mos6502& cpu, Bus& bus, size_t step)
{
  // Handle zero-page Y addressing mode
  switch (step)
  {
    case 0:
      // Fetch the zero-page address
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;  // Need another step to read the data
    case 1:
      // Read the data from zero-page address
      cpu.m_address = MakeAddress(bus.data, 0) + static_cast<int8_t>(cpu.m_y);
      // Set bus address for the next step
      bus.address = cpu.m_address;
      bus.control = Control::Read;
      return false;
    case 2: cpu.m_operand = bus.data; return true;
    default: assert(false && "Invalid step for zero-page X addressing mode"); return true;
  }
}

bool Mos6502::absolute(Mos6502& cpu, Bus& bus, size_t step)
{
  switch (step)
  {
    case 0:
      // Fetch low byte
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;
    case 1:
      // Store low byte and fetch high byte
      cpu.m_operand = bus.data;
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;
    case 2:
      // Fetch high byte and calculate address
      bus.address = MakeAddress(cpu.m_operand, bus.data);
      bus.control = Control::Read;
      return true;
    default: assert(false && "Invalid step for absolute addressing mode"); return true;
  }
}

bool Mos6502::absolute_x(Mos6502& cpu, Bus& bus, size_t step)
{
  switch (step)
  {
    case 0:
      // Fetch low byte
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;
    case 1:
      // Store low byte and fetch high byte
      cpu.m_operand = bus.data;
      bus.address = cpu.m_pc++;
      bus.control = Control::Read;
      return false;
    case 2:
      // Fetch high byte and calculate address
      bus.address = MakeAddress(cpu.m_operand, bus.data) + static_cast<int8_t>(cpu.m_x);
      bus.control = Control::Read;
      return true;
    default: assert(false && "Invalid step for absolute addressing mode"); return true;
  }
}

bool Mos6502::absolute_y(Mos6502& cpu, Bus& bus, size_t step)
{
  // suppress unused variable warning
  static_cast<void>(cpu);
  static_cast<void>(bus);
  static_cast<void>(step);
  return true;
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

bool Mos6502::brk(Mos6502& cpu, Bus& bus, size_t step)
{
  switch (step)
  {
    case 0:
      // Push PC high byte
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = HiByte(cpu.pc());
      bus.control = Control{};  // Write;
      return false;  // Need another step to push low byte
    case 1:
      // Push PC low byte
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = LoByte(cpu.pc());
      bus.control = Control{};  // Write;
      return false;  // Need another step to push status
    case 2:
      // Push status register
      bus.address = MakeAddress(cpu.m_sp--, c_StackPage);
      bus.data = cpu.status();
      bus.control = Control{};  // Write;
      return false;  // Need another step to set PC to reset vector
    case 3:
      // Fetch the low byte of the interrupt vector
      bus.address = c_brkVector;
      bus.control = Control::Read;
      return false;  // Need another step to fetch high byte
    case 4:
      cpu.m_operand = bus.data;
      // Fetch the high byte of the interrupt vector and set PC
      bus.address = c_brkVector + 1;
      bus.control = Control::Read;
      return false;  // Need another step to set PC
    case 5:
      // Set PC to the interrupt vector address
      cpu.set_pc(MakeAddress(cpu.m_operand, bus.data));
      return true;  // BRK instruction complete
    default: assert(false && "Invalid step for BRK instruction"); return true;
  }
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
