#pragma once

#include <catch2/catch_tostring.hpp>
#include <span>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

// Execute a sequence of microcode functions against a series of cycles, verifying
// that the bus requests match the expected values. The State object is modified
// as the microcode functions are executed. Returns true if all cycles matched,
// false if there was a mismatch or if the microcode ran out before the cycles did.
bool execute(cpu6502::State& state, std::span<const cpu6502::Microcode> microcode, std::span<const Common::Cycle> cycles);
