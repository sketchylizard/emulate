#include "core_cpu/core_cpu.h"

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

using namespace Common;

#if 0
BusRequest CoreCpu::nextOp(CoreCpu& cpu, BusResponse response)
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
    cpu.m_action = &CoreCpu::fetchNextOpcode;
    return fetchNextOpcode(cpu, BusResponse{});
  }

  cpu.m_action = cpu.m_instruction->op[cpu.m_stage];
  return cpu.m_action(cpu, response);
}
#endif
