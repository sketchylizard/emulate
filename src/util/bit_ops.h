#pragma once
#include <cstdint>

namespace emu6502 {

constexpr bool bit(uint8_t val, int n) {
  return (val >> n) & 1;
}

} // namespace emu6502
