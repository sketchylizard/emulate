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
  template<Common::Byte State::* reg>
  static MicrocodeResponse addZeroPageIndex(State& cpu, Common::BusResponse response)
  // Stores the incoming low byte/only byte of a zero page address adjusts the low byte by the given
  // index register. Overflow is ignored for zero page indexing, it just wraps around onto the zero
  // page. However, because the 6502 could not add the index in one cycle, it will do one read from
  // the unmodified zero page address, then a second read from the indexed address. This function
  // makes the first read request.
  {
    // The incoming data is the low byte (only byte) of a zero page address.
    cpu.lo = response.data;
    // For zero page addressing modes, the hi byte is always 0
    cpu.hi = 0;

    // Zero page indexing reads from the address before indexing
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    // Wrap around within zero page
    cpu.lo += (cpu.*reg);

    return {BusRequest::Read(effectiveAddr)};
  }

  template<Common::Byte State::* reg>
  static MicrocodeResponse addIndex16(State& cpu, Common::BusResponse response)
  // Stores the incoming hi byte into the operand and adjusts the low byte by the given index register.
  // If there is overflow, it sets up the next action to fix the page boundary crossing. Either way,
  // it will make a request to read from the effective address (it just might be the wrong one).
  {
    // The incoming data is the high byte of a 16-bit address.
    cpu.hi = response.data;

    auto temp = static_cast<uint16_t>(cpu.lo);
    temp += (cpu.*reg);
    cpu.lo = static_cast<Common::Byte>(temp & 0xFF);
    bool carry = (temp & 0xFF00) != 0;

    // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
    // it will be wrong and we'll schedule an extra step to request the right one.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    // Check for page boundary crossing
    if (carry)
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
  static constexpr std::span<const Microcode> ops = {};
  static constexpr DisassemblyFormat format;
};

struct Accumulator : AddressMode
{
  static constexpr std::span<const Microcode> ops = {};
  static constexpr DisassemblyFormat format;
};

struct Relative : AddressMode
{
  static constexpr std::span<const Microcode> ops = {};
  static constexpr DisassemblyFormat format{"$", "", 1 /* e.g. "$44" */};
};

struct Immediate : AddressMode
{
  static constexpr const Microcode ops[] = {
      &requestAddress8,
  };

  static constexpr DisassemblyFormat format{"#$", "", 1 /* e.g. "#$44" */};
};

struct ZeroPage : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::requestOperand<&State::lo>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", "", 1};  // e.g. "$44"
};

struct ZeroPageX : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::addZeroPageIndex<&State::x>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", ",X", 1};  // e.g. "$44,X"
};

struct ZeroPageY : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // 1 cycles to fetch 8-bit address
      &AddressMode::addZeroPageIndex<&State::y>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", ",Y", 1};  // e.g. "$44,Y"
};

struct Absolute : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::requestOperand<&State::hi>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", "", 2};  // e.g. "$4400"
};

struct AbsoluteX : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::addIndex16<&State::x>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", ",X", 2};  // e.g. "$4400,X"
};

struct AbsoluteY : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
      &AddressMode::addIndex16<&State::y>,
      &AddressMode::requestOperand<&State::operand>,  // Read from effective address
  };
  static constexpr DisassemblyFormat format{"$", ",Y", 2};  // e.g. "$4400,Y"
};

struct AbsoluteJmp : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
  };
  static constexpr DisassemblyFormat format{"$", "", 2};  // e.g. "$4400"
};

struct AbsoluteIndirectJmp : AddressMode
{
  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress16,  // 2 cycles to fetch 16-bit address
  };
  static constexpr DisassemblyFormat format{"($", ")", 2};  // e.g. "($4400)"
};

struct IndirectZeroPageX : AddressMode
{
  static MicrocodeResponse requestIndirectLow(State& state, BusResponse response);
  static MicrocodeResponse requestIndirectHigh(State& state, BusResponse response);

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // Fetch base address $nn
      &AddressMode::addZeroPageIndex<&State::x>,  // Add X → pointer address
      &IndirectZeroPageX::requestIndirectLow,  // Read low byte from pointer
      &IndirectZeroPageX::requestIndirectHigh,  // Read high byte from pointer+1
      &AddressMode::requestOperand<&State::hi>,  // Store high byte and read from effective address
  };
  static constexpr DisassemblyFormat format{"($", ",X)", 1};  // e.g. "($44,X)"
};

struct IndirectZeroPageY : AddressMode
{
  static MicrocodeResponse requestIndirectLow(State& state, BusResponse response);

  static constexpr const Microcode ops[] = {
      &AddressMode::requestAddress8,  // Fetch base address $nn
      &IndirectZeroPageY::requestIndirectLow,  // Read low byte from pointer
      &IndirectZeroPageX::requestIndirectHigh,  // Read high byte from pointer+1
      &AddressMode::addIndex16<&State::y>,  // Add Y → pointer address
  };
  static constexpr DisassemblyFormat format{"($", ",)Y", 1};  // e.g. "($44),Y"
};

}  // namespace cpu6502
