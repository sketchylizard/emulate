#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

class Mos6502
{
public:
  using Byte = uint8_t;
  enum class Address
  {
  };


  constexpr Mos6502()
  {
    reset();
  }

  [[nodiscard]] constexpr Byte a() const noexcept
  {
    return m_aRegister;
  }
  constexpr void set_a(Byte v) noexcept
  {
    m_aRegister = v;
  }

  [[nodiscard]] constexpr Byte x() const noexcept
  {
    return m_xRegister;
  }
  constexpr void set_x(Byte v) noexcept
  {
    m_xRegister = v;
  }

  [[nodiscard]] constexpr Byte y() const noexcept
  {
    return m_yRegister;
  }
  constexpr void set_y(Byte v) noexcept
  {
    m_yRegister = v;
  }

  [[nodiscard]] constexpr Byte sp() const noexcept
  {
    return m_sp;
  }
  constexpr void set_sp(Byte v) noexcept
  {
    m_sp = v;
  }

  [[nodiscard]] constexpr Address pc() const noexcept
  {
    return m_pc;
  }
  constexpr void set_pc(Address addr) noexcept
  {
    m_pc = addr;
  }

  [[nodiscard]] constexpr Byte status() const noexcept
  {
    return m_status;
  }

  constexpr void set_status(Byte flags) noexcept
  {
    m_status = flags;
  }

  [[nodiscard]] constexpr Byte read_memory(Address address) const noexcept
  {
    return m_memory[static_cast<uint16_t>(address)];
  }

  constexpr void write_memory(Address address, Byte value) noexcept
  {
    m_memory[static_cast<uint16_t>(address)] = value;
  }

  constexpr void write_memory(Address address, std::span<const Byte> bytes) noexcept
  {
    assert(m_memory.size() - bytes.size() > static_cast<std::size_t>(address));

    auto offset = m_memory.begin() + static_cast<std::size_t>(address);
    std::ranges::copy(bytes, offset);
  }

  constexpr void step() noexcept
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

  void reset() noexcept
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

private:
  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  //! Action should return true if the instruction is complete
  using Action = bool (*)(Mos6502&, size_t step);

  struct Instruction
  {
    std::string_view name;
    Byte opcode;
    Action addressMode;
    Action operation;
  };

  static constexpr bool implied(Mos6502&, size_t step)
  {
    // We are done immediately
    static_cast<void>(step);  // Suppress unused variable warning
    assert(step == 0);
    return true;
  }

  static constexpr bool zero_page(Mos6502& cpu, size_t step)
  {
    // Handle zero-page addressing mode
    assert(step == 0);
    cpu.m_address = Address{static_cast<uint16_t>(cpu.fetch())};
    return true;
  }

  static constexpr bool immediate(Mos6502& cpu, size_t step)
  {
    // Handle immediate addressing mode
    assert(step == 0);
    cpu.m_operand = cpu.fetch();
    return true;
  }

  static constexpr bool adc(Mos6502& cpu, size_t step)
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

  static constexpr Instruction instructions[] = {
      {"NOP", 0xEA, &Mos6502::implied, nullptr},  //
      {"ADC", 0x69, &Mos6502::immediate, &Mos6502::adc},
      // Add more instructions as needed
  };

  Byte fetch() noexcept
  {
    Byte data = read_memory(m_pc);
    m_pc = Address{static_cast<uint16_t>(m_pc) + 1};
    return data;
  }

  constexpr void decodeNextInstruction()
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

  Address m_pc;
  Byte m_aRegister;
  Byte m_xRegister;
  Byte m_yRegister;
  Byte m_sp;
  Byte m_status;

  //! Number of cycles that the CPU has executed
  uint16_t m_cycles = 0;

  struct CurrentState
  {
    const Instruction* instruction = nullptr;
    Action action = nullptr;
    size_t cycle = 0;
  };

  CurrentState m_current;

  // Scratchpad for addressing calculations
  Address m_address = Address{0};
  Byte m_operand = 0;

  // Memory is just a placeholder — inject later
  std::array<Byte, 65536> m_memory{};
};
