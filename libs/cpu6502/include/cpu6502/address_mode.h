#pragma once

#include <cstdint>
#include <span>

#include "common/bus.h"
#include "cpu6502/state.h"

namespace cpu6502
{

struct AddressMode
{
  using Type = State::AddressModeType;

  // clang-format off
  static constexpr auto Implied     = Type::Implied;
  static constexpr auto Accumulator = Type::Accumulator;
  static constexpr auto Immediate   = Type::Immediate;
  static constexpr auto ZeroPage    = Type::ZeroPage;
  static constexpr auto ZeroPageX   = Type::ZeroPageX;
  static constexpr auto ZeroPageY   = Type::ZeroPageY;
  static constexpr auto Absolute    = Type::Absolute;
  static constexpr auto AbsoluteX   = Type::AbsoluteX;
  static constexpr auto AbsoluteY   = Type::AbsoluteY;
  static constexpr auto Indirect    = Type::Indirect;
  static constexpr auto IndirectZpX = Type::IndirectZpX;
  static constexpr auto IndirectZpY = Type::IndirectZpY;
  static constexpr auto Relative    = Type::Relative;
  // clang-format on

  // Define some microcode functions for addressing modes

  // Performs a fetch of the next opcode from the PC and increments the PC.
  static Common::BusRequest requestOpcode(State& cpu, Common::BusResponse response);

  // Makes a request to read the low byte of an address from the PC and increments the PC.
  static Common::BusRequest requestOperandLow(State& cpu, Common::BusResponse response);

  // Stores the incoming low byte into the operand and makes a request to read
  // the high byte from the PC, and increments the PC.
  static Common::BusRequest requestOperandHigh(State& cpu, Common::BusResponse response);

  // Stores the incoming hi byte into the operand and makes a request to read
  // from the effective address.
  static Common::BusRequest requestEffectiveAddress(State& cpu, Common::BusResponse response);

  // Stores the incoming hi byte into the operand and adjusts the low byte by the given index register.
  // If there is overflow, it sets up the next action to fix the page boundary crossing. Either way,
  // it will make a request to read from the effective address (it just might be the wrong one).
  template<Common::Byte State::* reg>
  static Common::BusRequest requestEffectiveAddressIndexed(State& cpu, Common::BusResponse response)
  {
    // The incoming response data is the high byte of the address
    cpu.hi = response.data;

    auto temp = static_cast<uint16_t>(cpu.lo);
    temp += (cpu.*reg);
    cpu.lo = static_cast<Common::Byte>(temp & 0xFF);

    // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
    // it's wrong and we'll schedule an extra step to request the right one.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    // Check for page boundary crossing
    if ((temp & 0xff00) != 0)
    {
      // Page crossing - need extra cycle
      cpu.next = [](State& cpu1, Common::BusResponse /*response*/)
      {
        // We already have the correct effectiveAddr from requestEffectiveAddressIndexed
        return Common::BusRequest::Read(Common::MakeAddress(cpu1.lo, cpu1.hi));
      };

      ++cpu.hi;

      // Read wrong address first
      return Common::BusRequest::Read(effectiveAddr);
    }

    // Read correct address
    return Common::BusRequest::Read(effectiveAddr);
  }

  // Stores the incoming data into the lo byte and makes a request to read from the zero page
  // address.
  static Common::BusRequest requestZeroPageAddress(State& cpu, Common::BusResponse response);

  template<Common::Byte State::* reg>
  static Common::BusRequest requestZeroPageAddressIndexed(State& cpu, Common::BusResponse response)
  {
    // The incoming response data is the low byte of the address (the high byte is always 0)
    cpu.lo = response.data + (cpu.*reg);

    // Always stays in zero page (hi = 0)
    return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00));
  }

  static constexpr State::Microcode immediate[] = {  //
      &AddressMode::requestOperandLow};

  static constexpr State::Microcode relative[] = {  //
      &AddressMode::requestOperandLow};

  static constexpr State::Microcode zeroPage[] = {  //
      &requestOperandLow,  //
      &requestZeroPageAddress};

  static constexpr State::Microcode zeroPageX[] = {  //
      &AddressMode::requestOperandLow,  //
      &AddressMode::requestZeroPageAddressIndexed<&State::x>};

  static constexpr State::Microcode zeroPageY[] = {  //
      &AddressMode::requestOperandLow,  //
      &AddressMode::requestZeroPageAddressIndexed<&State::y>};

  static constexpr State::Microcode absolute[] = {  //
      &requestOperandLow,  //
      &requestOperandHigh,  //
      &requestEffectiveAddress};

  static constexpr State::Microcode absoluteX[] = {  //
      &requestOperandLow,  //
      &requestOperandHigh,  //
      &requestEffectiveAddressIndexed<&State::x>};

  static constexpr State::Microcode absoluteY[] = {  //
      &requestOperandLow,  //
      &requestOperandHigh,  //
      &requestEffectiveAddressIndexed<&State::y>};

  // Helper function to get addressing mode microcode
  template<Type mode>
  static constexpr std::span<const State::Microcode> getAddressingOps()
  {
    switch (mode)
    {
      case AddressMode::Implied: return {};  // Empty span
      case AddressMode::Accumulator: return {};  // Empty span
      case AddressMode::Immediate: return immediate;
      case AddressMode::ZeroPage: return zeroPage;
      case AddressMode::ZeroPageX: return zeroPageX;
      case AddressMode::ZeroPageY: return zeroPageY;
      case AddressMode::Absolute: return absolute;
      case AddressMode::AbsoluteX: return absoluteX;
      case AddressMode::AbsoluteY: return absoluteY;
      case AddressMode::Indirect: throw std::out_of_range("Not implemented yet");
      case AddressMode::IndirectZpX: throw std::out_of_range("Not implemented yet");
      case AddressMode::IndirectZpY: throw std::out_of_range("Not implemented yet");
      case AddressMode::Relative: return relative;
    }
    throw std::out_of_range("Unknown addressing mode");
  }
};

}  // namespace cpu6502
