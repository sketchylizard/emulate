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

struct Instruction
{
  Common::Byte opcode = 0;
  const char* mnemonic = "???";
  State::AddressModeType addressMode = State::AddressModeType::Implied;
  Microcode ops[7] = {};  // sequence of microcode functions to execute
};

}  // namespace cpu6502
