#include "helpers.h"

#include <catch2/catch_all.hpp>
#include <iomanip>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

#if 0

using State = cpu6502::State;
using Microcode = cpu6502::Microcode;
using BusRequest = Common::BusRequest;
using BusResponse = Common::BusResponse;
using MicrocodeResponse = cpu6502::MicrocodeResponse;

bool execute(State& cpu, std::span<const Microcode> microcode, std::span<const Common::Cycle> cycles)
{
  MicrocodeResponse stateResponse;
  auto nextMicrocode = [&, it = microcode.begin()]() mutable -> Microcode
  {
    if (stateResponse.injection)
    {
      auto mc = stateResponse.injection;
      stateResponse.injection = nullptr;  // Clear after use
      return mc;
    }
    if (it != microcode.end())
    {
      return *it++;
    }
    return nullptr;
  };

  size_t cycleCount = 0;
  for (auto cycle : cycles)
  {
    auto codeToExecute = nextMicrocode();
    if (codeToExecute == nullptr)
    {
      UNSCOPED_INFO("Ran out of microcode at cycle " << cycleCount);
      return false;
    }

    stateResponse = codeToExecute(state, {cycle.input});

    if (!stateResponse.request)
    {
      UNSCOPED_INFO("Microcode did not produce a bus request at cycle " << cycleCount);
      return false;
    }
    // Compare with expected result
    if (stateResponse.request != cycle.expected)
    {
      //      UNSCOPED_INFO("Cycle " << cycleCount << " mismatch:");
      //      UNSCOPED_INFO("  Expected: " << cycle.expected);
      //      UNSCOPED_INFO("  Actual:   " << stateResponse.request);
      //      UNSCOPED_INFO("  Input:    $" << std::hex << std::setfill('0') << std::setw(2) <<
      //      static_cast<int>(cycle.input)); UNSCOPED_INFO("  State PC: $" << std::setw(4) << state.pc);
      return false;
    }
    ++cycleCount;
  }

  if (nextMicrocode() != nullptr)
  {
    UNSCOPED_INFO("Microcode remaining after all cycles completed");
    return false;
  }

  return true;
}
#endif
