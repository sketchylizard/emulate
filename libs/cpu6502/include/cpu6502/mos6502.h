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

struct mos6502 : CpuDefinition
{

  static Response fetchNextOpcode(State& state, BusResponse) noexcept;

  static std::pair<Microcode*, Microcode*> decodeOpcode(uint8_t opcode) noexcept;

  //! Latch any values at the beginning of the cycle.
  //! This is optional; only define it if you need to latch values.
  //! It will be called at the start of each tick(), before executing any microcode.
  static void latch(State& state) noexcept;
};

Common::FixedFormatter& operator<<(
    Common::FixedFormatter& formatter, std::pair<const State&, std::span<Common::Byte, 3>> stateAndBytes) noexcept;

}  // namespace cpu6502
