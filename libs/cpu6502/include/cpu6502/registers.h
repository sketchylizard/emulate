#pragma once

#include "common/address.h"
#include "common/bus.h"
#include "common/fixed_formatter.h"

namespace cpu6502
{

struct Registers
{
  enum class Flag : Common::Byte
  {
    Carry = 0x01,
    Zero = 0x02,
    Interrupt = 0x04,
    Decimal = 0x08,
    Break = 0x10,
    Unused = 0x20,  // "U" bit, set when pushed
    Overflow = 0x40,
    Negative = 0x80,
  };

  Common::Address pc{0};
  Common::Byte a{0};
  Common::Byte x{0};
  Common::Byte y{0};
  Common::Byte sp{0};
  Common::Byte p{static_cast<Common::Byte>(Flag::Unused)};  // keep U set

  bool operator<=>(const Registers&) const = default;
};

Common::FixedFormatter& flagsToStr(Common::FixedFormatter& formatter, Common::Byte value) noexcept;

}  // namespace cpu6502
