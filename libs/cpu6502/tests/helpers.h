#pragma once

#include <span>

#include "common/Bus.h"
#include "common/address.h"
#include "cpu6502/state.h"

// Testing helper. A Cycle represents one clock cycle of the CPU, including
// the input data (from memory or external device) and the expected bus request
// output.
struct Cycle
{
  Common::Byte input;  // Input data for this cycle
  Common::BusRequest expected;  // Expected bus request output
};

// Execute a sequence of microcode functions against a series of cycles, verifying
// that the bus requests match the expected values. The State object is modified
// as the microcode functions are executed. Returns true if all cycles matched,
// false if there was a mismatch or if the microcode ran out before the cycles did.
bool execute(cpu6502::State& state, std::span<const cpu6502::State::Microcode> microcode, std::span<const Cycle> cycles);

// Helper function for readable output
std::string formatBusRequest(const Common::BusRequest& request);
