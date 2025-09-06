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
  static MicrocodeResponse requestAddress8(State& cpu, Common::BusResponse response);

  // Makes requests to read a full 16-bit address (low byte first, then high byte) from the PC,
  // incrementing the PC after each read. Requires 2 cycles.
  // After this function completes, cpu.lo will contain the low byte of the address, and
  // the next bus response will contain the high byte of the address.
  static MicrocodeResponse requestAddress16(State& state, BusResponse response);

  // Makes a request to read the value at the effective address stored in cpu.lo and cpu.hi.
  static MicrocodeResponse requestOperand(State& state, BusResponse response);

  // Makes a request to read the value at the effective address stored in cpu.lo and cpu.hi,
  // but ignores the data returned from the previous read.
  static MicrocodeResponse requestOperandIgnoreData(State& state, BusResponse response);

protected:
  template<Common::Byte State::* reg>
  static MicrocodeResponse preIndex(State& cpu, Common::BusResponse response)
  // Stores the incoming hi byte into the operand and adjusts the low byte by the given index register.
  // If there is overflow, it sets up the next action to fix the page boundary crossing. Either way,
  // it will make a request to read from the effective address (it just might be the wrong one).
  {
    // The incoming response data is the high byte of the address
    cpu.hi = response.data;

    auto temp = static_cast<uint16_t>(cpu.lo);

    temp += (cpu.*reg);
    cpu.lo = static_cast<Common::Byte>(temp & 0xFF);

    // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
    // it will be wrong wrong and we'll schedule an extra step to request the right one.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    // Check for page boundary crossing
    if ((temp & 0xff00) != 0)
    {
      // We've already calculated the incorrect address so we can go ahead and fix up the hi byte
      // for the next read.
      ++cpu.hi;

      // Read wrong address first
      return {BusRequest::Read(effectiveAddr), &fixupPageCrossing};
    }

    // Read correct address
    return {BusRequest::Read(effectiveAddr)};
  }

private:
  // Called as the second step of requestAddress16 to fetch the high byte after the low byte has been fetched.
  static MicrocodeResponse requestAddressHigh(State& state, BusResponse response);
  static MicrocodeResponse fixupPageCrossing(State& state, BusResponse response);
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
      &requestAddress8,
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
      &requestAddress8, &requestAddress,
      // requestAddressAfterIndexing is added conditionally if reg != nullptr
  };
};

struct Absolute : AddressMode
{
  static constexpr auto type = State::AddressModeType::Absolute;
  // clang-format on

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::requestOperand,  // Read from effective address
  };
};

struct AbsoluteX : AddressMode
{
  static constexpr auto type = State::AddressModeType::AbsoluteX;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::preIndex<&State::x>,
      &AddressMode::requestOperandIgnoreData,  // Read from effective address
  };
};

struct AbsoluteY : AddressMode
{
  static constexpr auto type = State::AddressModeType::AbsoluteY;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::preIndex<&State::y>,
      &AddressMode::requestOperandIgnoreData,  // Read from effective address
  };
};

struct AbsoluteJmp
{
  static constexpr auto type = State::AddressModeType::Absolute;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
  };
};

struct AbsoluteIndirectJmp
{
  static constexpr auto type = State::AddressModeType::Indirect;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
  };
};

template<Common::Byte State::* reg = nullptr>
struct Indirect : AddressMode
{
  // clang-format off
  static constexpr auto type =
       reg == &State::x ? State::AddressModeType::IndirectZpX :
      (reg == &State::y ? State::AddressModeType::IndirectZpY :
                          State::AddressModeType::Indirect);
  // clang-format on

  static MicrocodeResponse requestAddress(State& cpu, Common::BusResponse response)
  {
    // The incoming response data is the low byte of a zero page address
    cpu.lo = response.data;

    // Indirect zp,X adds X to the low byte, wrapping around within the zero page, before reading
    // the byte at the given address.
    if constexpr (reg == &State::x)
    {
      // Add X to the low byte, wrapping around within the zero page
      cpu.lo = static_cast<Common::Byte>((cpu.lo + cpu.x) & 0xFF);
    }

    // This is the address that will be requested this cycle.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, 0x00);

    return {BusRequest::Read(effectiveAddr)};
  }

  static MicrocodeResponse requestAddress1(State& cpu, Common::BusResponse response)
  {
    // The incoming response data is the low byte of a zero page address
    cpu.lo = response.data;

    // Indirect zp,Y adds Y to the low byte, possibly crossing
    if constexpr (reg == &State::y)
    {
      // Add Y to the low byte, wrapping around within the zero page
      cpu.lo = static_cast<Common::Byte>((cpu.lo + cpu.y) & 0xFF);
    }

    // Fetch the high byte of the effective address from the next zero page location
    ++cpu.lo;  // Wraps around within zero page automatically
    return {BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00))};
  }
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &requestAddress,
  };
};

#if 0

struct Immediate : AddressMode

{

  Microcode ops[] = {fetchAddress8};
};

struct Absolute : AddressMode

{

  Microcode ops[] = {fetchAddress16};
#endif

}  // namespace cpu6502
