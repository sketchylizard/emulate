#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/bus.h"
#include "common/memory.h"
#include "common/microcode_pump.h"
#include "cpu6502/mos6502.h"
#include "cpu6502/state.h"

using json = nlohmann::json;
using namespace Common;
using namespace cpu6502;

// Structure to represent a single test cycle
struct TestCycle
{
  Address address;
  Byte value;
  std::string type;  // "read", "write", "sync"

  BusRequest toBusRequest() const
  {
    if (type == "sync")
    {
      return BusRequest::Fetch(address);
    }
    else if (type == "read")
    {
      return BusRequest::Read(address);
    }
    else if (type == "write")
    {
      return BusRequest::Write(address, value);
    }
    return BusRequest{address, value, Control::None};
  }
};

// Structure to represent initial/final CPU state
struct CpuState
{
  Address pc{0};
  Byte a{0};
  Byte x{0};
  Byte y{0};
  Byte sp{0};
  Byte p{0};

  void applyTo(State& state) const
  {
    state.pc = pc;
    state.a = a;
    state.x = x;
    state.y = y;
    state.sp = sp;
    state.p = p;
  }

  bool matches(const State& state) const
  {
    return state.pc == pc && state.a == a && state.x == x && state.y == y && state.sp == sp && state.p == p;
  }

  std::string toString() const
  {
    return std::format("PC:{:04X} A:{:02X} X:{:02X} Y:{:02X} SP:{:02X} P:{:02X}", static_cast<uint16_t>(pc), a, x, y, sp, p);
  }
};

// Structure to represent memory changes
struct MemoryChange
{
  Address address;
  Byte value;
};

// Structure to represent a complete test case
struct TestCase
{
  std::string name;
  CpuState initial_state;
  std::vector<MemoryChange> initial_memory;
  CpuState final_state;
  std::vector<MemoryChange> final_memory;
  std::vector<TestCycle> cycles;
};

// JSON parsing functions
void from_json(const json& j, TestCycle& cycle)
{
  cycle.address = Address{j.at("address").get<uint16_t>()};
  cycle.value = j.at("value").get<uint8_t>();
  cycle.type = j.at("type").get<std::string>();
}

void from_json(const json& j, CpuState& state)
{
  state.pc = Address{j.at("pc").get<uint16_t>()};
  state.a = j.at("a").get<uint8_t>();
  state.x = j.at("x").get<uint8_t>();
  state.y = j.at("y").get<uint8_t>();
  state.sp = j.at("s").get<uint8_t>();  // Note: JSON uses 's' for stack pointer
  state.p = j.at("p").get<uint8_t>();
}

void from_json(const json& j, MemoryChange& change)
{
  change.address = Address{j.at("address").get<uint16_t>()};
  change.value = j.at("value").get<uint8_t>();
}

void from_json(const json& j, TestCase& test)
{
  test.name = j.at("name").get<std::string>();
  test.initial_state = j.at("initial").get<CpuState>();
  test.initial_memory = j.at("ram").get<std::vector<MemoryChange>>();
  test.final_state = j.at("final").get<CpuState>();
  test.final_memory = j.at("ram").get<std::vector<MemoryChange>>();
  test.cycles = j.at("cycles").get<std::vector<TestCycle>>();
}

class SingleStepTestRunner
{
private:
  std::array<Byte, 65536> memory_array{};
  MicrocodePump<mos6502> pump;
  MemoryDevice<Byte> memory;
  State cpu_state;

public:
  SingleStepTestRunner()
    : memory(memory_array)
  {
  }

