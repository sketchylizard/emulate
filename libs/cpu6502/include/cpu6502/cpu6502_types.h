#pragma once

#include <cstdint>

#include "common/address.h"
#include "common/bus.h"
#include "common/microcode.h"
#include "cpu6502/state.h"

namespace cpu6502
{
using CpuDefinition = Common::MicrocodeDefinition<State, Common::BusResponse, Common::BusRequest>;

using Microcode = CpuDefinition::Microcode;
using BusRequest = Common::BusRequest;
using BusResponse = Common::BusResponse;
using MicrocodeResponse = CpuDefinition::Response;

struct DisassemblyFormat
{
  char prefix[3] = "";  // e.g. "#$" or "($" -- 2 characters + null terminator
  char suffix[4] = "";  // e.g. ",X)" or ",Y" -- 3 characters + null terminator
  Common::Byte numberOfOperands = 0;
};
static_assert(sizeof(DisassemblyFormat) == 8);

struct Instruction
{
  static constexpr size_t c_maxOperations = 7;

  Common::Byte opcode = 0;
  Common::Byte length = 1;  // total length in bytes (opcode + operands)
  DisassemblyFormat format;
  const char* mnemonic = "???";
  Microcode ops[c_maxOperations] = {};  // sequence of microcode functions to execute
};

//! Exception thrown when the CPU encounters a trap condition, such as a self-jump
//! or self branch.
class TrapException : public std::exception
{
public:
  explicit TrapException(Common::Address trapAddress)
    : m_address(trapAddress)
  {
  }

  Common::Address address() const noexcept
  {
    return m_address;
  }

  const char* what() const noexcept override
  {
    return "CPU trap detected";
  }

private:
  Common::Address m_address;
};

}  // namespace cpu6502
