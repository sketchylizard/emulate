#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>

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

// JSON parsing functions
BusRequest getRequest(const json& j)
{
  BusRequest req;
  auto [address, data, typeStr] = j.get<std::tuple<Address, Byte, std::string>>();

  if (typeStr == "read")
  {
    req = BusRequest::Read(address);
  }
  else if (typeStr == "write")
  {
    req = BusRequest::Write(address, data);
  }
  else if (typeStr == "sync")
  {
    req = BusRequest::Fetch(address);
  }
  else
  {
    throw std::runtime_error("Unknown bus request type: " + typeStr);
  }
  return req;
}

VisibleState getState(const json& j)
{
  VisibleState state;

  state.pc = Address{j.at("pc").get<uint16_t>()};
  state.a = j.at("a").get<uint8_t>();
  state.x = j.at("x").get<uint8_t>();
  state.y = j.at("y").get<uint8_t>();
  state.sp = j.at("s").get<uint8_t>();  // Note: JSON uses 's' for stack pointer
  state.p = j.at("p").get<uint8_t>();

  return state;
}

struct SparseMemory
{
  std::vector<std::pair<Address, Byte>> mem;

  void set(Address addr, Byte value)
  {
    auto it = std::ranges::find_if(mem, [addr](const auto& pair) { return pair.first == addr; });
    if (it == mem.end())
    {
      mem.emplace_back(addr, value);
      return;
    }
    it->second = value;
  }
  Byte get(Address addr) const
  {
    auto it = std::ranges::find_if(mem, [addr](const auto& pair) { return pair.first == addr; });
    if (it == mem.end())
    {
      std::cout << "Memory read of uninitialized address " << std::format("{:04X}", static_cast<uint16_t>(addr))
                << ", returning 0x00" << std::endl;
      return 0x00;
    }
    return it->second;
  }

  BusResponse tick(const BusRequest& req)
  {
    if (req.isRead())
    {
      return BusResponse{get(req.address)};
    }
    set(req.address, req.data);
    return BusResponse{req.data, true};
  }

  bool operator==(const SparseMemory& other) const
  {
    return std::ranges::equal(mem, other.mem);
  }
};

SparseMemory readMemory(const json& j)
{
  SparseMemory memory;
  memory.mem.clear();
  memory.mem.reserve(j.size());
  const auto& items = j.items();
  for (const auto& item : items)
  {
    auto address = Address{item.value()[0].get<uint16_t>()};
    Byte value = item.value()[1].get<uint8_t>();
    memory.mem.emplace_back(address, value);
  }
  return memory;
}

class SingleStepTestRunner
{
private:
  MicrocodePump<mos6502> pump;
  SparseMemory memory;
  State cpu_state;

public:
  SingleStepTestRunner() = default;

  bool runTest(const json& test)
  {
    static size_t count = 0;
    if (++count % 10 == 0)
    {
      std::cout << "Running test #" << count << ": " << test.at("name").get<std::string>() << std::endl;
    }

    // Reset everything
    auto initial = test.at("initial");

    memory = readMemory(initial.at("ram"));

    pump = MicrocodePump<mos6502>{};

    // Apply initial state
    cpu_state = State(getState(initial));

    // Run each cycle
    BusResponse response{0x00};  // Start with default response

    const auto& cycles = test.at("cycles");
    for (const auto& cycle : cycles)
    {
      auto [address, data, typeStr] = cycle.get<std::tuple<Address, Byte, std::string>>();
      // Execute one CPU tick
      BusRequest actual_request = pump.tick(cpu_state, response);
      if (actual_request.address == address &&  // actual_request.data == data &&
          ((typeStr == "write" && actual_request.isWrite())  //
              || (typeStr == "sync" && actual_request.isSync())  //
              || (typeStr == "read" && actual_request.isRead())))
      {
        // Request matches expected
      }
      else
      {
        UNSCOPED_INFO("Test: " << test.at("name").get<std::string>());
        UNSCOPED_INFO("Cycle bus request mismatch:");
#if 0
        UNSCOPED_INFO("  Expected: " << typeStr << " " << std::format("{:04X}", static_cast<uint16_t>(address))
                                     << (typeStr == "write" ? " = " + std::format("{:02X}", data) : ""));
        UNSCOPED_INFO("  Actual:   " << (actual_request.isSync()     ? "sync " :
                                            actual_request.isRead()  ? "read " :
                                            actual_request.isWrite() ? "write " :
                                                                       "none ")
                                     << std::format("{:04X}", static_cast<uint16_t>(actual_request.address))
                                     << (actual_request.isWrite() ? " = " + std::format("{:02X}", actual_request.data) : ""));
        UNSCOPED_INFO("  CPU State: " << getCurrentState().toString());
#endif
        return false;
      }

      // Execute memory operation and prepare response for next cycle
      BusResponse memory_response = memory.tick(actual_request);
      response.data = memory_response.data;
    }

    const auto& final = test.at("final");

    // Verify final CPU state
    auto finalState = getState(final);
    if (static_cast<const VisibleState&>(cpu_state) != finalState)
    {
      UNSCOPED_INFO("Test: " << test["name"]);
      UNSCOPED_INFO("Final CPU state mismatch:");
#if 0
      UNSCOPED_INFO("  Expected: " << finalState);
      UNSCOPED_INFO("  Actual:   " << cpu_state);
#endif
      // logAllCycles(test, test.cycles.size());
      return false;
    }

    // Verify final memory state
    auto finalMemory = readMemory(final.at("ram"));
    return memory == finalMemory;
  }
#if 0
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
#endif
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
      bool passed = runner.runTest(test);
      CHECK(passed);
    }
  }
}
