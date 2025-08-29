#include "common/address.h"

#include <format>

namespace Common
{

std::ostream& operator<<(std::ostream& os, Common::Address addr)
{
  os << std::format("{:4X}", static_cast<int>(addr));
  return os;
}

}  // namespace Common
