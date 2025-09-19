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

// Define a concept for a type that must be a Byte or Const Byte
template<typename T>
concept ByteOrConstByte = std::same_as<T, Byte> || std::same_as<T, const Byte>;

// Define a concept for a range that is a contiguous range of Byte
template<typename R>
concept WritableByteRange =
    std::ranges::contiguous_range<R> &&
    (std::same_as<std::ranges::range_value_t<R>, Byte> || std::same_as<std::ranges::range_value_t<R>, const Byte>);

// Define a concept for a range that is a contiguous range of const Byte
template<typename R>
concept ConstByteRange = std::ranges::contiguous_range<R> && std::same_as<std::ranges::range_value_t<R>, const Byte>;

//! A Bus device that uses a contiguous range (array, vector, span, etc) as
//! backing storage. The range can be of Byte or const Byte, allowing for
//! RAM-like (read/write) or ROM-like (read-only) behavior.
template<ByteOrConstByte T>
class MemoryDevice
{
public:
  using ValueType = T;
  using ReferenceType = T&;

  // Check if the range allows writes (reference type is non-const)
  static constexpr bool isWritable = !std::is_const_v<std::remove_reference_t<ReferenceType>>;

  template<WritableByteRange R>
  explicit constexpr MemoryDevice(R&& range, Address baseAddress = Address{0}) noexcept
    : m_memory(std::span{range})
    , m_baseAddress(static_cast<size_t>(baseAddress))
  {
  }

  // For const ranges (ROM-like behavior)
  template<ConstByteRange R>
  explicit constexpr MemoryDevice(R&& range, Address baseAddress = Address{0}) noexcept
    : m_memory(reinterpret_cast<const Byte*>(std::ranges::data(range)), std::ranges::size(range))
    , m_baseAddress(static_cast<size_t>(baseAddress))
  {
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
