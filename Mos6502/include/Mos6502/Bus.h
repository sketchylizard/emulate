#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <utility>

using Byte = uint8_t;

enum class Address : uint16_t
{
};

enum class Control : uint8_t
{
  None = 0,
  Interrupt = 1 << 0,
  NonMaskableInterrupt = 1 << 1,
  Ready = 1 << 2,
  Reset = 1 << 3,
  Read = 1 << 4,
  Sync = 1 << 5,
};

struct Bus
{
  Address address;
  Byte data;
  Control control;
};

// Specialize formatter for Address and Control
template<>
struct std::formatter<Address> : std::formatter<uint16_t>
{
  auto format(const Address& addr, auto& ctx) const
  {
    return std::formatter<uint16_t>::format(static_cast<uint16_t>(addr), ctx);
  }
};

template<>
struct std::formatter<Control> : std::formatter<uint8_t>
{
  auto format(const Control& ctrl, auto& ctx) const
  {
    return std::formatter<uint8_t>::format(static_cast<uint8_t>(ctrl), ctx);
  }
};

constexpr Control& operator|=(Control& lhs, Control rhs) noexcept
{
  return lhs = static_cast<Control>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr Control& operator&=(Control& lhs, Control rhs) noexcept
{
  return lhs = static_cast<Control>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr Control operator|(Control lhs, Control rhs) noexcept
{
  return static_cast<Control>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr Control operator&(Control lhs, Control rhs) noexcept
{
  return static_cast<Control>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr Control operator~(Control value) noexcept
{
  return static_cast<Control>(~static_cast<uint8_t>(value));
}

constexpr bool operator!(Control value) noexcept
{
  return static_cast<uint8_t>(value) == 0;
}

constexpr Address MakeAddress(Byte lo, Byte hi) noexcept
{
  return Address{static_cast<uint16_t>(static_cast<uint16_t>(hi) << 8 | static_cast<uint16_t>(lo))};
}

constexpr Address& operator+=(Address& lhs, int8_t rhs) noexcept
{
  auto tmp = static_cast<int32_t>(lhs) + rhs;
  assert(tmp >= 0 && tmp <= 0xFFFF);
  lhs = Address{static_cast<uint16_t>(tmp)};
  return lhs;
}

constexpr Address operator+(Address lhs, int8_t rhs) noexcept
{
  auto tmp = static_cast<int32_t>(lhs) + rhs;
  assert(tmp >= 0 && tmp <= 0xFFFF);
  return Address{static_cast<uint16_t>(tmp)};
}

constexpr Address& operator-=(Address& lhs, int8_t rhs) noexcept
{
  auto tmp = static_cast<int32_t>(lhs) - rhs;
  assert(tmp >= 0 && tmp <= 0xFFFF);
  lhs = Address{static_cast<uint16_t>(tmp)};
  return lhs;
}

constexpr Address operator-(Address lhs, int8_t rhs) noexcept
{
  auto tmp = static_cast<int32_t>(lhs) - rhs;
  assert(tmp >= 0 && tmp <= 0xFFFF);
  return Address{static_cast<uint16_t>(tmp)};
}

constexpr Address& operator++(Address& lhs) noexcept
{
  auto tmp = static_cast<uint16_t>(lhs);
  lhs = Address{++tmp};
  return lhs;
}

constexpr Address operator++(Address& lhs, int) noexcept
{
  auto tmp = lhs;
  ++lhs;
  return tmp;
}

constexpr bool IsSamePage(Address lhs, Address rhs) noexcept
{
  return (static_cast<uint16_t>(lhs) & 0xFF00) == (static_cast<uint16_t>(rhs) & 0xFF00);
}

constexpr Byte LoByte(Address address) noexcept
{
  return static_cast<Byte>(static_cast<uint16_t>(address) & 0x00FF);
}

constexpr Byte HiByte(Address address) noexcept
{
  return static_cast<Byte>((static_cast<uint16_t>(address) & 0xFF00) >> 8);
}
