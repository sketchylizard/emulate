#pragma once

#include <cstdint>
#include <tuple>

// MicrocodePump - executes microcode operations in sequence, fetching opcodes as needed.
// The CpuDefinition template parameter must define the following types and static methods:
// - using State = <CPU state structure>
// - using BusRequest = <Bus request type>
// - using BusResponse = <Bus response type>
// - using Microcode = BusRequest (*)(State&, BusResponse response);
// - static BusRequest fetchNextOpcode();
template<typename CpuDefinition>
class MicrocodePump
{
public:
  using State = typename CpuDefinition::State;
  using BusRequest = typename CpuDefinition::BusRequest;
  using BusResponse = typename CpuDefinition::BusResponse;
  using Microcode = typename CpuDefinition::Microcode;

  MicrocodePump() = default;

  [[nodiscard]] BusRequest tick(State& state, BusResponse response);

  [[nodiscard]] uint64_t microcodeCount() const noexcept
  {
    return m_microcodeCount;
  }

  [[nodiscard]] uint64_t cyclesSinceLastFetch() const noexcept
  {
    return m_cyclesSinceLastFetch;
  }

private:
  // Get next microcode to execute
  Microcode getNextMicrocode(BusResponse response) noexcept;

  uint64_t m_microcodeCount = 0;  // Number of microcode operations executed

  // Microcode execution state
  Microcode m_injectedOp = nullptr;  // Head of queue if not null - for page crossing penalties etc.
  Microcode* m_currentBegin = nullptr;  // Current position in microcode sequence
  Microcode* m_currentEnd = nullptr;  // End of current microcode sequence

  uint8_t m_cyclesSinceLastFetch = 0;  // Number of cycles since the last fetch operation.
  bool m_shouldDecode = false;  // Flag: decode on next tick
};

template<typename CpuDefinition>
typename CpuDefinition::Microcode MicrocodePump<CpuDefinition>::getNextMicrocode(BusResponse response) noexcept
{
  // Priority 1: Execute injected operation (for page crossing, etc.)
  if (m_injectedOp != nullptr)
  {
    auto op = m_injectedOp;
    m_injectedOp = nullptr;
    return op;
  }

  // Priority 2: Decode if flagged from previous cycle
  if (m_shouldDecode)
  {
    m_cyclesSinceLastFetch = 0;
    m_shouldDecode = false;
    auto [begin, end] = CpuDefinition::decodeOpcode(response.data);
    m_currentBegin = begin;
    m_currentEnd = end;
  }

  // Priority 3: Execute from current microcode sequence
  if (m_currentBegin != m_currentEnd && *m_currentBegin != nullptr)
  {
    auto op = *m_currentBegin;
    ++m_currentBegin;
    return op;
  }

  // Priority 4: Nothing to execute, get fetch operation and set decode flag
  m_shouldDecode = true;  // Decode the response on next tick
  return CpuDefinition::fetchNextOpcode;
}

template<typename CpuDefinition>
typename CpuDefinition::BusRequest MicrocodePump<CpuDefinition>::tick(State& state, BusResponse response)
{
  // Get next microcode to execute
  auto microcode = getNextMicrocode(response);

  ++m_microcodeCount;
  ++m_cyclesSinceLastFetch;

  // Execute it
  auto [request, injection] = microcode(state, response);

  if (!request && !injection)
  {
    // Previous instruction is done, we need to fetch next opcode
    m_shouldDecode = true;  // Decode the response on next tick
    return CpuDefinition::fetchNextOpcode(state, response).request;
  }

  // Handle microcode injection (for page crossing penalties, etc.) If it's null then nothing to inject.
  m_injectedOp = injection;
  return request;
}