  bool runTest(const TestCase& test)
  {
    // Reset everything
    memory_array.fill(0);
    pump = MicrocodePump<mos6502>{};
    cpu_state = State{};

    // Apply initial state
    test.initial_state.applyTo(cpu_state);

    // Apply initial memory
    for (const auto& change : test.initial_memory)
    {
      memory_array[static_cast<size_t>(change.address)] = change.value;
    }

    // Run each cycle
    BusResponse response{0x00};  // Start with default response

    for (size_t cycle_idx = 0; cycle_idx < test.cycles.size(); ++cycle_idx)
    {
      const auto& expected_cycle = test.cycles[cycle_idx];

      // Execute one CPU tick
      BusRequest actual_request = pump.tick(cpu_state, response);
      BusRequest expected_request = expected_cycle.toBusRequest();

      // Verify bus request matches expected
      if (actual_request != expected_request)
      {
        UNSCOPED_INFO("Test: " << test.name);
        UNSCOPED_INFO("Cycle " << cycle_idx << " bus request mismatch:");
        UNSCOPED_INFO("  Expected: " << formatBusRequest(expected_request));
        UNSCOPED_INFO("  Actual:   " << formatBusRequest(actual_request));
        UNSCOPED_INFO("  CPU State: " << getCurrentState().toString());

        // Log all cycles for context
        logAllCycles(test, cycle_idx);
        return false;
      }

      // Execute memory operation and prepare response for next cycle
      BusResponse memory_response = memory.tick(actual_request);

      // For next cycle, the response data should be what was read/written
      if (actual_request.isRead() || actual_request.isSync())
      {
        response.data = memory_response.data;
      }
      else
      {
        response.data = actual_request.data;  // Write operations
      }
      response.ready = memory_response.ready;
    }

    // Verify final CPU state
    if (!test.final_state.matches(cpu_state))
    {
      UNSCOPED_INFO("Test: " << test.name);
      UNSCOPED_INFO("Final CPU state mismatch:");
      UNSCOPED_INFO("  Expected: " << test.final_state.toString());
      UNSCOPED_INFO("  Actual:   " << getCurrentState().toString());
      logAllCycles(test, test.cycles.size());
      return false;
    }

    // Verify final memory state
    for (const auto& expected_change : test.final_memory)
    {
      Byte actual_value = memory_array[static_cast<size_t>(expected_change.address)];
      if (actual_value != expected_change.value)
      {
        UNSCOPED_INFO("Test: " << test.name);
        UNSCOPED_INFO("Final memory mismatch at " << expected_change.address << ":");
        UNSCOPED_INFO("  Expected: $" << std::format("{:02X}", expected_change.value));
        UNSCOPED_INFO("  Actual:   $" << std::format("{:02X}", actual_value));
        logAllCycles(test, test.cycles.size());
        return false;
      }
    }

    return true;  // Test passed
  }

private:
  CpuState getCurrentState() const
  {
    CpuState current;
    current.pc = cpu_state.pc;
    current.a = cpu_state.a;
    current.x = cpu_state.x;
    current.y = cpu_state.y;
    current.sp = cpu_state.sp;
    current.p = cpu_state.p;
    return current;
  }

  std::string formatBusRequest(const BusRequest& req) const
  {
    if (req.isSync())
    {
      return std::format("SYNC {:04X}", static_cast<uint16_t>(req.address));
    }
    else if (req.isRead())
    {
      return std::format("READ {:04X}", static_cast<uint16_t>(req.address));
    }
    else if (req.isWrite())
    {
      return std::format("WRITE {:04X} = {:02X}", static_cast<uint16_t>(req.address), req.data);
    }
    else
    {
      return std::format("NONE {:04X} {:02X}", static_cast<uint16_t>(req.address), req.data);
    }
  }

  void logAllCycles(const TestCase& test, size_t failed_at_cycle) const
  {
    UNSCOPED_INFO("Complete cycle trace:");
    for (size_t i = 0; i < test.cycles.size(); ++i)
    {
      const auto& cycle = test.cycles[i];
      std::string marker = (i == failed_at_cycle) ? " <-- FAILED HERE" : "";
      UNSCOPED_INFO("  Cycle " << i << ": " << formatBusRequest(cycle.toBusRequest()) << marker);
    }

    UNSCOPED_INFO("Initial state: " << test.initial_state.toString());
    UNSCOPED_INFO("Expected final: " << test.final_state.toString());
    UNSCOPED_INFO("Actual final:   " << getCurrentState().toString());

    if (!test.initial_memory.empty())
    {
      UNSCOPED_INFO("Initial memory:");
      for (const auto& mem : test.initial_memory)
      {
        UNSCOPED_INFO("  " << mem.address << " = $" << std::format("{:02X}", mem.value));
      }
    }

    if (!test.final_memory.empty())
    {
      UNSCOPED_INFO("Expected final memory:");
      for (const auto& mem : test.final_memory)
      {
        Byte actual = memory_array[static_cast<size_t>(mem.address)];
        std::string match = (actual == mem.value) ? " ✓" : " ✗";
        UNSCOPED_INFO("  " << mem.address << " = $" << std::format("{:02X}", mem.value) << " (actual: $"
                           << std::format("{:02X}", actual) << ")" << match);
      }
    }
  }
};

// Function to load test cases from JSON file
json loadTestsFromFile(const std::string& filename)
{
  std::ifstream file(filename);
  if (!file.is_open())
  {
    throw std::runtime_error("Could not open test file: " + filename);
  }

  json j;
  file >> j;

  return j;
}

// Alternative test function for testing specific opcodes
TEST_CASE("SingleStep Specific Opcode", "[single_step][manual]")
{
  const std::string test_file = "build/clang/debug/_deps/65x02-src/6502/v1/ea.json";  // NOP instruction

  if (!std::filesystem::exists(test_file))
  {
    SKIP("Test file not found: " + test_file);
    return;
  }

  SingleStepTestRunner runner;
  auto test_cases = loadTestsFromFile(test_file);

  for (auto test : test_cases)
  {
    DYNAMIC_SECTION("NOP Test " << ": " << test["name"].get<std::string>())
    {
      // bool passed = runner.runTest(test);
      CHECK(false);
    }
  }
}
