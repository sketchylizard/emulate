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

  struct FixupPageCrossing;
  struct WrapZeroPage;

  // Define some microcode functions for addressing modes

  // Makes a request to read the low byte of an address from the PC and increments the PC.
  static MicrocodeResponse requestAddress8(State& cpu, Common::BusResponse response);

  // Makes requests to read a full 16-bit address (low byte first, then high byte) from the PC,
  // incrementing the PC after each read. Requires 2 cycles.
  // After this function completes, cpu.lo will contain the low byte of the address, and
  // the next bus response will contain the high byte of the address.
  static MicrocodeResponse requestAddress16(State& state, BusResponse response);

  // Makes a request to read the value at the effective address. Depending on which address mode is
  // executing, the incoming byte could be a zero page address (readTarget == &state.lo), or the the
  // high byte of a 16-bit address (lo byte alread in state.lo and readTarget == &state.hi).
  // Alternatively, it could be a spurious read, for example, while indexing an absolute address
  // (readTarget == &state.operand).
  template<Common::Byte State::* readTarget>
  static MicrocodeResponse requestOperand(State& state, BusResponse response)
  {
    (state.*readTarget) = response.data;

    auto effectiveAddr = Common::MakeAddress(state.lo, state.hi);
    return {BusRequest::Read(effectiveAddr)};
  }

protected:
  template<Common::Byte State::* reg, typename PageCrossingPolicy>
  static MicrocodeResponse preIndex(State& cpu, Common::BusResponse response)
  // Stores the incoming hi byte into the operand and adjusts the low byte by the given index register.
  // If there is overflow, it sets up the next action to fix the page boundary crossing. Either way,
  // it will make a request to read from the effective address (it just might be the wrong one).
  {
    bool carry = false;
    Common::Address effectiveAddr{0};

    if constexpr (std::is_same_v<PageCrossingPolicy, WrapZeroPage>)
    {
      // The incoming data is the low byte (only byte) of a zero page address.
      cpu.lo = response.data;
      // For zero page addressing modes, the hi byte is always 0
      cpu.hi = 0;

      // Zero page indexing reads from the address before indexing
      effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

      // Wrap around within zero page
      cpu.lo += (cpu.*reg);
    }
    else
    {
      // The incoming data is the high byte of a 16-bit address.
      cpu.hi = response.data;

      auto temp = static_cast<uint16_t>(cpu.lo);
      temp += (cpu.*reg);
      cpu.lo = static_cast<Common::Byte>(temp & 0xFF);
      carry = (temp & 0xFF00) != 0;

      // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
      // it will be wrong and we'll schedule an extra step to request the right one.
      effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);
    }
    assert(effectiveAddr != Common::Address{0});

    if constexpr (std::is_same_v<PageCrossingPolicy, FixupPageCrossing>)
    {
      // Check for page boundary crossing
      if (carry)
      {
        // We've already calculated the incorrect address so we can go ahead and fix up the hi byte
        // for the next read.
        ++cpu.hi;

        // Read wrong address first
        return {BusRequest::Read(effectiveAddr), &fixupPageCrossing};
      }
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

struct ZeroPage : AddressMode
{
  static constexpr auto type = State::AddressModeType::ZeroPage;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::requestOperand<&State::lo>,  // Read from effective address
  };
};

struct ZeroPageX : AddressMode
{
  static constexpr auto type = State::AddressModeType::ZeroPageX;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::preIndex<&State::x, WrapZeroPage>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
};

struct ZeroPageY : AddressMode
{
  static constexpr auto type = State::AddressModeType::ZeroPageY;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::preIndex<&State::y, WrapZeroPage>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
};

struct Absolute : AddressMode
{
  static constexpr auto type = State::AddressModeType::Absolute;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::requestOperand<&State::hi>,  // Read from effective address
  };
};

struct AbsoluteX : AddressMode
{
  static constexpr auto type = State::AddressModeType::AbsoluteX;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::preIndex<&State::x, FixupPageCrossing>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
};

struct AbsoluteY : AddressMode
{
  static constexpr auto type = State::AddressModeType::AbsoluteY;

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::preIndex<&State::y, FixupPageCrossing>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
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

}  // namespace cpu6502
