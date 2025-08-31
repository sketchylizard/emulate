#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

namespace cpu6502
{

struct mos6502 : CpuDefinition
{

  static BusRequest fetchNextOpcode(State& state) noexcept;

  static std::pair<Microcode*, Microcode*> decode(uint8_t opcode) noexcept;
};

}  // namespace cpu6502
