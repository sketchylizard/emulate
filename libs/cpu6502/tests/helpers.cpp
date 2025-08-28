#include "helpers.h"

#include <catch2/catch_all.hpp>
#include <iomanip>

#include "common/Bus.h"
#include "common/address.h"
#include "common/address_string_maker.h"
#include "cpu6502/state.h"

using State = cpu6502::State;

namespace Common
{

std::ostream& operator<<(std::ostream& os, Common::Address addr)
{
  os << std::hex << std::setfill('0') << std::setw(4) << static_cast<int>(addr);
  return os;
}
}  // namespace Common

bool execute(State& state, std::span<const State::Microcode> microcode, std::span<const Cycle> cycles)
{
  state.next = nullptr;
  size_t microcodeIndex = 0;

  for (size_t cycleIndex = 0; cycleIndex < cycles.size(); ++cycleIndex)
  {
    const auto& cycle = cycles[cycleIndex];

    // Determine which microcode function to call
    State::Microcode currentMicrocode = nullptr;

    if (state.next != nullptr)
    {
      // Use the scheduled next action
      currentMicrocode = state.next;
      state.next = nullptr;  // Reset for next iteration
    }
    else
    {
      // Get next microcode from span
      if (microcodeIndex >= microcode.size())
      {
        UNSCOPED_INFO("Ran out of microcode at cycle " << cycleIndex);
        return false;
      }

      currentMicrocode = microcode[microcodeIndex];
      if (currentMicrocode == nullptr)
      {
        UNSCOPED_INFO("Hit nullptr microcode at index " << microcodeIndex << ", cycle " << cycleIndex);
        return false;
      }

      ++microcodeIndex;
    }

    // Execute the microcode
    Common::BusResponse response{cycle.input};
    Common::BusRequest actual = currentMicrocode(state, response);

    // Compare with expected result
    if (actual != cycle.expected)
    {
      UNSCOPED_INFO("Cycle " << cycleIndex << " mismatch:");
      UNSCOPED_INFO("  Expected: " << formatBusRequest(cycle.expected));
      UNSCOPED_INFO("  Actual:   " << formatBusRequest(actual));
      UNSCOPED_INFO("  Input:    $" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(cycle.input));
      UNSCOPED_INFO("  State PC: $" << std::setw(4) << state.pc);
      return false;
    }
  }

  // Verify we've consumed all microcode (unless there are nullptr entries)
  while (microcodeIndex < microcode.size() && microcode[microcodeIndex] == nullptr)
  {
    ++microcodeIndex;
  }

  if (microcodeIndex < microcode.size() && state.next == nullptr)
  {
    UNSCOPED_INFO("Unused microcode remaining after all cycles completed");
    return false;
  }

  return true;
}

// Helper function for readable output
std::string formatBusRequest(const Common::BusRequest& request)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');

  if (request.isRead())
  {
    oss << "READ($" << std::setw(4) << request.address << ")";
    if (request.isSync())
      oss << " SYNC";
  }
  else if (request.isWrite())
  {
    oss << "WRITE($" << std::setw(4) << request.address << ", $" << std::setw(2) << static_cast<int>(request.data) << ")";
  }
  else
  {
    oss << "NONE";
  }

  return oss.str();
}
