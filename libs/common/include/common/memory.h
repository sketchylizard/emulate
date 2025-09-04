#pragma once

#include <array>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "common/address.h"
#include "common/bus.h"

namespace Common
{

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span. You cannot load into a RomSpan, but you can write into a block of
// bytes and then create a RomSpan over it, presenting a read-only view of the data.
void Load(std::span<Byte> memory, const std::string& filename, Address start_addr = Address{0});

//! A Bus device that uses a contiguous range (array, vector, span, etc) as backing storage. The
//! range can be of Byte or const Byte, allowing for RAM-like (read/write) or ROM-like (read-only)
//! behavior.
template<typename T>
  requires std::same_as<T, Common::Byte> || std::same_as<T, const Common::Byte>
class MemoryDevice
{
public:
  using ValueType = T;
  using ReferenceType = T&;

  // Check if the range allows writes (reference type is non-const)
  static constexpr bool isWritable = !std::is_const_v<std::remove_reference_t<ReferenceType>>;

  template<typename R>
    requires std::ranges::contiguous_range<R> && std::same_as<std::ranges::range_value_t<R>, Byte>
  explicit constexpr MemoryDevice(R&& range, Address baseAddress = Address{0}) noexcept
    : m_memory(std::span{range})
    , m_baseAddress(static_cast<size_t>(baseAddress))
  {
  }

  // For const ranges (ROM-like behavior)
  template<typename R>
    requires std::ranges::contiguous_range<R> && std::same_as<std::ranges::range_value_t<R>, const Byte>
  explicit constexpr MemoryDevice(R&& range, Address baseAddress = Address{0}) noexcept
    : m_memory(reinterpret_cast<const Byte*>(std::ranges::data(range)), std::ranges::size(range))
    , m_baseAddress(static_cast<size_t>(baseAddress))
  {
  }

  constexpr BusResponse tick(BusRequest req) noexcept
  {
    // Calculate offset into the backing array
    size_t busAddress = static_cast<uint16_t>(req.address);
    if (busAddress < m_baseAddress)
    {
      return {0x00, true};  // Address below our range
    }

    size_t offset = busAddress - m_baseAddress;
    if (offset >= m_memory.size())
    {
      return {0x00, true};  // Address above our range
    }

    if (req.isWrite())
    {
      if constexpr (isWritable)
      {
        m_memory[offset] = req.data;
        return {req.data, true};
      }
      else
      {
        // Write to ROM - ignore the write but return the existing data
        return {m_memory[offset], true};
      }
    }
    else
    {
      // Read operation
      return {m_memory[offset], true};
    }
  }

  // Constexpr size for compile-time bus mapping
  constexpr size_t size() const noexcept
  {
    return m_memory.size();
  }

  constexpr Address baseAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_baseAddress)};
  }

  constexpr Address startAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_baseAddress)};
  }

  constexpr Address endAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_baseAddress + m_memory.size() - 1)};
  }

  // Debug access to underlying memory
  constexpr std::span<const Byte> data() const noexcept
  {
    if constexpr (isWritable)
    {
      return std::span<const Byte>{m_memory};
    }
    else
    {
      return m_memory;
    }
  }

  // Writable access only if the underlying range is writable
  constexpr std::span<Byte> data() noexcept
    requires isWritable
  {
    return m_memory;
  }

private:
  std::span<ValueType> m_memory;
  size_t m_baseAddress;
};

// Deduction guides to help with template argument deduction
// template<std::ranges::contiguous_range R>
// MemoryDevice(R&& range, Address baseAddress = Address{0}) -> MemoryDevice<std::remove_cvref_t<R>>;

// For std::array
template<typename T, std::size_t N>
MemoryDevice(std::array<T, N>&, Address baseAddress = Address{0}) -> MemoryDevice<T>;

// For std::vector
template<typename T>
MemoryDevice(std::vector<T>&, Address baseAddress = Address{0}) -> MemoryDevice<T>;

// For C-style array
template<typename T, std::size_t N>
MemoryDevice(T (&)[N], Address baseAddress = Address{0}) -> MemoryDevice<T>;

}  // namespace Common
