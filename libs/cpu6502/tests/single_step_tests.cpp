#include <cstdint>
#include <fstream>
#include <iostream>

#include "common/address.h"
#include "common/bus.h"
#include "common/memory.h"
#include "common/microcode_pump.h"
#include "cpu6502/logger.h"
#include "cpu6502/mos6502.h"
#include "cpu6502/registers.h"
#include "simdjson.h"
#include "test_reporter.h"
#include "test_run.h"

using namespace simdjson;  // optional

using namespace Common;
using namespace cpu6502;

struct BusImpl : Generic6502Definition::BusInterface
{
  SparseMemory memory;
  std::vector<Cycle> cycles{};

  BusImpl(std::vector<MemoryLocation>&& mem)
    : memory(std::move(mem))
  {
    cycles.reserve(7);
  }

  Byte read(Address address)
  {
    auto data = memory.read(address);
    cycles.emplace_back(address, data, true);
    return data;
  }

  void write(Address address, Byte data)
  {
    cycles.emplace_back(address, data, false);
    memory.write(address, data);
  }
};

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cout << "Usage: " << argv[0] << " <testfile.json>" << std::endl;
    return -1;
  }

  std::cout << "Using test file: " << argv[1] << std::endl;

  // Logger::setLevel(LogLevel::Minimal);

  // Disable trap handling for tests
  cpu6502::mos6502::trapHandler = [](Address /*pc*/) { /* ignore traps */ };

  auto reporter = TestReporting::createReporter(TestReporting::ReporterType::Minimal);

  reporter->testSuiteStarted(argv[1]);

  ondemand::parser parser;
  auto json = padded_string::load(argv[1]);
  // position a pointer at the beginning of the JSON data
  ondemand::document doc = parser.iterate(json);

  [[maybe_unused]] int32_t testCount = 0;
  int32_t failCount = 0;
  for (auto test : doc.get_array())
  {
    std::string_view name = test["name"].get_string().value();

    reporter->testCaseStarted(std::string{name});

    Snapshot initial = test["initial"].get<Snapshot>();
    Snapshot final = test["final"].get<Snapshot>();

    MicrocodePump<mos6502> pump;
    Generic6502Definition cpu_state(initial.regs);

    BusImpl bus{std::move(initial.memory)};

    test["cycles"].get(final.cycles);

    using BusToken = Generic6502Definition::BusToken;

    // Set a breakpoint if necessary
    // if (name != "31 29 9a")
    //{
    //  std::cout << "Skipping " << name << "\n";
    //  continue;
    //}

    LOG_INSTRUCTION_BYTES(cpu_state.registers, name);

    while (pump.tick(cpu_state, BusToken{&bus}))
    {
      // Keep executing until the instruction is finished.
    }

    std::ranges::sort(bus.memory.mem, {}, &MemoryLocation::address);
    std::ranges::sort(final.memory, {}, &MemoryLocation::address);

    Snapshot actual;
    actual.regs = cpu_state.registers;
    actual.memory = bus.memory.mem;
    actual.cycles = bus.cycles;

    if (actual != final)
    {
      {
        TestReporting::SectionScope registers(reporter.get(), "Registers");
        reporter->report("PC", final.regs.pc, actual.regs.pc);
        reporter->report("A", final.regs.a, actual.regs.a);
        reporter->report("X", final.regs.x, actual.regs.x);
        reporter->report("Y", final.regs.y, actual.regs.y);
        reporter->report("S", final.regs.sp, actual.regs.sp);
      }
      {
        using OptionalByte = std::optional<Common::Byte>;
        struct CombinedMemory
        {
          Common::Address address;
          OptionalByte expected;
          OptionalByte actual;
        };
        std::vector<CombinedMemory> unified;
        unified.reserve(actual.memory.size() + final.memory.size());

        std::ranges::transform(final.memory, std::back_inserter(unified),
            [](const MemoryLocation& loc) { return CombinedMemory{loc.address, loc.value, std::nullopt}; });
        for (const auto& loc : actual.memory)
        {
          auto it = std::ranges::find_if(unified, [addr = loc.address](const auto& m) { return m.address == addr; });
          if (it == unified.end())
          {
            unified.emplace_back(loc.address, std::nullopt, loc.value);
          }
          else
          {
            it->actual = loc.value;
          }
        }
        std::ranges::sort(unified, {}, &CombinedMemory::address);

        TestReporting::SectionScope registers(reporter.get(), "Memory");
        for (const auto& mem : unified)
        {
          if (mem.expected != mem.actual)
          {
            std::string field = std::format("{:04X}", static_cast<uint16_t>(mem.address));
            reporter->report(field, mem.expected.value_or(0), mem.actual.value_or(0));
          }
        }
      }
      {
        TestReporting::SectionScope registers(reporter.get(), "Cycles");
        size_t minSize = std::min(actual.cycles.size(), final.cycles.size());
        for (size_t i = 0; i < minSize; ++i)
        {
          const auto& expected = final.cycles[i];
          if (expected != actual.cycles[i])
          {
            std::string field = std::format("Cycle {}", i);
            if (expected.address != actual.cycles[i].address)
              reporter->report(field + " Address", expected.address, actual.cycles[i].address);
            reporter->report(field + " Data", expected.data, actual.cycles[i].data);
            reporter->report(field + " Type", expected.isRead, actual.cycles[i].isRead);
          }
        }
        if (actual.cycles.size() != final.cycles.size())
        {
          reporter->report("Cycle count", final.cycles.size(), actual.cycles.size());
        }
      }
      ++failCount;
    }
    ++testCount;
    reporter->testCaseFinished();
  }

  reporter->testSuiteFinished();
  std::cout << "Completed " << testCount << " tests with " << failCount << " failures.\n";

  return failCount;
}
