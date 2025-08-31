#include "helpers.h"

#include <catch2/catch_all.hpp>
#include <iomanip>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/bus.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

using State = cpu6502::State;
using Microcode = cpu6502::Microcode;
using BusRequest = Common::BusRequest;
using BusResponse = Common::BusResponse;
using MicrocodeResponse = cpu6502::MicrocodeResponse;

// In common/test_helpers.h or wherever you put the test utilities
// Helper function for readable output
std::ostream& operator<<(std::ostream& os, const BusRequest& value)
{
  if (value.isSync())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isRead())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isWrite())
  {
    os << std::format("Bus write(${:04x}, ${:02x})", value.address, value.data);
  }
  os << "NONE";
  return os;
}

bool execute(State& state, std::span<const Microcode> microcode, std::span<const Common::Cycle> cycles)
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
