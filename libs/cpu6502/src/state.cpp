#include "cpu6502/state.h"

#include "common/fixed_formatter.h"

namespace cpu6502
{

Common::FixedFormatter& flagsToStr(Common::FixedFormatter& formatter, Common::Byte value) noexcept
{
  using Flag = VisibleState::Flag;

  auto uvalue = static_cast<uint8_t>(value);

  formatter << (uvalue & static_cast<uint8_t>(Flag::Negative) ? 'N' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Overflow) ? 'O' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Unused) ? 'U' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Break) ? 'B' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Decimal) ? 'D' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Interrupt) ? 'I' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Zero) ? 'Z' : '-');
  formatter << (uvalue & static_cast<uint8_t>(Flag::Carry) ? 'C' : '-');

  return formatter;
}

}  // namespace cpu6502
