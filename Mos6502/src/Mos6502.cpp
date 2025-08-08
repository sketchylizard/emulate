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

using Byte = Mos6502::Byte;
using Address = Mos6502::Address;

inline Address Add(Address lhs, uint16_t rhs)
{
  return Address{static_cast<uint16_t>(lhs) + rhs};
}

Byte Mos6502::read_memory(Address address) const noexcept
{
  return m_memory[static_cast<uint16_t>(address)];
}

void Mos6502::write_memory(Address address, Byte value) noexcept
{
  m_memory[static_cast<uint16_t>(address)] = value;
}

void Mos6502::write_memory(Address address, std::span<const Byte> bytes) noexcept
{
  assert(m_memory.size() - bytes.size() > static_cast<std::size_t>(address));

  auto offset = m_memory.begin() + static_cast<std::size_t>(address);
  std::ranges::copy(bytes, offset);
}

void Mos6502::step() noexcept
{
  ++m_cycles;

  if (!m_current.instruction)
  {
    decodeNextInstruction();
    return;
  }

  // We have a current instruction, so perform the next step
  assert(m_current.instruction);
  assert(m_current.action);

  if (m_current.action(*this, m_current.cycle++))
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
      // Instruction complete
      m_current = {};
    }
  }
}

void Mos6502::reset() noexcept
{
  m_pc = c_resetVector;
  auto lo = fetch();
  auto hi = fetch();
  m_pc = Address{static_cast<uint16_t>(hi) << 8 | lo};

  m_aRegister = 0;
  m_xRegister = 0;
  m_yRegister = 0;
  m_sp = 0;
  m_status = 0;  // Clear all flags
  m_current = {};
}

Byte Mos6502::fetch() noexcept
{
  Byte data = read_memory(m_pc);
  m_pc = Address{static_cast<uint16_t>(m_pc) + 1};
  return data;
}

void Mos6502::decodeNextInstruction() noexcept
{
  assert(!m_current.instruction);

  // Fetch opcode
  Byte opcode = fetch();

  // Decode opcode
  auto it = std::find_if(std::begin(instructions), std::end(instructions),
      [opcode](const Instruction& instr) { return instr.opcode == opcode; });
  if (it != std::end(instructions))
  {
    m_current = {&*it, it->addressMode, 0};
  }
}

////////////////////////////////////////////////////////////////////////////////
// Addressing modes and operations
////////////////////////////////////////////////////////////////////////////////
bool Mos6502::implied(Mos6502&, size_t step)
{
  // We are done immediately
  static_cast<void>(step);  // Suppress unused variable warning
  assert(step == 0);
  return true;
}

bool Mos6502::immediate(Mos6502& cpu, size_t step)
{
  // Handle immediate addressing mode
  assert(step == 0);
  cpu.m_operand = cpu.fetch();
  return true;
}

bool Mos6502::zero_page(Mos6502& cpu, size_t step)
{
  // Handle zero-page addressing mode
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch())};
  return true;
}

bool Mos6502::zero_page_x(Mos6502& cpu, size_t step)
{
  // Handle zero-page X addressing mode
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch() + cpu.x())};
  return true;
}

bool Mos6502::zero_page_y(Mos6502& cpu, size_t step)
{
  // Handle zero-page Y addressing mode
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch() + cpu.y())};
  return true;
}

bool Mos6502::absolute(Mos6502& cpu, size_t step)
{
  switch (step)
  {
    case 0:
      // Fetch low byte
      cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch())};
      return false;  // Need another step to fetch high byte
    case 1:
      // Fetch high byte and calculate address
      cpu.m_address = Address{
          (static_cast<uint16_t>(cpu.fetch()) << 8)  // hi byte
          | static_cast<uint16_t>(cpu.m_address)  // lo byte
      };
      return true;
    default: assert(false && "Invalid step for absolute addressing mode"); return true;
  }
}

bool Mos6502::absolute_x(Mos6502& cpu, size_t step)
{
  switch (step)
  {
    case 0: return absolute(cpu, step);
    case 1:
      absolute(cpu, step);
      {
        auto address = static_cast<uint16_t>(cpu.m_address);
        auto addressWithOffset = address + cpu.x();
        cpu.m_address = Address{addressWithOffset};
        // If we crossed a page boundary, requires an extra cycle
        return ((addressWithOffset & 0xff00) == (address & 0xff00));
      }
    case 2:
      // Extra step after crossing page boundary
      return true;
    default: assert(false && "Invalid step for absolute X addressing mode"); return true;
  }
  // Now we have the address, apply X offset
  cpu.m_address = Address{static_cast<uint16_t>(cpu.m_address) + cpu.x()};
  return true;
}

bool Mos6502::absolute_y(Mos6502& cpu, size_t step)
{
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch() + cpu.y())};
  return true;
}

bool Mos6502::indirect(Mos6502& cpu, size_t step)
{
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch())};
  return true;
}

bool Mos6502::indirect_x(Mos6502& cpu, size_t step)
{
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch() + cpu.x())};
  return true;
}

bool Mos6502::indirect_y(Mos6502& cpu, size_t step)
{
  assert(step == 0);
  cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch() + cpu.y())};
  return true;
}

bool Mos6502::adc(Mos6502& cpu, size_t step)
{
  // Handle ADC operation
  assert(step == 0);
  Byte operand = cpu.m_operand;
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
