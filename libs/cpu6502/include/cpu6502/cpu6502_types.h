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
  const char* prefix = "";  // e.g. "#$" or "($"
  const char* suffix = "";  // e.g. ",X)" or ",Y"
  Common::Byte numberOfOperands = 0;
};

struct Instruction
{
  static constexpr size_t c_maxOperations = 7;

  Common::Byte opcode = 0;
  State::AddressModeType addressMode = State::AddressModeType::Implied;
  Common::Byte length = 1;  // total length in bytes (opcode + operands)
  const char* mnemonic = "???";
  const DisassemblyFormat* format = nullptr;
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
