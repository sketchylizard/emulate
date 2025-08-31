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

  static Response fetchNextOpcode(State& state, BusResponse) noexcept;

  static std::pair<Microcode*, Microcode*> decodeOpcode(uint8_t opcode) noexcept;

  static void disassemble(const State& state, Common::Address correctedPc, std::span<const Common::Byte, 3> bytes,
      std::span<char, 80> buffer) noexcept;
};

}  // namespace cpu6502
