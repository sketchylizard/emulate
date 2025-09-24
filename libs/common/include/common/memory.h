#pragma once

#include <array>
#include <concepts>
#include <fstream>
#include <iostream>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "common/address.h"
#include "common/bus.h"
#include "common/logger.h"

namespace Common
{

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span. You cannot load into a RomSpan, but you can write into a block of
// bytes and then create a RomSpan over it, presenting a read-only view of the data.
void Load(std::span<Byte> memory, const std::string& filename, Address start_addr = Address{0});

//! A Bus device that uses a contiguous range (array, vector, span, etc) as
//! backing storage. The range can be of Byte or const Byte, allowing for
//! RAM-like (read/write) or ROM-like (read-only) behavior.
template<ByteOrConstByte T>
class MemoryDevice
{
public:
  using ValueType = T;
  using ReferenceType = T&;

  explicit constexpr MemoryDevice(std::span<T> range, Address baseAddress = Address{0}) noexcept
    : m_memory(range)
    , m_baseAddress(static_cast<size_t>(baseAddress))
  {
  }

  // Constexpr size for compile-time bus mapping
  constexpr size_t size() const noexcept
  {
    return m_memory.size();
  }

  constexpr Address startAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_baseAddress)};
  }

  constexpr Address endAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_baseAddress + m_memory.size() - 1)};
  }

  // Read method for Bus interface
  ValueType read(Address address) const noexcept
  {
    size_t index = static_cast<size_t>(address) - m_baseAddress;
    if (index < m_memory.size())
    {
      if (address >= Address{0x300} && address < Address{0x3ff})
      {
        std::stringstream stream;
        stream << "read " << std::hex << static_cast<uint16_t>(address) << " = " << std::hex
               << static_cast<uint16_t>(m_memory[index]) << std::dec << "\n";
        LOG(stream.str());
      }
      return m_memory[index];
    }
    return 0;  // Out of range reads return 0
  }

  // Read method for Bus interface
  void write(Address address, ValueType value) const noexcept
  {
    if constexpr (!std::is_const_v<ValueType>)
    {
      if (address >= Address{0x300} && address < Address{0x3ff})
      {
        std::stringstream stream;
        stream << "write " << std::hex << static_cast<uint16_t>(address) << " = " << std::hex
               << static_cast<uint16_t>(value) << std::dec << "\n";
        LOG(stream.str());
      }
      size_t index = static_cast<size_t>(address) - m_baseAddress;
      if (index < m_memory.size())
      {
        m_memory[index] = value;
      }
    }
  }

  bool contains(Address address) const noexcept
  {
    size_t index = static_cast<size_t>(address) - m_baseAddress;
    return index < m_memory.size();
  }

private:
  std::span<ValueType> m_memory;
  size_t m_baseAddress;
};

// For std::array
template<typename T, std::size_t N>
MemoryDevice(std::array<T, N>&, Address baseAddress = Address{0}) -> MemoryDevice<T>;

template<typename T, std::size_t N>
MemoryDevice(const std::array<T, N>&, Address baseAddress = Address{0}) -> MemoryDevice<const T>;

// For std::vector
template<typename T>
MemoryDevice(std::vector<T>&, Address baseAddress = Address{0}) -> MemoryDevice<T>;

template<typename T>
MemoryDevice(const std::vector<T>&, Address baseAddress = Address{0}) -> MemoryDevice<const T>;

// For C-style array
template<typename T, std::size_t N>
MemoryDevice(T (&)[N], Address baseAddress = Address{0}) -> MemoryDevice<T>;

}  // namespace Common
