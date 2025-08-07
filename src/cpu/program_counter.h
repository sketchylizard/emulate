#pragma once

#include <cstddef>
#include <cstdint>

class ProgramCounter
{
  using Word = std::uint16_t;

  Word m_value;

public:
  constexpr ProgramCounter()
    : m_value(0)
  {
  }

  constexpr Word read() const noexcept
  {
    return m_value;
  }
  constexpr void write(Word addr) noexcept
  {
    m_value = addr;
  }

  // Prefix increment
  constexpr ProgramCounter& operator++() noexcept
  {
    m_value = ++m_value;
    return *this;
  }

  // Increment by a value
  constexpr ProgramCounter& operator+=(Word increment) noexcept
  {
    m_value = (m_value + increment) & 0xFFFF; // Wrap around at 0xFFFF
    return *this;
  }

  constexpr std::byte lo() const noexcept
  {
    return std::byte{static_cast<std::uint8_t>(m_value & 0xFF)};
  }
  constexpr std::byte hi() const noexcept
  {
    return std::byte{static_cast<std::uint8_t>(m_value >> 8)};
  }

  constexpr void set_lo(std::byte lo) noexcept
  {
    m_value = (m_value & 0xFF00) | static_cast<Word>(lo);
  }

  constexpr void set_hi(std::byte hi) noexcept
  {
    m_value = static_cast<Word>((m_value & 0x00FF) | (static_cast<Word>(hi) << 8));
  }
};
