#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/state.h"

namespace cpu6502
{

struct mos6502
{
  static std::span<const Instruction, 256> GetInstructions() noexcept;
};

}  // namespace cpu6502
