#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>

#include "common/bus.h"
#include "common/microcode.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/state.h"

namespace cpu6502
{

// Some instructions want to read the operand first, others won't the effective address only
// (e.g. STA). We can use this concept to indicate which behavior is desired.
template<typename F>
concept NeedsOperand = std::is_invocable_r_v<Generic6502Definition::Response, F, State&, Common::Byte>;

template<typename T>
concept IsWriteOperation = requires {
  { T::isWrite } -> std::convertible_to<bool>;
  requires T::isWrite == true;
};

struct AddressMode
{
  using MicrocodeResponse = Generic6502Definition::Response;
  using BusToken = Generic6502Definition::BusToken;
  using Address = Generic6502Definition::Address;
  using Byte = Common::Byte;
  using State = Generic6502Definition::State;
};

template<typename Derived>
struct Implied : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    return Derived::step0(cpu, bus.read(cpu.pc));
  }
  static constexpr Generic6502Definition::DisassemblyFormat format;
};

template<typename Derived>
struct Accumulator : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    return Derived::step0(cpu, bus.read(cpu.pc));
  }
  static constexpr Generic6502Definition::DisassemblyFormat format;
};

template<typename Derived>
struct Immediate : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    // Read the immediate operand from the instruction
    return Derived::step0(cpu, bus.read(cpu.pc++));
  }
  static constexpr Generic6502Definition::DisassemblyFormat format{"#$", "", 1 /* e.g. "#$44" */};
};

template<typename Derived>
struct Relative : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    // Read the signed 8-bit offset from the instruction
    return Derived::step0(cpu, bus.read(cpu.pc++));
  }
  static constexpr Generic6502Definition::DisassemblyFormat format{"$", "", 1 /* e.g. "$44" */};
};

template<typename Derived, Common::Byte VisibleState::* reg>
struct ZeroPageBase : AddressMode
{
  // 3 cycles: fetch 8-bit address, add X to it (wrap in zero page), read from zero page
  // Effective address is $00XX where XX is the fetched byte + X (with wrap)
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    cpu.hi = 0;
    if constexpr (reg != nullptr)
      return {addZeroPageIndex};

    return {finalize};
  }

  static MicrocodeResponse addZeroPageIndex(State& cpu, BusToken bus)
  {
    // Adds the given register to the lo register of the effective address. Overflow is ignored for
    // zero page indexing, it just wraps around onto the zero page. However, because the 6502 could
    // not add the index in one cycle, it will do one read from the unmodified zero page address, then
    // a second read from the indexed address.

    // Read unmodified address
    cpu.operand = bus.read(Common::MakeAddress(cpu.lo, 0x00));

    // Wrap around within zero page
    cpu.lo += (cpu.*reg);

    return {finalize};
  }

  static MicrocodeResponse finalize(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, 0x00);
    if constexpr (NeedsOperand<decltype(Derived::step0)>)
      return Derived::step0(cpu, bus.read(effectiveAddr));
    else
      return Derived::step0(cpu, bus, effectiveAddr);
  }

#if 0
  static Generic6502Definition::DisassemblyFormat format{"$",
      (reg == &State::x    ? ",X" :
          reg == &State::y ? ",Y" :
                             ""),
      1};  // e.g. "$44,X"
#endif
};

template<typename Derived>
using ZeroPage = ZeroPageBase<Derived, nullptr>;

template<typename Derived>
using ZeroPageX = ZeroPageBase<Derived, &State::x>;

template<typename Derived>
using ZeroPageY = ZeroPageBase<Derived, &State::y>;

template<typename Derived>
struct Absolute : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    return {readHighByte};
  }
#if 0

  static constexpr Generic6502Definition::DisassemblyFormat format{"$",
      (reg == &State::x    ? ",X" :
          reg == &State::y ? ",Y" :
                             ""),
      2 /* e.g. "$4400" */};
#endif

protected:
  static MicrocodeResponse readHighByte(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(cpu.pc++);

    return {finalize};
  }

  static MicrocodeResponse finalize(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    if constexpr (NeedsOperand<decltype(Derived::step0)>)
      return Derived::step0(cpu, bus.read(effectiveAddr));
    else
      return Derived::step0(cpu, bus, effectiveAddr);
  }
};

template<typename Derived, Common::Byte VisibleState::* reg>
struct AbsoluteIndex : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    return {readHighByte};
  }

  static MicrocodeResponse readHighByte(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(cpu.pc++);

    return {&addIndex};
  }

  static MicrocodeResponse addIndex(State& cpu, BusToken bus)
  {
    // Adjusts the low byte by the given index register. If there is overflow, it sets up the next
    // action to fix the page boundary crossing. Either way, it will make a request to read from the
    // effective address (it just might be the wrong one).
    auto temp = static_cast<uint16_t>(cpu.lo);
    temp += (cpu.*reg);
    cpu.lo = static_cast<Byte>(temp & 0xFF);
    bool carry = (temp & 0xFF00) != 0;

    // This is the address that will be requested this cycle. If we overflowed (HiByte(temp) != 0),
    // it will be wrong and we'll schedule an extra step to request the right one.
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);
    cpu.operand = bus.read(effectiveAddr);

    // Check for page boundary crossing
    if (carry)
    {
      // We've already calculated the incorrect address so we can go ahead and fix up the hi byte
      // for the next read.
      ++cpu.hi;

      // Schedule an extra read cycle to get the correct address
      return {pageCrossingPenalty};
    }

    if constexpr (IsWriteOperation<Derived>)
    {
      // For write operations, we need to write the original value back first
      return {pageCrossingPenalty};
    }

    if constexpr (NeedsOperand<decltype(Derived::step0)>)
    {
      // This is a read operation, so it only takes 4 steps if there was no page crossing
      return Derived::step0(cpu, cpu.operand);
    }
    else
    {
      return {finalize};
    }
  }

  static MicrocodeResponse pageCrossingPenalty(State& /*cpu*/, BusToken /*bus*/)
  {
    return {finalize};
  }

  static MicrocodeResponse finalize(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    if constexpr (NeedsOperand<decltype(Derived::step0)>)
    {
      return Derived::step0(cpu, bus.read(effectiveAddr));
    }
    else
    {
      return Derived::step0(cpu, bus, effectiveAddr);
    }
  }
};

