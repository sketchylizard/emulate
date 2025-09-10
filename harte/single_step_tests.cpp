#include <cstdint>
#include <fstream>
#include <iostream>
#include <ranges>
#include <unordered_map>

#include "common/address.h"
#include "common/bus.h"
#include "common/memory.h"
#include "common/microcode_pump.h"
#include "cpu6502/mos6502.h"
#include "cpu6502/state.h"
#include "simdjson.h"

using namespace simdjson;  // optional

using namespace Common;
using namespace cpu6502;

static constexpr std::string_view filename = "build/clang/debug/_deps/65x02-src/6502/v1/d0.json";

struct MemoryLocation
{
  Address address;
  Byte value;

  bool operator==(const MemoryLocation& other) const
  {
    return address == other.address && value == other.value;
  }
};

struct Snapshot
{
  VisibleState regs;
  std::vector<MemoryLocation> memory;
};

namespace simdjson
{

// This tag_invoke MUST be inside simdjson namespace

template<typename simdjson_value>
auto tag_invoke(deserialize_tag, simdjson_value& val, MemoryLocation& location)
{
  int32_t value;
  [[maybe_unused]] auto arr = val.get_array();
  auto it = arr.begin();

  auto error = (*it).get(value);
  if (error)
  {
    return error;
  }
  location.address = Address{static_cast<uint16_t>(value)};

  assert(it != arr.end());
  ++it;
  error = (*it).get(value);
  if (error)
  {
    return error;
  }
  location.value = static_cast<Byte>(value);
  return simdjson::SUCCESS;
}

template<typename simdjson_value>
auto tag_invoke(deserialize_tag, simdjson_value& val, BusRequest& request)
{
  int32_t value;
  [[maybe_unused]] auto arr = val.get_array();
  auto it = arr.begin();

  auto error = (*it).get(value);
  if (error)
  {
    return error;
  }
  request.address = Address{static_cast<uint16_t>(value)};

  assert(it != arr.end());
  ++it;
  error = (*it).get(value);
  if (error)
  {
    return error;
  }
  request.data = static_cast<Byte>(value);
  assert(it != arr.end());
  ++it;
  std::string_view controlStr;
  error = (*it).get(controlStr);
  if (error)
  {
    return error;
  }
  if (controlStr == "read")
  {
    request.control |= Control::Read;
  }
  else if (controlStr == "write")
  {
    request.control = Control::Write;
  }
  return simdjson::SUCCESS;
}


template<typename simdjson_value>
auto tag_invoke(deserialize_tag, simdjson_value& val, Snapshot& snapshot)
{
  ondemand::object obj;
  auto error = val.get_object().get(obj);
  if (error)
  {
    return error;
  }
  uint16_t tmp;
  if ((error = obj["pc"].get(tmp)))
  {
    return error;
  }
  snapshot.regs.pc = Address{tmp};
  if ((error = obj["s"].get(snapshot.regs.sp)))
  {
    return error;
  }
  if ((error = obj["a"].get(snapshot.regs.a)))
  {
    return error;
  }
  if ((error = obj["x"].get(snapshot.regs.x)))
  {
    return error;
  }
  if ((error = obj["y"].get(snapshot.regs.y)))
  {
    return error;
  }
  if ((error = obj["p"].get(snapshot.regs.p)))
  {
    return error;
  }
  for (auto location : obj["ram"].get_array())
  {
    snapshot.memory.push_back(MemoryLocation(location));
  }
  return simdjson::SUCCESS;
}

}  // namespace simdjson


struct SparseMemory
{
  std::vector<MemoryLocation> mem;

  BusResponse tick(const BusRequest& req)
  {
    if (req.isRead())
    {
      auto it = std::ranges::find_if(mem, [addr = req.address](const auto& pair) { return pair.address == addr; });
      if (it == mem.end())
      {
        std::cout << "Memory read of uninitialized address "
                  << std::format("{:04X}", static_cast<uint16_t>(req.address)) << ", returning 0x00" << std::endl;
        return BusResponse{0x00};
      }
      return BusResponse{it->value};
    }

    // Write
    auto it = std::ranges::find_if(mem, [addr = req.address](const auto& pair) { return pair.address == addr; });
    if (it == mem.end())
    {
      mem.emplace_back(req.address, req.data);
      return BusResponse{req.data};
    }
    it->value = req.data;
    return BusResponse{req.data, true};
  }

