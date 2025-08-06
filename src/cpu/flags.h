#pragma once
#include <cstdint>

namespace emu6502 {

enum StatusFlags : uint8_t {
  Carry = 0x01,
  Zero = 0x02,
  InterruptDisable = 0x04,
  Decimal = 0x08,
  Break = 0x10,
  Unused = 0x20,
  Overflow = 0x40,
  Negative = 0x80
};

struct Flags {
  uint8_t P = Unused;

  constexpr void set(StatusFlags flag) { P |= flag; }
  constexpr void clear(StatusFlags flag) { P &= ~flag; }
  constexpr bool is_set(StatusFlags flag) const { return P & flag; }

  constexpr void update_nz(uint8_t val) {
    if (val == 0) set(Zero); else clear(Zero);
    if (val & 0x80) set(Negative); else clear(Negative);
  }
};

} // namespace emu6502