template<typename Derived>
using AbsoluteX = AbsoluteIndex<Derived, &State::x>;

template<typename Derived>
using AbsoluteY = AbsoluteIndex<Derived, &State::y>;

struct AbsoluteJump : AddressMode
{
  // 3 cycles: fetch 16-bit address, jump to it
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    [[maybe_unused]] auto data = bus.read(cpu.pc++);
    return {};
  }

  static constexpr Generic6502Definition::DisassemblyFormat format{"$", "", 2 /* e.g. "$4400" */};
};

struct AbsoluteIndirectJump : AddressMode
{
  // 5 cycles: fetch 16-bit pointer address, read 16-bit target address from pointer, jump to it
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    return {};
  }

  static constexpr Generic6502Definition::DisassemblyFormat format{"($", ")", 2 /* e.g. "($4400)" */};
};

template<typename Derived>
struct IndirectZeroPageX : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    // Read the zero page address from the instruction
    cpu.lo = bus.read(cpu.pc++);
    cpu.hi = 0;
    return {spuriousRead};
  }

#if 0
  static constexpr Generic6502Definition::DisassemblyFormat{"($", (reg == &State::x ? ",X)" : "),Y"), 1};
#endif

private:
  static MicrocodeResponse spuriousRead(State& cpu, BusToken bus)
  {
    cpu.operand = bus.read(Common::MakeAddress(cpu.lo, 0x00));
    return {addZeroPageIndex};
  }

  static MicrocodeResponse addZeroPageIndex(State& cpu, BusToken bus)
  {
    // Wrap around within zero page
    cpu.lo += cpu.x;

    cpu.operand = bus.read(Common::MakeAddress(cpu.lo++, 0x00));
    return {readHiByteFromZeroPage};
  }

  static MicrocodeResponse readLoByteFromZeroPage(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, 0x00);
    // we can't replace cpu.lo yet.
    cpu.operand = bus.read(effectiveAddr);
    return {readHiByteFromZeroPage};
  }

  static MicrocodeResponse readHiByteFromZeroPage(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, 0x00);
    cpu.lo = cpu.operand;
    cpu.hi = bus.read(effectiveAddr);

    return {readEffectiveAddress};
  }

  static MicrocodeResponse readEffectiveAddress(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    if constexpr (NeedsOperand<decltype(Derived::step0)>)
      return {Derived::step0(cpu, bus.read(effectiveAddr))};
    else
      return Derived::step0(cpu, bus, effectiveAddr);
  }
};

template<typename Derived>
struct IndirectZeroPageY : AddressMode
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    // Read the zero page address from the instruction
    cpu.lo = bus.read(cpu.pc++);
    cpu.hi = 0;
    return {readLoByteFromZeroPage};
  }

#if 0
  static constexpr Generic6502Definition::DisassemblyFormat{"($", (reg == &State::x ? ",X)" : "),Y"), 1};
#endif

private:
  static MicrocodeResponse readLoByteFromZeroPage(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo, 0x00);
    // we can't replace cpu.lo yet.
    cpu.operand = bus.read(effectiveAddr);
    return {readHiByteFromZeroPage};
  }

  static MicrocodeResponse readHiByteFromZeroPage(State& cpu, BusToken bus)
  {
    auto effectiveAddr = Common::MakeAddress(cpu.lo + 1, 0x00);
    cpu.lo = cpu.operand;
    cpu.hi = bus.read(effectiveAddr);

    return {add16BitIndex};
  }

  static MicrocodeResponse add16BitIndex(State& cpu, BusToken bus)
  {
    // Wrap around within zero page
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi) + cpu.y;

    cpu.lo = Common::LoByte(effectiveAddr);

    bool carry = (Common::HiByte(effectiveAddr) != cpu.hi);
    if (carry)
    {
      // Page boundary crossed, we need an extra read cycle
      effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);
    }

    // We always read from the calculated effective address (it could be wrong if we crossed a page
    // boundary.)
    cpu.operand = bus.read(effectiveAddr);

    // Read operations without carry are done now, pass the operand to the derived class
    if constexpr (!IsWriteOperation<Derived>)
    {
      if (!carry)
      {
        return Derived::step0(cpu, cpu.operand);
      }
    }

    // If there was a carry, go ahead and fix up the hi byte.
    if (carry)
    {
      ++cpu.hi;
    }

    return {fixupHighByte};
  }

  static MicrocodeResponse fixupHighByte(State& cpu, BusToken bus)
  {
    // The high byte has already been fixed up if necessary, just read from the correct address now
    auto effectiveAddr = Common::MakeAddress(cpu.lo, cpu.hi);

    if constexpr (NeedsOperand<decltype(Derived::step0)>)
      return {Derived::step0(cpu, bus.read(effectiveAddr))};
    else
      return Derived::step0(cpu, bus, effectiveAddr);
  }
};

}  // namespace cpu6502
