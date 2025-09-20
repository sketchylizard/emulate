#pragma once

#include <cstdint>
#include <vector>

#include "common/address.h"
#include "common/bus.h"
#include "cpu6502/registers.h"
#include "simdjson.h"


//! A memory location (address and value) in a sparse representation
struct MemoryLocation
{
  Common::Address address;
  Common::Byte value;

  bool operator==(const MemoryLocation& other) const
  {
    return address == other.address && value == other.value;
  }
};

struct Cycle
{
  Common::Address address;
  Common::Byte data;
  bool isRead;

  bool operator==(const Cycle& cycle) const noexcept = default;
};

//! Snapshot of CPU registers and memory at a point in time
struct Snapshot
{
  cpu6502::Registers regs;
  std::vector<MemoryLocation> memory;
  std::vector<Cycle> cycles;

  bool operator==(const Snapshot& other) const
  {
    bool registersMatch = regs == other.regs;
    bool memoryMatch = memory == other.memory;
    bool cyclesMatch = cycles == other.cycles;
    return registersMatch && memoryMatch && cyclesMatch;
  }
};

//! Wrapper around a vector of MemoryLocations to provide tick() method for memory operations
struct SparseMemory
{
  std::vector<MemoryLocation> mem;

  SparseMemory(std::vector<MemoryLocation>&& incoming)
    : mem(std::move(incoming))
  {
  }

  Common::Byte read(Common::Address address);
  void write(Common::Address address, Common::Byte data);
};

void reportError(std::string_view name, const Snapshot& expected, const Snapshot& actual);

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
  location.address = Common::Address{static_cast<uint16_t>(value)};

  assert(it != arr.end());
  ++it;
  error = (*it).get(value);
  if (error)
  {
    return error;
  }
  location.value = static_cast<Common::Byte>(value);
  return simdjson::SUCCESS;
}

template<typename simdjson_value>
auto tag_invoke(deserialize_tag, simdjson_value& val, Cycle& cycle)
{
  int32_t value;
  auto arr = val.get_array();
  auto it = arr.begin();

  auto error = (*it).get(value);
  if (error)
  {
    return error;
  }
  cycle.address = Common::Address{static_cast<uint16_t>(value)};

  assert(it != arr.end());
  ++it;
  error = (*it).get(value);
  if (error)
  {
    return error;
  }
  cycle.data = static_cast<Common::Byte>(value);
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
    cycle.isRead = true;
  }
  else if (controlStr == "write")
  {
    cycle.isRead = false;
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
  snapshot.regs.pc = Common::Address{tmp};
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
