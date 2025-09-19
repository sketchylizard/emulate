#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

namespace Common
{
class FixedFormatter;
}

namespace cpu6502
{

struct mos6502 : public Generic6502Definition
{
  static Microcode fetchNextOpcode(State& cpu, BusToken bus) noexcept;
};

// Common::FixedFormatter& operator<<(
//     Common::FixedFormatter& formatter, std::pair<const State&, std::span<Common::Byte, 3>> stateAndBytes) noexcept;

}  // namespace cpu6502
