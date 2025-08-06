#pragma once

#include <cstdint>
#include <bit>

namespace emu6502 {

enum class ALUOp { AND, ORA, EOR };

constexpr uint8_t alu(ALUOp op, uint8_t a, uint8_t b) {
  switch (op) {
    case ALUOp::AND: return a & b;
    case ALUOp::ORA: return a | b;
    case ALUOp::EOR: return a ^ b;
  }
  return 0;
}

} // namespace emu6502
