#include <cstdint>
#include <fstream>
#include <iostream>

#include "common/address.h"
#include "common/bus.h"
#include "common/memory.h"
#include "common/microcode_pump.h"
#include "cpu6502/mos6502.h"
#include "cpu6502/state.h"
#include "simdjson.h"
#include "test_run.h"

using namespace simdjson;  // optional

using namespace Common;
using namespace cpu6502;

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cout << "Usage: " << argv[0] << " <testfile.json>" << std::endl;
    return -1;
  }

  std::cout << "Using test file: " << argv[1] << std::endl;

  cpu6502::mos6502::trapHandler = [](Address /*pc*/) { /* ignore traps */ };

  ondemand::parser parser;
  auto json = padded_string::load(argv[1]);
  // position a pointer at the beginning of the JSON data
  ondemand::document doc = parser.iterate(json);

  std::vector<BusRequest> expectedCycles;
  std::vector<BusRequest> actualCycles;

  [[maybe_unused]] int32_t testCount = 0;
  int32_t failCount = 0;
  for (auto test : doc.get_array())
  {
    std::string_view name = test["name"].get_string().value();

    Snapshot initial = test["initial"].get<Snapshot>();
    Snapshot final = test["final"].get<Snapshot>();

    MicrocodePump<mos6502> pump;
    State cpu_state(initial.regs);
    SparseMemory memory{initial.memory};

    test["cycles"].get(final.cycles);
    actualCycles.clear();
    actualCycles.reserve(final.cycles.size());

    BusResponse response{0x00};  // Start with default response

    for (auto expected : final.cycles)
    {
      static_cast<void>(expected);  // ignore unused in release builds

      // Execute one CPU tick
      BusRequest actual = pump.tick(cpu_state, response);

      // Execute memory operation and prepare response for next cycle
      response = memory.tick(actual);

      actual.data = response.data;  // Capture data for comparison

      if (actual.isSync())
      {
        // Incoming cycles don't mark sync operations, so clear it here for comparison
        actual.control &= ~Control::Sync;
        actual.control |= Control::Write;  // just to set the high bit
      }
      actualCycles.push_back(actual);
    }

    std::ranges::sort(memory.mem, {}, &MemoryLocation::address);
    std::ranges::sort(final.memory, {}, &MemoryLocation::address);

    Snapshot actual;
    actual.regs = cpu_state;
    actual.memory = memory.mem;
    actual.cycles = actualCycles;

    ++testCount;
    if (actual != final)
    {
      std::cout << "Test '" << name << "' failed\n";
      reportError(name, final, actual);
      ++failCount;
    }
  }

  return failCount;
}
