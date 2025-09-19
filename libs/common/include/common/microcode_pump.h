#pragma once

#include <concepts>
#include <cstdint>
#include <tuple>

// MicrocodePump - executes microcode operations in sequence, fetching opcodes as needed.
// The CpuDefinition template parameter must define the following types and static methods:

template<typename CpuDefinition>
class MicrocodePump
{
public:
  using State = typename CpuDefinition::State;
  using BusToken = typename CpuDefinition::BusToken;
  using Microcode = typename CpuDefinition::Microcode;

  MicrocodePump() = default;

  bool tick(State& cpu, BusToken bus)
  {
    if (!m_nextMicrocode)
    {
      m_nextMicrocode = CpuDefinition::fetchNextOpcode(cpu, bus);  // Fetch next opcode
    }
    else
    {
      m_nextMicrocode = m_nextMicrocode(cpu, bus).injection;
    }

    ++m_cycles;
    // Return true if there is more microcode to execute
    return m_nextMicrocode != nullptr;
  }

  [[nodiscard]] uint64_t cycles() const noexcept
  {
    return m_cycles;
  }

private:
  Microcode m_nextMicrocode = nullptr;
  uint64_t m_cycles = 0;  // Number of microcode operations executed
};
