#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "Mos6502/Bus.h"

static constexpr Address c_maxAddress{0xFFFF};  // Maximum address for 16-bit address space

// Memory class template for 6502 emulator
// T can be Byte or const Byte, allowing for read-only or read-write memory
// Start and End are the address range for the memory (inclusive)
template<typename T, Address Start = Address{0x0000}, Address End = c_maxAddress>
  requires((std::is_same_v<T, Byte> || std::is_same_v<T, const Byte>) && Start < End)
class Memory
{
public:
  explicit Memory(std::span<const Byte> bytes = {})
    : m_bytes{}
  {
    if (!bytes.empty())
    {
      std::copy(bytes.begin(), bytes.end(), m_bytes.begin());
    }
  }

  Bus Tick(Bus bus) noexcept
  {
    if (bus.address < Start || bus.address > End)
    {
      return bus;  // Out of bounds, return unchanged bus
    }

    if ((bus.control & Control::Read) != Control::None)
    {
      // Read operation
      bus.data = m_bytes[static_cast<uint16_t>(bus.address)];
      return bus;
    }

    if constexpr (std::is_same_v<T, Byte>)
    {
      // If T is Byte, we can write to memory, if Read is not set, then we assume it's a write operation.

      m_bytes[static_cast<uint16_t>(bus.address)] = bus.data;
    }

    return bus;
  }

  void Load(Address address, std::span<const Byte> bytes) noexcept
  {
    // Make sure the address is within bounds
    auto effectiveAddress = static_cast<size_t>(address);
    assert(effectiveAddress >= static_cast<size_t>(Start) && effectiveAddress + bytes.size() <= static_cast<size_t>(End));
    // Copy bytes to memory
    assert(bytes.size() <= (static_cast<size_t>(End) - static_cast<size_t>(Start) + 1));
    std::copy(bytes.begin(), bytes.end(), m_bytes.begin() + static_cast<size_t>(address) - static_cast<size_t>(Start));
  }

  [[nodiscard]] Byte& operator[](Address address) noexcept
    requires(std::is_same_v<T, Byte>)
  {
    assert(static_cast<size_t>(address) < std::size(m_bytes));
    return m_bytes[static_cast<size_t>(address)];
  }

  [[nodiscard]] Byte operator[](Address address) const noexcept
  {
    assert(static_cast<size_t>(address) < std::size(m_bytes));
    return m_bytes[static_cast<size_t>(address)];
  }

private:
  size_t m_start{};
  size_t m_end{};
  std::array<T, static_cast<size_t>(End) - static_cast<size_t>(Start) + 1> m_bytes;
};

// Load file helper function.
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;
