#pragma once

#include <catch2/catch_tostring.hpp>

#include "common/address.h"

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

}  // namespace Catch
