#pragma once

#include <cstdint>
#include <span>

#include "common/bus.h"
#include "common/microcode.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

namespace cpu6502
{

struct AddressMode
{
  using Type = State::AddressModeType;

  // Define some microcode functions for addressing modes

  // Makes a request to read the low byte of an address from the PC and increments the PC.
  static MicrocodeResponse requestOperandLow(State& cpu, Common::BusResponse response);

  // Stores the incoming low byte into the operand and makes a request to read
  // the high byte from the PC, and increments the PC.
  static MicrocodeResponse requestOperandHigh(State& cpu, Common::BusResponse response);

  // Stores the incoming hi byte into the operand and makes a request to read
  // from the effective address.
  static MicrocodeResponse requestEffectiveAddress(State& cpu, Common::BusResponse response);
};

struct Implied : AddressMode
{
  static constexpr auto type = AddressMode::Type::Implied;
  static constexpr std::span<const Microcode> ops = {};
};

struct Accumulator : AddressMode
{
  static constexpr auto type = AddressMode::Type::Accumulator;
  static constexpr std::span<const Microcode> ops = {};
};

struct Relative : AddressMode
{
  static constexpr auto type = AddressMode::Type::Relative;
  static constexpr std::span<const Microcode> ops = {};
};

struct Immediate : AddressMode
{
  static constexpr auto type = AddressMode::Type::Immediate;

  static constexpr const Microcode ops[] = {
      &requestOperandLow,
  };
};

template<Common::Byte State::* reg = nullptr>
struct ZeroPage : AddressMode
{
  static MicrocodeResponse requestAddress(State& cpu, Common::BusResponse response)
  {
    // The incoming response data is the low byte of the address (the high byte is always 0)
    cpu.lo = response.data;

    Microcode extraStep = nullptr;

    if constexpr (reg != nullptr)
    {
      // If the register is not null, schedule another step to adjust the low byte by the index
      // register.
      extraStep = &requestAddressAfterIndexing;
    }

    return {BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00)), extraStep};
  }

  static MicrocodeResponse requestAddressAfterIndexing(State& cpu, Common::BusResponse /*response*/)
  {
    // If this function is called, it means we need to adjust the low byte by the index register.
    assert(reg != nullptr);

    // Add the index register to the low byte, wrapping around within the zero page.
    cpu.lo = static_cast<Common::Byte>((cpu.lo + (cpu.*reg)) & 0xFF);

    // Always stays in zero page (hi = 0)
    return {BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00))};
  }

  // clang-format off
  static constexpr auto type =
       reg == nullptr   ? State::AddressModeType::ZeroPage :
      (reg == &State::x ? State::AddressModeType::ZeroPageX :
                          State::AddressModeType::ZeroPageY);
  // clang-format on

  static constexpr const Microcode ops[] = {
      &requestOperandLow, &requestAddress,
      // requestAddressAfterIndexing is added conditionally if reg != nullptr
  };
};

template<Common::Byte State::* reg = nullptr>
struct Absolute
{
  // clang-format off
  static constexpr auto type =
       reg == nullptr   ? State::AddressModeType::Absolute :
      (reg == &State::x ? State::AddressModeType::AbsoluteX :
                          State::AddressModeType::AbsoluteY);
  // clang-format on

  static MicrocodeResponse requestAddress(State& cpu, Common::BusResponse response)
  // Stores the incoming hi byte into the operand and adjusts the low byte by the given index register.
  // If there is overflow, it sets up the next action to fix the page boundary crossing. Either way,
  // it will make a request to read from the effective address (it just might be the wrong one).
  {
    // The incoming response data is the high byte of the address
    cpu.hi = response.data;

    auto temp = static_cast<uint16_t>(cpu.lo);

    if constexpr (reg != nullptr)
    {
      temp += (cpu.*reg);
      cpu.lo = static_cast<Common::Byte>(temp & 0xFF);
    }

    // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
    // it's wrong and we'll schedule an extra step to request the right one.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    // Check for page boundary crossing
    if ((temp & 0xff00) != 0)
    {
      // Page crossing - need extra cycle
      auto fixupCycle = [](State& cpu1, Common::BusResponse /*response*/) -> MicrocodeResponse
      {
        // We already have the correct effectiveAddr from requestEffectiveAddressIndexed
        return {BusRequest::Read(Common::MakeAddress(cpu1.lo, cpu1.hi)), nullptr};
      };

      ++cpu.hi;

      // Read wrong address first
      return {BusRequest::Read(effectiveAddr), fixupCycle};
    }

    // Read correct address
    return {BusRequest::Read(effectiveAddr)};
  }

  static constexpr const Microcode ops[] = {
      &AddressMode::requestOperandLow,
      &AddressMode::requestOperandHigh,
      &requestAddress,
  };
};

struct AbsoluteJmp
{
  static constexpr auto type = State::AddressModeType::Absolute;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestOperandLow,
      &AddressMode::requestOperandHigh,
  };
};

struct AbsoluteIndirectJmp
{
  static constexpr auto type = State::AddressModeType::Indirect;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestOperandLow,
      &AddressMode::requestOperandHigh,
  };
};

}  // namespace cpu6502
