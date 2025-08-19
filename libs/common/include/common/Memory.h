#pragma once
#include <cassert>
#include <cstdint>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>

namespace Common
{
using Byte = uint8_t;

enum class Address : uint16_t
{
};

using RamSpan = std::span<Byte>;
using RomSpan = std::span<const Byte>;

// Address arithmetic operators
constexpr Address& operator++(Address& addr)
{  // prefix
  addr = Address(static_cast<uint16_t>(addr) + 1);
  return addr;
}

constexpr Address operator++(Address& addr, int)
{  // postfix
  Address old = addr;
  ++addr;
  return old;
}

constexpr Address& operator--(Address& addr)
{  // prefix
  addr = Address(static_cast<uint16_t>(addr) - 1);
  return addr;
}

constexpr Address operator--(Address& addr, int)
{  // postfix
  Address old = addr;
  --addr;
  return old;
}

constexpr Address& operator+=(Address& addr, int16_t offset)
{
  addr = Address(static_cast<uint16_t>(addr) + offset);
  return addr;
}

constexpr Address& operator-=(Address& addr, int16_t offset)
{
  addr = Address(static_cast<uint16_t>(addr) - offset);
  return addr;
}

constexpr Address operator+(Address addr, uint16_t offset)
{
  return Address(static_cast<uint16_t>(addr) + offset);
}

constexpr Address operator-(Address addr, uint16_t offset)
{
  return Address(static_cast<uint16_t>(addr) - offset);
}

constexpr uint16_t operator-(Address lhs, Address rhs)
{
  return static_cast<uint16_t>(lhs) - static_cast<uint16_t>(rhs);
}

// Byte extraction functions
constexpr Byte HiByte(uint16_t value)
{
  return static_cast<Byte>(value >> 8);
}

constexpr Byte LoByte(uint16_t value)
{
  return static_cast<Byte>(value & 0xFF);
}

constexpr Byte HiByte(Address addr)
{
  return HiByte(static_cast<uint16_t>(addr));
}

constexpr Byte LoByte(Address addr)
{
  return LoByte(static_cast<uint16_t>(addr));
}

// Address construction
constexpr Address MakeAddress(Byte low, Byte high)
{
  return Address((static_cast<uint16_t>(high) << 8) | low);
}

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span
void Load(RamSpan memory, const std::string& filename, Address start_addr = Address{0});

constexpr Address operator""_addr(unsigned long long value) noexcept
{
  assert(value <= 0xFFFF);
  return Address{static_cast<uint16_t>(value)};
}

}  // namespace Common

// Specialize formatter for Address and Control
template<>
struct std::formatter<Common::Address> : std::formatter<uint16_t>
{
  auto format(const Common::Address& addr, auto& ctx) const
  {
    return std::formatter<uint16_t>::format(static_cast<uint16_t>(addr), ctx);
  }
};
