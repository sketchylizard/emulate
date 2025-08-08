#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

class Cpu6502
{
public:
  using Byte = uint8_t;
  using Word = uint16_t;

private:
  Word m_pc;
  Byte m_aRegister;
  Byte m_xRegister;
  Byte m_yRegister;
  Byte m_sp;
  Byte m_status;

  // Memory is just a placeholder — inject later
  std::array<Byte, 65536> m_memory{};

public:
  constexpr Cpu6502() = default;

  [[nodiscard]] constexpr uint8_t a() const noexcept
  {
    return m_aRegister;
  }
  constexpr void set_a(uint8_t v) noexcept
  {
    m_aRegister = v;
  }

  [[nodiscard]] constexpr uint8_t x() const noexcept
  {
    return m_xRegister;
  }
  constexpr void set_x(uint8_t v) noexcept
  {
    m_xRegister = v;
  }

  [[nodiscard]] constexpr uint8_t y() const noexcept
  {
    return m_yRegister;
  }
  constexpr void set_y(uint8_t v) noexcept
  {
    m_yRegister = v;
  }

  [[nodiscard]] constexpr uint8_t sp() const noexcept
  {
    return m_sp;
  }
  constexpr void set_sp(uint8_t v) noexcept
  {
    m_sp = v;
  }

  [[nodiscard]] constexpr uint16_t pc() const noexcept
  {
    return m_pc;
  }
  constexpr void set_pc(uint16_t addr) noexcept
  {
    m_pc = addr;
  }

  [[nodiscard]] constexpr uint8_t status() const noexcept
  {
    return m_status;
  }

  constexpr void set_status(uint8_t flags) noexcept
  {
    m_status = flags;
  }

  [[nodiscard]] constexpr Byte read_memory(uint16_t address) const noexcept
  {
    return m_memory[address];
  }

  constexpr void write_memory(uint16_t address, Byte value) noexcept
  {
    m_memory[address] = value;
  }

  constexpr void write_memory(uint16_t address, std::span<const uint8_t> bytes) noexcept
  {
    assert(m_memory.size() - bytes.size() > address);

    auto offset = m_memory.begin() + address;
    std::ranges::copy(bytes, offset);
  }

  constexpr void step()
  {
    // Fetch
    Byte opcode = read_memory(m_pc);

    // Increment PC
    ++m_pc;

    // (Very simple execute example — no decode)
    if (opcode == Byte{0xEA})
    {  // NOP
       // Do nothing
    }
    else if (opcode == Byte{0x69})  // ADC #imm
    {
    }
  }

  constexpr void reset(uint16_t reset_vector)
  {
    m_pc = reset_vector;
    m_aRegister = 0;
    m_xRegister = 0;
    m_yRegister = 0;
    m_sp = 0xfd;
  }
};
