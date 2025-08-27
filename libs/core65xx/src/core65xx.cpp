#include "core65xx/core65xx.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

#include "common/Bus.h"
#include "core65xx/address_mode.h"

using namespace Common;

Core65xx::Core65xx(std::span<const Instruction, 256> isa) noexcept
  : m_instructions(isa)
{
}

Common::BusRequest Core65xx::Tick(Common::BusResponse response) noexcept
{
  ++m_tickCount;

  assert(m_action);

  // Execute the current action until it calls nextOp.
  m_lastBusRequest = m_action(*this, response);

  return m_lastBusRequest;
}

BusRequest Core65xx::fetchNextOpcode(Core65xx& cpu, BusResponse /*response*/)
{
  // Fetch the next opcode
  cpu.m_action = &Core65xx::decodeOpcode;
  return BusRequest::Fetch(cpu.regs.pc++);
}

BusRequest Core65xx::decodeOpcode(Core65xx& cpu, BusResponse response)
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

void Core65xx::setInstruction(const Instruction& instr) noexcept
{
  m_instruction = &instr;

  m_stage = 0;

  assert(m_instruction);
  m_action = m_instruction->op[0];
  assert(m_action);
}

BusRequest Core65xx::nextOp(Core65xx& cpu, BusResponse response)
{
  assert(cpu.m_instruction);

  // Find the next action in the instruction's operation sequence.
  // If there are no more actions, finish the operation.
  do
  {
    ++cpu.m_stage;
  } while (cpu.m_stage < std::size(cpu.m_instruction->op) && !cpu.m_instruction->op[cpu.m_stage]);
  if (cpu.m_stage >= std::size(cpu.m_instruction->op))
  {
    // Instruction complete, log the last instruction.

    cpu.m_instruction = nullptr;
    cpu.m_action = &Core65xx::fetchNextOpcode;
    return fetchNextOpcode(cpu, BusResponse{});
  }

  cpu.m_action = cpu.m_instruction->op[cpu.m_stage];
  return cpu.m_action(cpu, response);
}