  bool operator==(const SparseMemory& other) const
  {
    return std::ranges::equal(mem, other.mem);
  }

  void readMemory(ondemand::array ram)
  {
    mem.clear();
    size_t count = ram.count_elements();
    mem.reserve(count);
    for (auto item : ram)
    {
      Address address;
      Common::Byte value;

      size_t i = 0;
      for (auto entry : item.get_array())
      {
        ++i;
        if (i == 1)
        {
          uint16_t tmp = entry.get<uint16_t>();
          address = Address{tmp};
          continue;
        }
        if (i == 2)
        {
          value = entry.get<uint8_t>();
          break;
        }
      }
      mem.emplace_back(Address{address}, value);
    }
  }
};

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cout << "Usage: " << argv[0] << " <testfile.json>" << std::endl;
    std::cout << "Using default test file: " << filename << std::endl;
    return 1;
  }

  std::cout << "Using test file: " << argv[1] << std::endl;

  SparseMemory memory;

  ondemand::parser parser;
  auto json = padded_string::load(argv[1]);
  ondemand::document doc = parser.iterate(json);  // position a pointer at the beginning of the JSON data

  MicrocodePump<mos6502> pump;

  BusResponse response{0x00};  // Start with default response

  std::vector<BusRequest> cycles;

  for (auto test : doc.get_array())
  {
    std::cout << "Test name: " << test["name"].get_string().value() << std::endl;

    pump = MicrocodePump<mos6502>{};

    Snapshot initial = test["initial"].get<Snapshot>();

    State cpu_state(initial.regs);

    memory.mem = std::move(initial.memory);

    for (auto cycle : test["cycles"].get_array())
    {
      BusRequest expected = cycle.get<BusRequest>();

      // Execute one CPU tick
      BusRequest actual = pump.tick(cpu_state, response);
      if (actual.address != expected.address && actual.isRead() == expected.isRead() &&
          actual.isWrite() == expected.isWrite())
      {
        throw std::runtime_error("Address mismatch");
      }

      // Execute memory operation and prepare response for next cycle
      response = memory.tick(actual);

      if (response.data != expected.data)
      {
        throw std::runtime_error("Data mismatch");
      }
    }
    Snapshot final = test["final"].get<Snapshot>();
    if (static_cast<const VisibleState&>(cpu_state) != State(final.regs))
    {
      throw std::runtime_error("CPU state mismatch");
    }

    std::ranges::sort(memory.mem, {}, &MemoryLocation::address);
    std::ranges::sort(final.memory, {}, &MemoryLocation::address);

    if (memory.mem != final.memory)
    {
      memory.mem.push_back({Address{0xFFFF}, 0xFF});  // Sentinel to terminate the loop
      final.memory.push_back({Address{0xFFFF}, 0xFF});  // Sentinel to terminate the loop
      std::cout << "Memory state mismatch:\n";
      auto endActual = memory.mem.end();
      auto endExpected = final.memory.end();
      auto itActual = memory.mem.begin();
      auto itExpected = final.memory.begin();
      for (; itActual != endActual && itExpected != endExpected;)
      {
        if (itActual->address < itExpected->address)
        {
          std::cout << std::format("  {:04X}: actual {:02X}, expected --\n", static_cast<uint16_t>(itActual->address),
              static_cast<int32_t>(itActual->value));
          ++itActual;
        }
        else if (itActual->address > itExpected->address)
        {
          std::cout << std::format("  {:04X}: actual --, expected {:02X}\n", static_cast<uint16_t>(itExpected->address),
              static_cast<int32_t>(itExpected->value));
          ++itExpected;
        }
        else
        {
          assert(itActual->address == itExpected->address);
          std::cout << std::format("  {:04X}: actual {:02X}, expected {:02X}\n",
              static_cast<uint16_t>(itActual->address), itActual->value, itExpected->value);
          ++itActual;
          ++itExpected;
        }
      }
      throw std::runtime_error("Memory state mismatch");
    }
  }

  return 0;
}

#if 0

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
#endif
