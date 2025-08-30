#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <string_view>

#include "common/Bus.h"
#include "common/address.h"

template<typename CpuState>
class MicrocodePump
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;
  using BusRequest = Common::BusRequest;
  using BusResponse = Common::BusResponse;
  using Instruction = typename CpuState::Instruction;
  using InstructionTable = std::span<const Instruction, 256>;

  // Callbacks (simple function pointers for speed)
  using CycleCallback = void (*)(const CpuState& state, BusRequest request, BusResponse response);
  using InstructionCallback = void (*)(const CpuState& state, uint8_t opcode);
  using TrapCallback = bool (*)(const CpuState& state, Address address);  // Return true to halt

  explicit MicrocodePump(InstructionTable instructions) noexcept;

  [[nodiscard]] BusRequest tick(BusResponse response) noexcept;

  //! Get the number of ticks since the last reset.
  [[nodiscard]] uint64_t tickCount() const noexcept;

  // State access
  [[nodiscard]] const CpuState& getState() const noexcept;
  [[nodiscard]] CpuState& getState() noexcept;  // For testing

  // Callback registration
  void setCycleCallback(CycleCallback callback) noexcept;
  void setInstructionCallback(InstructionCallback callback) noexcept;
  void setTrapCallback(TrapCallback callback) noexcept;

private:
  using Microcode = typename CpuState::Microcode;

  // State transition functions
  Microcode getNext(BusResponse response) noexcept;

  static BusRequest fetchNextOpcode(CpuState& cpu, BusResponse response);
  static BusRequest decodeOpcode(CpuState& cpu, BusResponse response);
  static BusRequest nextOp(CpuState& cpu, BusResponse response);

  InstructionTable m_instructions;

  const Instruction* m_instruction = nullptr;

  //! Called for every tick to perform the current action. Can be expensive.
  CycleCallback m_onCycle = nullptr;

  //! Called when a new instruction is started (once per instruction).
  InstructionCallback m_onInstructionStart = nullptr;

  //! Called when a trap condition is met. If it returns true, the CPU will halt.
  TrapCallback m_onTrap = nullptr;

  //! Number of ticks since the last reset
  uint64_t m_tickCount = 0;

  CpuState m_state;

  Byte m_currentMicrocode = 0;

  // Last bus request
  BusRequest m_lastBusRequest;
};

template<typename CpuState>
MicrocodePump<CpuState>::MicrocodePump(InstructionTable instructions) noexcept
  : m_instructions(instructions)
{
  m_state.next = &MicrocodePump::fetchNextOpcode;
}

template<typename CpuState>
typename CpuState::Microcode MicrocodePump<CpuState>::getNext(BusResponse response) noexcept
{
  // If last request was SYNC, we're receiving an opcode - decode it
  if (m_lastBusRequest.isSync())
  {
    m_instruction = &m_instructions[response.data];
    m_currentMicrocode = 0;

    // Notify instruction start
    if (m_onInstructionStart)
    {
      m_onInstructionStart(m_state, response.data);
    }
  }

  // Scenario 1: We have a valid next pointer in state
  if (m_state.next != nullptr)
  {
    auto microcode = m_state.next;
    m_state.next = nullptr;
    return microcode;
  }

  // Scenario 2: We're in the middle of an instruction
  if (m_instruction != nullptr && m_currentMicrocode < 7 && m_instruction->ops[m_currentMicrocode] != nullptr)
  {

    return m_instruction->ops[m_currentMicrocode++];
  }

  // Scenario 3: Need to fetch next opcode
  return &fetchNextOpcode;
}

template<typename CpuState>
Common::BusRequest MicrocodePump<CpuState>::tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  // Get the next microcode function to execute
  auto microcode = getNext(response);

  // Execute it
  m_lastBusRequest = microcode(m_state, response);

  // If the last bus request was a NOOP (i.e. no address, no data, no control), fetch next opcode
  if (!m_lastBusRequest)
  {
    m_lastBusRequest = fetchNextOpcode(m_state, response);
  }

  // Check for traps
  if (m_onTrap && m_onTrap(m_state, m_state.pc))
  {
    // request = BusRequest::Halt();  // Or however you handle halts
  }

  // Notify cycle completion
  if (m_onCycle)
  {
    m_onCycle(m_state, m_lastBusRequest, response);
  }

  return m_lastBusRequest;
}

template<typename CpuState>
inline uint64_t MicrocodePump<CpuState>::tickCount() const noexcept
{
  return m_tickCount;
}

template<typename CpuState>
Common::BusRequest MicrocodePump<CpuState>::fetchNextOpcode(CpuState& cpu, Common::BusResponse /*response*/)
{
  return Common::BusRequest::Fetch(cpu.pc++);
}

template<typename CpuState>
Common::BusRequest MicrocodePump<CpuState>::decodeOpcode(CpuState& cpu, Common::BusResponse response)
{
  // Decode opcode
  Byte opcode = response.data;
  const auto& instruction = cpu.m_instructions[static_cast<size_t>(opcode)];
  if (instruction.op[0] == nullptr)
  {
    throw std::runtime_error(std::format("Unknown opcode: ${:02X} at PC=${:04X}\n", opcode, cpu.pc - 1));
  }

  cpu.setInstruction(instruction);
  return cpu.m_action(cpu, BusResponse{});
}

template<typename CpuState>
const CpuState& MicrocodePump<CpuState>::getState() const noexcept
{
  return m_state;
}

template<typename CpuState>
CpuState& MicrocodePump<CpuState>::getState() noexcept
{
  return m_state;
}

template<typename CpuState>
void MicrocodePump<CpuState>::setCycleCallback(CycleCallback callback) noexcept
{
  m_onCycle = callback;
}

template<typename CpuState>
void MicrocodePump<CpuState>::setInstructionCallback(InstructionCallback callback) noexcept
{
  m_onInstructionStart = callback;
}

template<typename CpuState>
void MicrocodePump<CpuState>::setTrapCallback(TrapCallback callback) noexcept
{
  m_onTrap = callback;
}
