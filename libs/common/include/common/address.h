#pragma once

#include <cassert>
#include <cstdint>
#include <format>

namespace Common
{
using Byte = uint8_t;

//! 16-bit address type
enum class Address : uint16_t
{
};

// Define a concept for a type that must be a Byte or Const Byte
template<typename T>
concept ByteOrConstByte = std::same_as<T, Byte> || std::same_as<T, const Byte>;

// Address arithmetic operators
constexpr Address& operator++(Address& addr) noexcept
{  // prefix
  addr = Address(static_cast<uint16_t>(addr) + 1);
  return addr;
}

constexpr Address operator++(Address& addr, int) noexcept
{  // postfix
  Address old = addr;
  ++addr;
  return old;
}

constexpr Address& operator--(Address& addr) noexcept
{  // prefix
  addr = Address(static_cast<uint16_t>(addr) - 1);
  return addr;
}

constexpr Address operator--(Address& addr, int) noexcept
{  // postfix
  Address old = addr;
  --addr;
  return old;
}

constexpr Address& operator+=(Address& addr, int16_t offset) noexcept
{
  addr = Address(static_cast<uint16_t>(addr) + offset);
  return addr;
}

constexpr Address& operator-=(Address& addr, int16_t offset) noexcept
{
  addr = Address(static_cast<uint16_t>(addr) - offset);
  return addr;
}

constexpr Address operator+(Address addr, uint16_t offset) noexcept
{
  return Address(static_cast<uint16_t>(addr) + offset);
}

constexpr Address operator-(Address addr, uint16_t offset) noexcept
{
  return Address(static_cast<uint16_t>(addr) - offset);
}

constexpr uint16_t operator-(Address lhs, Address rhs) noexcept
{
  return static_cast<uint16_t>(lhs) - static_cast<uint16_t>(rhs);
}

// Byte extraction functions
constexpr Byte HiByte(uint16_t value) noexcept
{
  return static_cast<Byte>(value >> 8);
}

constexpr Byte LoByte(uint16_t value) noexcept
{
  return static_cast<Byte>(value & 0xFF);
}

constexpr Byte HiByte(Address addr) noexcept
{
  return HiByte(static_cast<uint16_t>(addr));
}

constexpr Byte LoByte(Address addr) noexcept
{
  return LoByte(static_cast<uint16_t>(addr));
}

// Address construction
constexpr Address MakeAddress(Byte low, Byte high) noexcept
{
  return Address((static_cast<uint16_t>(high) << 8) | low);
}

constexpr Address operator""_addr(unsigned long long value) noexcept
{
  assert(value <= 0xFFFF);
  return Address{static_cast<uint16_t>(value)};
}

std::ostream& operator<<(std::ostream& os, Common::Address addr);

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
