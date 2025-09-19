#pragma once

#include <cstdint>

#include "common/address.h"
#include "common/microcode.h"
#include "cpu6502/state.h"

namespace cpu6502
{

struct Generic6502Definition : public Common::ProcessorDefinition<Common::Address, Common::Byte, State>
{

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

}  // namespace cpu6502
