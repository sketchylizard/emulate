#pragma once

#include <cstdint>

#include "common/address.h"
#include "common/microcode.h"
#include "cpu6502/registers.h"

namespace cpu6502
{

struct Generic6502Definition : public Common::ProcessorDefinition<Common::Address, Common::Byte, Generic6502Definition>
{
  using Flag = Registers::Flag;

  Generic6502Definition() = default;
  explicit Generic6502Definition(const Registers& state)
    : registers(state)
  {
  }

  Registers registers;

  // Flag helpers
  [[nodiscard]] constexpr bool has(Flag f) const noexcept;
  constexpr void set(Flag f, bool v) noexcept;
  constexpr void setZN(Common::Byte v) noexcept;
  constexpr void assignP(Common::Byte v) noexcept;

  // Storage for low byte of address during addressing modes
  Common::Byte lo = 0;
  // Storage for high byte of address during addressing modes
  Common::Byte hi = 0;
  // Storage for operand during instruction execution
  Common::Byte operand = 0;

  struct DisassemblyFormat
  {
    const char prefix[3] = "";  // e.g. "#$" or "($" -- 2 characters + null terminator
    const char suffix[4] = "";  // e.g. ",X)" or ",Y" -- 3 characters + null terminator
    Common::Byte numberOfOperands = 0;
  };

  struct Instruction
  {
    static constexpr size_t c_maxOperations = 7;

    Common::Byte opcode = 0;
    char mnemonic[4] = "???";
    DisassemblyFormat format;
    Microcode op = {};  // first microcode operation
  };
};

constexpr bool Generic6502Definition::has(Flag f) const noexcept
{
  return (registers.p & static_cast<uint8_t>(f)) != 0;
}

constexpr void Generic6502Definition::set(Flag f, bool v) noexcept
{
  registers.p = v ? (registers.p | static_cast<uint8_t>(f)) : (registers.p & ~static_cast<uint8_t>(f));
}

constexpr void Generic6502Definition::setZN(Common::Byte v) noexcept
{
  set(Flag::Zero, v == 0);
  set(Flag::Negative, (v & 0x80) != 0);
}

// If you ever assign p wholesale, re-assert U:
constexpr void Generic6502Definition::assignP(Common::Byte v) noexcept
{
  registers.p = Common::Byte((v | static_cast<uint8_t>(Flag::Unused)));
}

}  // namespace cpu6502
