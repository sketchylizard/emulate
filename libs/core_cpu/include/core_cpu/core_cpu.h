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

inline constexpr Common::Byte c_ZeroPage{0x00};
inline constexpr Common::Byte c_StackPage{0x01};

template<typename StateType>
class Core65xx
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;
  using BusRequest = Common::BusRequest;
  using BusResponse = Common::BusResponse;

  enum class Register
  {
    A,
    X,
    Y
  };

  static constexpr Address c_nmiVector = Address{0xFFFA};
  static constexpr Address c_resetVector = Address{0xFFFC};
  static constexpr Address c_irqVector = Address{0xFFFE};
  static constexpr Address c_brkVector = Address{0xFFFE};

  struct Instruction
  {
    std::string_view name;
    StateType::Microcode op[7] = {};
  };

  Core65xx(std::span<const Instruction, 256> isa) noexcept;

  [[nodiscard]] BusRequest tick(BusResponse response) noexcept;

  //! Get the number of ticks since the last reset.
  [[nodiscard]] uint64_t tickCount() const noexcept;

private:
  // State transition functions

  static BusRequest decodeOpcode(StateType& cpu, BusResponse response);
  static BusRequest nextOp(StateType& cpu, BusResponse response);

  std::span<const Instruction, 256> m_instructions;

  const Instruction* m_instruction = nullptr;

  uint64_t m_tickCount = 0;  // Number of ticks since the last reset

  StateType m_state;

  // Stage of the current instruction (0-2)
  Byte m_stage = 0;

  // Last bus request
  BusRequest m_lastBusRequest;
};

template<typename StateType>
Core65xx<StateType>::Core65xx(std::span<const Instruction, 256> isa) noexcept
  : m_instructions(isa)
{
}

template<typename StateType>
Common::BusRequest Core65xx<StateType>::tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  assert(m_state.m_action);

  // Execute the current action until it calls nextOp.
  m_lastBusRequest = m_action(*this, response);

  return m_lastBusRequest;
}

template<typename StateType>
inline uint64_t Core65xx<StateType>::tickCount() const noexcept
{
  return m_tickCount;
}

template<typename StateType>
Common::BusRequest Core65xx<StateType>::decodeOpcode(StateType& cpu, Common::BusResponse response)
{
  // Decode opcode
  Byte opcode = response.data;
  const auto& instruction = cpu.m_instructions[static_cast<size_t>(opcode)];
  if (instruction.op[0] == nullptr)
  {
    throw std::runtime_error(std::format("Unknown opcode: ${:02X} at PC=${:04X}\n", opcode, cpu.regs.pc - 1));
  }

  cpu.setInstruction(instruction);
  return cpu.m_action(cpu, BusResponse{});
}
