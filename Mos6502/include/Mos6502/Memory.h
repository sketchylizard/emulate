#pragma once

#include <cassert>
#include <cstdint>
#include <span>

#include "Mos6502/Bus.h"

class Memory
{
public:
  Memory() = default;

  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;

  Bus Tick(Bus bus) noexcept;

  void Load(Address address, std::span<const Byte> bytes) noexcept;

  [[nodiscard]] Byte& operator[](Address address) noexcept;
  [[nodiscard]] Byte operator[](Address address) const noexcept;

private:
  Byte m_ram[0x10000] = {};  // 64KB of RAM
};

inline Byte& Memory::operator[](Address address) noexcept
{
  assert(static_cast<std::size_t>(address) < std::size(m_ram));
  return m_ram[static_cast<std::size_t>(address)];
}

inline Byte Memory::operator[](Address address) const noexcept
{
  assert(static_cast<std::size_t>(address) < std::size(m_ram));
  return m_ram[static_cast<std::size_t>(address)];
}

