#pragma once

#include <catch2/catch_tostring.hpp>

#include "common/address.h"
#include "common/bus.h"

namespace Catch
{
// Specialize StringMaker for Common::Address
template<>
struct StringMaker<Common::Address>
{
  static std::string convert(const Common::Address& addr)
  {
    return std::format("${:04X}", static_cast<uint16_t>(addr));
  }
};

// Specialize StringMaker for Common::Address
template<>
struct StringMaker<Common::BusRequest>
{
  static std::string convert(const Common::BusRequest& value)
  {
    if (value.isSync())
    {
      return std::format("Bus read(${:04x})", value.address);
    }
    else if (value.isRead())
    {
      return std::format("Bus read(${:04x})", value.address);
    }
    else if (value.isWrite())
    {
      return std::format("Bus write(${:04x}, ${:02x})", value.address, value.data);
    }
    return "NONE";
  }
};

}  // namespace Catch
