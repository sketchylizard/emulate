#pragma once

#include <array>
#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "common/address.h"
#include "common/bus.h"

namespace Common
{

//! RAM is just a writable span of bytes
using RamSpan = std::span<Byte>;

//! ROM is a read-only span of bytes
using RomSpan = std::span<const Byte>;

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span. You cannot load into a RomSpan, but you can write into a block of
// bytes and then create a RomSpan over it, presenting a read-only view of the data.
void Load(RamSpan memory, const std::string& filename, Address start_addr = Address{0});


namespace bus_concepts
{

// Concept for anything that can be read from using operator[]
template<typename T>
concept Readable = std::ranges::random_access_range<T> && requires(const T& t, size_t addr) {
  { t[addr] } -> std::convertible_to<Common::Byte>;
};

// Concept for anything that can be written to using operator[]
template<typename T>
concept Writable = requires(T& t, Common::Address addr, Common::Byte value) {
  { t[addr] } -> std::convertible_to<Common::Byte&>;
  { t[addr] = value } -> std::convertible_to<Common::Byte&>;
};

// Combined concept - something that's at least readable
template<typename T>
concept MemoryLike = Readable<T>;

template<typename T>
concept ByteSpan = std::same_as<T, std::span<Byte>> || std::same_as<T, std::span<const Byte>>;

}  // namespace bus_concepts

template<bus_concepts::MemoryLike T>
class MemoryDevice
{
public:
  // Conditional storage type: value for spans, reference for others
  using storage_type = std::conditional_t<bus_concepts::ByteSpan<T>, T, T&>;

  // Constructor for span types (store by value)
  constexpr explicit MemoryDevice(T span, Address baseAddress = Address{0})
    requires bus_concepts::ByteSpan<T>
    : m_storage(span)
    , m_base_address(static_cast<size_t>(baseAddress))
  {
  }

  // Constructor for non-span types (store by reference)
  constexpr explicit MemoryDevice(T& ref, Address baseAddress = Address{0})
    requires(!bus_concepts::ByteSpan<T>)
    : m_storage(ref)
    , m_base_address(static_cast<size_t>(baseAddress))
  {
  }

  constexpr Address baseAddress() const noexcept
  {
    return Address{static_cast<uint16_t>(m_base_address)};
  }
  constexpr size_t size() const noexcept
  {
    return m_storage.size();
  }

  Common::BusResponse tick(Common::BusRequest req) noexcept
  {
    // Calculate offset from base address
    size_t offset = static_cast<size_t>(req.address) - m_base_address;

    if (req.isRead())
    {
      Common::Byte data = m_storage[offset];
      return {data, true};
    }
    else if (req.isWrite())
    {
      // Only handle writes if the storage is writable
      if constexpr (bus_concepts::Writable<T>)
      {
        m_storage[offset] = req.data;
        return {req.data, true};
      }
      else
      {
        // ROM - silently ignore writes
        return {0x00, true};
      }
    }

    return {0x00, true};
  }

private:
  storage_type m_storage;
  size_t m_base_address;
};

}  // namespace Common
