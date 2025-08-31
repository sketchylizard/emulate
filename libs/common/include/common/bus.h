#pragma once

#include <cassert>
#include <cstdint>
#include <format>
#include <utility>

#include "common/address.h"

namespace Common
{

enum class Control : uint8_t
{
  None = 0,
  Interrupt = 0b1000'0001,
  NonMaskableInterrupt = 0b1000'0010,
  Ready = 0b1000'0100,
  Reset = 0b1000'1000,
  Read = 0b1001'0000,
  Sync = 0b1010'0000,
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

struct BusRequest
{
  Address address = Address{0};
  Byte data = 0;
  Control control = Control::None;

  constexpr explicit operator bool() const noexcept
  {
    return (static_cast<uint8_t>(control) & 0b1000'0000) != 0;
  }

  constexpr bool isRead() const noexcept
  {
    return hasControl(Control::Read);
  }

  constexpr bool isWrite() const noexcept
  {
    return !hasControl(Control::Read);
  }

  constexpr bool isSync() const noexcept
  {
    return hasControl(Control::Sync);
  }

  constexpr bool hasControl(Control flag) const noexcept
  {
    return (control & flag) == flag;
  }

  static constexpr BusRequest Read(Address addr, Control additionalFlags = Control::None) noexcept
  {
    return BusRequest{addr, 0, Control::Read | additionalFlags};
  }

  static constexpr BusRequest Write(Address addr, Byte data, Control additionalFlags = Control::None) noexcept
  {
    return BusRequest{addr, data, additionalFlags & ~Control::Read};
  }

  static constexpr BusRequest Fetch(Address addr) noexcept
  {
    return BusRequest{addr, 0, Control::Read | Control::Sync};
  }

  friend bool operator==(const BusRequest& lhs, const BusRequest& rhs) noexcept
  {
    return lhs.address == rhs.address && lhs.data == rhs.data && lhs.control == rhs.control;
  }
};

struct BusResponse
{
  Byte data;
  bool ready = true;  // for devices that need wait states
};

constexpr bool IsSamePage(Address lhs, Address rhs) noexcept
{
  return (static_cast<uint16_t>(lhs) & 0xFF00) == (static_cast<uint16_t>(rhs) & 0xFF00);
}

// Testing helper. A Cycle represents one clock cycle of the CPU, including the input data (from
// memory or external device) and the expected bus request output.
struct Cycle
{
  Common::Byte input;  // Input data for this cycle
  Common::BusRequest expected;  // Expected bus request output
};

}  // namespace Common

template<>
struct std::formatter<Common::Control> : std::formatter<uint8_t>
{
  auto format(const Common::Control& ctrl, auto& ctx) const
  {
    return std::formatter<uint8_t>::format(static_cast<uint8_t>(ctrl), ctx);
  }
};
