#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "common/Bus.h"
#include "common/address.h"
#include "core65xx/core65xx.h"

struct mos6502
{
  static std::span<const Core65xx::Instruction, 256> GetInstructions() noexcept;
};
