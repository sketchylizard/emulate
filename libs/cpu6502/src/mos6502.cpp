#include "cpu6502/mos6502.h"

#include "common/address.h"
#include "common/bus.h"
#include "common/fixed_formatter.h"
#include "cpu6502/address_mode.h"
#include "cpu6502/state.h"

using namespace Common;

namespace cpu6502
{

using MicrocodeResponse = Generic6502Definition::Response;
using BusToken = Generic6502Definition::BusToken;
using Address = Generic6502Definition::Address;
using State = Generic6502Definition::State;

////////////////////////////////////////////////////////////////////////////////
// ReadModifyWrite Operation
////////////////////////////////////////////////////////////////////////////////
template<auto Operation>
struct ReadModifyWrite
{
  static constexpr bool isWrite = true;

  // Step 1: Read from memory, write unmodified value back (6502 quirk)
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    cpu.operand = operand;  // Store original value for step1
    return {spuriousWrite};
  }

  static MicrocodeResponse spuriousWrite(State& cpu, BusToken bus)
  {
    bus.write(Common::MakeAddress(cpu.lo, cpu.hi), cpu.operand);
    return {step1};
  }

  static MicrocodeResponse step1(State& cpu, BusToken bus)
  {
    cpu.operand = Operation(cpu, cpu.operand);

    // Reconstruct effective address and write modified value
    // Note: This assumes addressing mode left address info in reconstructible form
    bus.write(Common::MakeAddress(cpu.lo, cpu.hi), cpu.operand);

    return {};
  }
};

struct nop
{
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    // No operation; used to consume a cycle
    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// BRK - Software Interrupt (7 cycles)
////////////////////////////////////////////////////////////////////////////////

struct brk
{
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    return {pushHighPC};
  }

  // Step 1: Push return address high byte (PC)
  static MicrocodeResponse pushHighPC(State& cpu, BusToken bus)
  {
    // BRK pushes PC (current instruction + 2), unlike JSR which pushes PC-1
    Byte return_addr_high = Common::HiByte(++cpu.pc);
    bus.write(Common::MakeAddress(cpu.sp--, 0x01), return_addr_high);
    return {pushLowPC};
  }

  // Step 2: Push return address low byte
  static MicrocodeResponse pushLowPC(State& cpu, BusToken bus)
  {
    Byte return_addr_low = Common::LoByte(static_cast<uint16_t>(cpu.pc));
    bus.write(Common::MakeAddress(cpu.sp--, 0x01), return_addr_low);
    return {pushProcessorStatus};
  }

  // Step 3: Push processor status with Break flag set
  static MicrocodeResponse pushProcessorStatus(State& cpu, BusToken bus)
  {
    // Push P with Break flag set (like PHP)
    Byte status = cpu.p | static_cast<Byte>(State::Flag::Break);
    bus.write(Common::MakeAddress(cpu.sp--, 0x01), status);
    return {readIRQVectorLow};
  }

  // Step 4: Read IRQ vector low byte, set Interrupt flag
  static MicrocodeResponse readIRQVectorLow(State& cpu, BusToken bus)
  {
    // Set Interrupt flag to disable further interrupts
    cpu.set(State::Flag::Interrupt, true);

    // Read IRQ vector low byte from $FFFE
    cpu.lo = bus.read(0xFFFE_addr);

    return {readIRQVectorHigh};
  }

  // Step 5: Read IRQ vector high byte and jump
  static MicrocodeResponse readIRQVectorHigh(State& cpu, BusToken bus)
  {
    // Read IRQ vector high byte from $FFFF
    cpu.hi = bus.read(0xFFFF_addr);
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// Flag operations (CLC, SEC, CLI, SEI, CLD, SED, CLV)
////////////////////////////////////////////////////////////////////////////////
template<State::Flag Flag, bool Set>
struct FlagOp
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    cpu.set(Flag, Set);
    return {};
  }
};

using CLC = FlagOp<State::Flag::Carry, false>;
using SEC = FlagOp<State::Flag::Carry, true>;
using CLI = FlagOp<State::Flag::Interrupt, false>;
using SEI = FlagOp<State::Flag::Interrupt, true>;
using CLV = FlagOp<State::Flag::Overflow, false>;
using CLD = FlagOp<State::Flag::Decimal, false>;
using SED = FlagOp<State::Flag::Decimal, true>;

////////////////////////////////////////////////////////////////////////////////
// Increment operations (INX, INY)

template<Common::Byte VisibleState::* reg>
  requires(reg != &State::a)
struct Increment
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // Handle increment operation for X or Y registers
    auto& r = (cpu.*reg);
    ++r;
    cpu.setZN(r);
    return {};
  }
};

using INX = Increment<&State::x>;
using INY = Increment<&State::y>;

template<Common::Byte VisibleState::* reg>
  requires(reg != &State::a)
struct Decrement
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // Handle decrement operation for X or Y registers
    auto& r = (cpu.*reg);
    --r;
    cpu.setZN(r);
    return {};
  }
};

using DEX = Decrement<&State::x>;
using DEY = Decrement<&State::y>;

////////////////////////////////////////////////////////////////////////////////
// Push/Pull operations, PLA, PLA, PHP, PLP
////////////////////////////////////////////////////////////////////////////////

template<Common::Byte VisibleState::* SourceReg, bool SetBreakFlag = false>
struct PushOp
{
  // Step 0: Dummy read to consume cycle
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    return {step1};
  }

  // Step 2: Write register value to stack
  static MicrocodeResponse step1(State& cpu, BusToken bus)
  {
    Byte data = (cpu.*SourceReg);

    // Apply Break flag modification for PHP
    if constexpr (SetBreakFlag)
    {
      data |= static_cast<Byte>(State::Flag::Break);
    }

    bus.write(Common::MakeAddress(cpu.sp--, 0x01), data);
    return {};
  }
};

template<Common::Byte VisibleState::* TargetReg>
struct PullOp
{
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    return {step2};
  }

  static MicrocodeResponse step2(State& cpu, BusToken bus)
  {
    cpu.operand = bus.read(Common::MakeAddress(cpu.sp++, 0x01));
    return {step3};
  }

  // Step 3: Store data and set flags if needed
  static MicrocodeResponse step3(State& cpu, BusToken bus)
  {
    Common::Byte data = bus.read(Common::MakeAddress(cpu.sp, 0x01));

    // Apply Break flag clearing for PLP
    if constexpr (TargetReg == &State::p)
    {
      data &= ~static_cast<Byte>(State::Flag::Break);
      cpu.assignP(data);  // Use assignP to ensure U flag is set
    }
    else
    {
      // Update N/Z flags for PLA
      // Store in target register
      (cpu.*TargetReg) = data;
      cpu.setZN(data);
    }

    return {};
  }
};

using PLA = PullOp<&State::a>;
using PLP = PullOp<&State::p>;
using PHA = PushOp<&State::a, false>;
using PHP = PushOp<&State::p, true>;

////////////////////////////////////////////////////////////////////////////////
// RTI - Return from Interrupt (6 cycles)
////////////////////////////////////////////////////////////////////////////////

struct Rti
{
  // Step 0: Dummy read from current PC
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    return {step2};
  }

  static MicrocodeResponse step2(State& cpu, BusToken bus)
  {
    [[maybe_unused]] auto byte = bus.read(Common::MakeAddress(cpu.sp++, 0x01));
    return {popProcessorStatus};
  }

  static MicrocodeResponse popProcessorStatus(State& cpu, BusToken bus)
  {
    // Restore processor status (clear Break flag like PLP)
    Byte status = bus.read(Common::MakeAddress(cpu.sp++, 0x01)) & ~static_cast<Byte>(State::Flag::Break);
    cpu.assignP(status);  // Use assignP to preserve Unused flag

    return {popReturnAddressLow};
  }

  static MicrocodeResponse popReturnAddressLow(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(Common::MakeAddress(cpu.sp++, 0x01));
    return {popReturnAddressHigh};
  }

  static MicrocodeResponse popReturnAddressHigh(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(Common::MakeAddress(cpu.sp, 0x01));
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);

    // Note: Unlike RTS, RTI does NOT increment PC
    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// RTS - Return from Subroutine (6 cycles)
////////////////////////////////////////////////////////////////////////////////

struct Rts
{
  static MicrocodeResponse step0(State& /*cpu*/, Common::Byte /*operand*/)
  {
    return {step2};
  }

  static MicrocodeResponse step2(State& cpu, BusToken bus)
  {
    [[maybe_unused]] auto byte = bus.read(Common::MakeAddress(cpu.sp++, 0x01));
    return {popReturnAddressLow};
  }

  static MicrocodeResponse popReturnAddressLow(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(Common::MakeAddress(cpu.sp++, 0x01));

    return {popReturnAddressHigh};
  }

  static MicrocodeResponse popReturnAddressHigh(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(Common::MakeAddress(cpu.sp, 0x01));
    return {jumpToReturnAddress};
  }

  static MicrocodeResponse jumpToReturnAddress(State& cpu, BusToken bus)
  {
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);

    bus.read(cpu.pc++);

    // Note: Unlike RTS, RTI does NOT increment PC
    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// Transfer operations (TAX, TAY, TXA, TYA, TSX, TXS)
////////////////////////////////////////////////////////////////////////////////
template<Common::Byte VisibleState::* src, Common::Byte VisibleState::* dst>
  requires(src != dst)
struct Transfer
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    (cpu.*dst) = (cpu.*src);

    if constexpr (dst != &State::sp)
    {
      // Only affect flags if not transferring to stack pointer
      cpu.setZN((cpu.*dst));
    }
    return {};
  }
};

using TYA = Transfer<&State::y, &State::a>;
using TAY = Transfer<&State::a, &State::y>;
using TXA = Transfer<&State::x, &State::a>;
using TAX = Transfer<&State::a, &State::x>;
using TXS = Transfer<&State::x, &State::sp>;
using TSX = Transfer<&State::sp, &State::x>;

////////////////////////////////////////////////////////////////////////////////
// Shift/Rotate Instructions - Accumulator Mode (2 cycles)
////////////////////////////////////////////////////////////////////////////////

// ASL A - Arithmetic Shift Left Accumulator
struct ShiftLeftAccumulator
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // C ← [7][6][5][4][3][2][1][0] ← 0
    bool bit7 = (cpu.a & 0x80) != 0;
    cpu.a <<= 1;  // Shift left, bit 0 becomes 0

    cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags

    return {};
  }
};

// LSR A - Logical Shift Right Accumulator
struct ShiftRightAccumulator
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // 0 → [7][6][5][4][3][2][1][0] → C
    bool bit0 = (cpu.a & 0x01) != 0;
    cpu.a >>= 1;  // Shift right, bit 7 becomes 0

    cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags (N will always be 0)

    return {};
  }
};

// ROL A - Rotate Left Accumulator
struct RotateLeftAccumulator
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // C ← [7][6][5][4][3][2][1][0] ← C
    bool bit7 = (cpu.a & 0x80) != 0;
    bool old_carry = cpu.has(State::Flag::Carry);

    cpu.a <<= 1;  // Shift left
    if (old_carry)
    {
      cpu.a |= 0x01;  // Carry → Bit 0
    }

    cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags

    return {};
  }
};

// ROR A - Rotate Right Accumulator
struct RotateRightAccumulator
{
  static MicrocodeResponse step0(State& cpu, Common::Byte /*operand*/)
  {
    // C → [7][6][5][4][3][2][1][0] → C
    bool bit0 = (cpu.a & 0x01) != 0;
    bool old_carry = cpu.has(State::Flag::Carry);

    cpu.a >>= 1;  // Shift right
    if (old_carry)
    {
      cpu.a |= 0x80;  // Carry → Bit 7
    }

    cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags

    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// Shift/Rotate Instructions - Memory Mode (Read-Modify-Write)
////////////////////////////////////////////////////////////////////////////////

using ShiftLeft = ReadModifyWrite<[](State& cpu, Common::Byte value)
    {
      // C ← [7][6][5][4][3][2][1][0] ← 0
      bool bit7 = (value & 0x80) != 0;
      value <<= 1;  // Shift left, bit 0 becomes 0

      cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
      cpu.setZN(value);  // Set N and Z flags

      return value;
    }>;

using ShiftRight = ReadModifyWrite<[](State& cpu, Common::Byte value)
    {
      // 0 → [7][6][5][4][3][2][1][0] → C
      bool bit0 = (value & 0x01) != 0;
      value >>= 1;  // Shift right, bit 7 becomes 0

      cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
      cpu.setZN(value);  // Set N and Z flags (N will always be 0)

      return value;
    }>;

using RotateLeft = ReadModifyWrite<[](State& cpu, Common::Byte value)
    {
      // C ← [7][6][5][4][3][2][1][0] ← C
      bool bit7 = (value & 0x80) != 0;
      bool old_carry = cpu.has(State::Flag::Carry);

      value <<= 1;  // Shift left
      if (old_carry)
      {
        value |= 0x01;  // Carry → Bit 0
      }

      cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
      cpu.setZN(value);  // Set N and Z flags

      return value;
    }>;

using RotateRight = ReadModifyWrite<[](State& cpu, Common::Byte value)
    {
      // C → [7][6][5][4][3][2][1][0] → C
      bool bit0 = (value & 0x01) != 0;
      bool old_carry = cpu.has(State::Flag::Carry);

      value >>= 1;  // Shift right
      if (old_carry)
      {
        value |= 0x80;  // Carry → Bit 7
      }

      cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
      cpu.setZN(value);  // Set N and Z flags

      return value;
    }>;

////////////////////////////////////////////////////////////////////////////////
// Arithmetic and Logic Instructions
////////////////////////////////////////////////////////////////////////////////
struct Add
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    assert(cpu.has(State::Flag::Decimal) == false);  // BCD mode not supported

    const Byte a = cpu.a;
    const Byte m = operand;
    const Byte c = cpu.has(State::Flag::Carry) ? 1 : 0;

    const uint16_t sum = uint16_t(a) + uint16_t(m) + uint16_t(c);
    const Byte result = Byte(sum & 0xFF);

    cpu.set(State::Flag::Carry, (sum & 0x100) != 0);
    cpu.set(State::Flag::Overflow, ((~(a ^ m) & (a ^ result)) & 0x80) != 0);
    cpu.setZN(result);

    cpu.a = result;
    return {};
  }
};

struct Subtract
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    // SBC: A = A - M - (1 - C) = A + (~M) + C
    operand = ~operand;  // Invert the operand for two's complement
    Common::Byte carry = cpu.has(State::Flag::Carry) ? 1 : 0;

    uint16_t temp = static_cast<uint16_t>(cpu.a) + operand + carry;

    // Set carry flag (no borrow occurred if bit 8 is set)
    cpu.set(State::Flag::Carry, (temp & 0x100) != 0);

    // Check for signed overflow
    auto result = static_cast<Common::Byte>(temp & 0xFF);
    bool overflow = ((cpu.a ^ result) & (operand ^ result) & 0x80) != 0;
    cpu.set(State::Flag::Overflow, overflow);

    // Store result and set N,Z flags
    cpu.a = result;
    cpu.setZN(cpu.a);

    return {};
  }
};

template<Common::Byte VisibleState::* reg>
struct Compare
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    auto r = (cpu.*reg);
    uint16_t diff = uint16_t(r) - uint16_t(operand);  // Unsigned arithmetic
    bool borrow = (diff & 0x100) != 0;

    cpu.set(State::Flag::Carry, !borrow);
    cpu.setZN(static_cast<Common::Byte>(diff & 0xFF));
    return {};
  }
};

using CMP = Compare<&State::a>;
using CPX = Compare<&State::x>;
using CPY = Compare<&State::y>;

struct And
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    cpu.a &= operand;  // A ← A ∧ M
    cpu.setZN(cpu.a);  // Set N and Z flags based on result
    return {};
  }
};

struct Eor
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    cpu.a ^= operand;  // A ← A ⊕ M
    cpu.setZN(cpu.a);
    return {};
  }
};

struct Ora
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    // Perform OR with accumulator
    cpu.a |= operand;
    cpu.set(State::Flag::Zero, cpu.a == 0);  // Set zero flag
    cpu.set(State::Flag::Negative, cpu.a & 0x80);  // Set negative flag
    return {};
  }
};

template<Common::Byte VisibleState::* reg>
struct Load
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    // This is a generic load operation for A, X, or Y registers.

    auto data = (cpu.*reg) = operand;
    cpu.setZN(data);
    return {};
  }
};

using LDA = Load<&State::a>;
using LDX = Load<&State::x>;
using LDY = Load<&State::y>;

template<Common::Byte VisibleState::* reg>
struct Store
{
  static constexpr bool isWrite = true;

  static MicrocodeResponse step0(State& cpu, BusToken bus, Common::Address effectiveAddress)
  {
    // This is a generic store operation for A, X, or Y registers.
    bus.write(effectiveAddress, (cpu.*reg));

    return {};
  }
};

using STA = Store<&State::a>;
using STX = Store<&State::x>;
using STY = Store<&State::y>;

template<State::Flag flag, bool condition>
struct Branch
{

  // Evaluate condition and decide whether to branch
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    // The incoming operand is the relative branch offset.
    cpu.operand = operand;

    // Check if the flag matches the desired condition
    bool flagSet = cpu.has(flag);
    bool shouldBranch = (flagSet == condition);

    if (shouldBranch)
    {
      // Branch taken - continue to step 1
      return {branchTaken};
    }

    return {};
  }

  static MicrocodeResponse branchTaken(State& cpu, BusToken bus)
  {
    int8_t offset = static_cast<int8_t>(cpu.operand);

    if (offset == -2)
    {  // Self-branch detected
      Generic6502Definition::trap(cpu.pc - 2);
    }

    // Add offset to PC low byte
    auto tmp = static_cast<uint16_t>(cpu.pc) + offset;

    // We have three pieces of information:
    // - The low byte of the new PC (LoByte(tmp))
    // - The high byte of the new PC (HiByte(tmp))
    // - The high byte of the current PC (HiByte(cpu.pc))

    cpu.lo = static_cast<Common::Byte>(tmp & 0xFF);
    cpu.hi = HiByte(cpu.pc);
    cpu.operand = HiByte(static_cast<uint16_t>(tmp));

    // Read from the old PC address again
    [[maybe_unused]] auto data = bus.read(cpu.pc);

    // Detect if page boundary is crossed
    if (cpu.operand == cpu.hi)
    {
      // No page boundary crossed - branch complete
      cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
      return {};
    }

    // Page boundary crossed - need step 2 for fixup
    return {branchPageFixup};
  }

  static MicrocodeResponse branchPageFixup(State& cpu, BusToken bus)
  {
    // Read from the incorrect address
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);

    [[maybe_unused]] auto byte = bus.read(cpu.pc);

    cpu.hi = cpu.operand;
    cpu.pc = Common::MakeAddress(cpu.lo, static_cast<Common::Byte>(cpu.hi));

    // 4 cycles total, we're done
    return {};
  }
};

using BNE = Branch<State::Flag::Zero, false>;
using BEQ = Branch<State::Flag::Zero, true>;
using BPL = Branch<State::Flag::Negative, false>;
using BMI = Branch<State::Flag::Negative, true>;
using BCC = Branch<State::Flag::Carry, false>;
using BCS = Branch<State::Flag::Carry, true>;
using BVC = Branch<State::Flag::Overflow, false>;
using BVS = Branch<State::Flag::Overflow, true>;

using IncrementMemory = ReadModifyWrite<[](State& cpu, Common::Byte val)
    {
      ++val;
      cpu.setZN(val);
      return val;
    }>;

using DecrementMemory = ReadModifyWrite<[](State& cpu, Common::Byte val)
    {
      --val;
      cpu.setZN(val);
      return val;
    }>;

struct Bit
{
  static MicrocodeResponse step0(State& cpu, Common::Byte operand)
  {
    // BIT instruction: Test bits in memory with accumulator
    // - Z flag: Set if (A & M) == 0
    // - N flag: Copy bit 7 of memory operand
    // - V flag: Copy bit 6 of memory operand
    // - Accumulator is NOT modified
    // - C, I, D flags are not affected

    Byte memory_value = operand;
    Byte test_result = cpu.a & memory_value;

    // Set Zero flag based on AND result
    cpu.set(State::Flag::Zero, test_result == 0);

    // Copy bit 7 of memory to Negative flag
    cpu.set(State::Flag::Negative, (memory_value & 0x80) != 0);

    // Copy bit 6 of memory to Overflow flag
    cpu.set(State::Flag::Overflow, (memory_value & 0x40) != 0);

    // Note: Accumulator is unchanged!
    return {};
  }
};

// JSR - Jump to Subroutine (6 cycles)
struct JumpSubroutine
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    return {internal};
  }

  static MicrocodeResponse internal(State& cpu, BusToken bus)
  {
    cpu.operand = bus.read(Common::MakeAddress(cpu.sp, 0x01));
    return {pushHighPC};
  }

  static MicrocodeResponse pushHighPC(State& cpu, BusToken bus)
  {
    // Push return address high byte (PC-1)
    // JSR pushes PC-1 where PC currently points past the JSR instruction
    bus.write(Common::MakeAddress(cpu.sp--, 0x01), Common::HiByte(cpu.pc));

    return {pushLowPC};
  }

  static MicrocodeResponse pushLowPC(State& cpu, BusToken bus)
  {
    // Push return address low byte and jump
    bus.write(Common::MakeAddress(cpu.sp--, 0x01), Common::LoByte(cpu.pc));

    return {jump};
  }

  static MicrocodeResponse jump(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(cpu.pc++);
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
    return {};
  }
};

struct JumpAbsolute
{
  static MicrocodeResponse execute(State& cpu, BusToken bus)
  {
    cpu.lo = bus.read(cpu.pc++);
    return {readHighPC};
  }
  static MicrocodeResponse readHighPC(State& cpu, BusToken bus)
  {
    cpu.hi = bus.read(cpu.pc++);
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
    return {};
  }
};

struct JumpIndirect
{
  static MicrocodeResponse step0(State& cpu, BusToken bus, Common::Address effectiveAddress)
  {
    // cpu.lo and cpu.hi already hold the two bytes that followed the opcode, the
    // pointer address. We now need to load the byte at that address, then the byte at
    // the next address (with the 6502 page wrap bug).
    cpu.operand = bus.read(effectiveAddress);
    return {readDestinationHigh};
  }

  static MicrocodeResponse readDestinationHigh(State& cpu, BusToken bus)
  {
    // We read the low byte at the effective address, now we need to increment that address and read
    // the high byte, with the 6502 page wrap bug. The bug is that we only increment the low byte of
    // the address. If it wraps, we stay on the same page. I.e. if the pointer address is $xxFF, we read
    // the high byte from $xx00, not $xy00.
    Address ptr = Common::MakeAddress(++cpu.lo, cpu.hi);

    cpu.lo = cpu.operand;
    cpu.hi = bus.read(ptr);

    return {jump};
  }

  static MicrocodeResponse jump(State& cpu, BusToken /*bus*/)
  {
    Address target = Common::MakeAddress(cpu.lo, cpu.hi);
    // JMP (indirect) requires 3 bytes, so if we are jumping to the current instruction,
    // it is a self-jump.
    if (target == cpu.pc - 3)
    {  // Self-jump detected
      Generic6502Definition::trap(target);
    }
    cpu.pc = target;

    return {};
  }
};

////////////////////////////////////////////////////////////////////////////////
// CPU implementation
////////////////////////////////////////////////////////////////////////////////

namespace
{

using InstructionTable = std::array<Generic6502Definition::Instruction, 256>;

struct Builder
{
  InstructionTable& table;

  template<typename Cmd>
  constexpr Builder& add(Common::Byte opcode, std::string_view mnemonic)
  {
    Generic6502Definition::Instruction& instr = table[opcode];
    instr.opcode = opcode;
    std::ranges::copy(mnemonic, std::begin(instr.mnemonic));
    instr.op = Cmd::execute;
    return *this;
  }
};

}  // namespace

static constexpr auto c_instructions = []()
{
  InstructionTable table{};

  Builder builder{table};
  builder  //
      .add<Implied<nop>>(0xEA, "NOP")
      .add<Implied<brk>>(0x00, "BRK")

      // Flag operations
      .add<Implied<CLC>>(0x18, "CLC")
      .add<Implied<SEC>>(0x38, "SEC")
      .add<Implied<CLI>>(0x58, "CLI")
      .add<Implied<SEI>>(0x78, "SEI")
      .add<Implied<CLV>>(0xB8, "CLV")
      .add<Implied<CLD>>(0xD8, "CLD")
      .add<Implied<SED>>(0xF8, "SED")

      // Increment and Decrement instructions
      .add<Implied<INX>>(0xE8, "INX")
      .add<Implied<INY>>(0xC8, "INY")
      .add<Implied<DEX>>(0xCA, "DEX")
      .add<Implied<DEY>>(0x88, "DEY")

      // Stack operations
      .add<Implied<PLA>>(0x68, "PLA")
      .add<Implied<PHA>>(0x48, "PHA")
      .add<Implied<PLP>>(0x28, "PLP")
      .add<Implied<PHP>>(0x08, "PHP")

      // RTI/RTS
      .add<Implied<Rti>>(0x40, "RTI")
      .add<Implied<Rts>>(0x60, "RTS")

      // Transfer instructions
      .add<Implied<TYA>>(0x98, "TYA")
      .add<Implied<TAY>>(0xA8, "TAY")
      .add<Implied<TXA>>(0x8A, "TXA")
      .add<Implied<TAX>>(0xAA, "TAX")
      .add<Implied<TXS>>(0x9A, "TXS")
      .add<Implied<TSX>>(0xBA, "TSX")

      // Accumulator mode (2 cycles):
      .add<Accumulator<ShiftLeftAccumulator>>(0x0A, "ASL")
      .add<Accumulator<ShiftRightAccumulator>>(0x4A, "LSR")
      .add<Accumulator<RotateLeftAccumulator>>(0x2A, "ROL")
      .add<Accumulator<RotateRightAccumulator>>(0x6A, "ROR")

      // Memory modes (5-7 cycles):
      .add<ZeroPage<ShiftLeft>>(0x06, "ASL")  // ASL $nn
      .add<ZeroPageX<ShiftLeft>>(0x16, "ASL")  // ASL $nn,X
      .add<Absolute<ShiftLeft>>(0x0E, "ASL")  // ASL $nnnn
      .add<AbsoluteX<ShiftLeft>>(0x1E, "ASL")  // ASL $nnnn,X
      .add<ZeroPage<ShiftRight>>(0x46, "LSR")  // LSR $nn
      .add<ZeroPageX<ShiftRight>>(0x56, "LSR")  // LSR $nn,X
      .add<Absolute<ShiftRight>>(0x4E, "LSR")  // LSR $nnnn
      .add<AbsoluteX<ShiftRight>>(0x5E, "LSR")  // LSR $nnnn,X
      .add<ZeroPage<RotateLeft>>(0x26, "ROL")  // ROL $nn
      .add<ZeroPageX<RotateLeft>>(0x36, "ROL")  // ROL $nn,X
      .add<Absolute<RotateLeft>>(0x2E, "ROL")  // ROL $nnnn
      .add<AbsoluteX<RotateLeft>>(0x3E, "ROL")  // ROL $nnnn,X
      .add<ZeroPage<RotateRight>>(0x66, "ROR")  // ROR $nn
      .add<ZeroPageX<RotateRight>>(0x76, "ROR")  // ROR $nn,X
      .add<Absolute<RotateRight>>(0x6E, "ROR")  // ROR $nnnn
      .add<AbsoluteX<RotateRight>>(0x7E, "ROR")  // ROR $nnnn,X

      // ADC instructions
      .add<Immediate<Add>>(0x69, "ADC")
      //.add<ZeroPage, Add>(0x65, "ADC")
      //.add<ZeroPageX, Add>(0x75, "ADC")
      //.add<Absolute, Add>(0x6D, "ADC")
      //.add<AbsoluteX, Add>(0x7D, "ADC")
      //.add<AbsoluteY, Add>(0x79, "ADC")
      //.add<IndirectZeroPageX, Add>(0x61, "ADC")
      //.add<IndirectZeroPageY, Add>(0x71, "ADC")

      // AND instructions - all addressing modes
      .add<Immediate<And>>(0x29, "AND")  // AND #$nn
      .add<ZeroPage<And>>(0x25, "AND")  // AND $nn
      .add<ZeroPageX<And>>(0x35, "AND")  // AND $nn,X
      .add<Absolute<And>>(0x2D, "AND")  // AND $nnnn
      .add<AbsoluteX<And>>(0x3D, "AND")  // AND $nnnn,X
      .add<AbsoluteY<And>>(0x39, "AND")  // AND $nnnn,Y
      .add<IndirectZeroPageX<And>>(0x21, "AND")  // AND ($nn,X)
      .add<IndirectZeroPageY<And>>(0x31, "AND")  // AND ($nn),Y

      // CMP — Compare Accumulator
      .add<Immediate<CMP>>(0xC9, "CMP")
      .add<ZeroPage<CMP>>(0xC5, "CMP")
      .add<ZeroPageX<CMP>>(0xD5, "CMP")
      .add<Absolute<CMP>>(0xCD, "CMP")
      .add<AbsoluteX<CMP>>(0xDD, "CMP")
      .add<AbsoluteY<CMP>>(0xD9, "CMP")
      .add<IndirectZeroPageX<CMP>>(0xC1, "CMP")
      .add<IndirectZeroPageY<CMP>>(0xD1, "CMP")

      // CPX — Compare X Register
      .add<Immediate<CPX>>(0xE0, "CPX")
      .add<ZeroPage<CPX>>(0xE4, "CPX")
      .add<Absolute<CPX>>(0xEC, "CPX")

      // CPY — Compare Y Register
      .add<Immediate<CPY>>(0xC0, "CPY")
      .add<ZeroPage<CPY>>(0xC4, "CPY")
      .add<Absolute<CPY>>(0xCC, "CPY")

      // EOR instructions - all addressing modes (exclusive OR)
      .add<Immediate<Eor>>(0x49, "EOR")
      .add<ZeroPage<Eor>>(0x45, "EOR")
      .add<ZeroPageX<Eor>>(0x55, "EOR")
      .add<Absolute<Eor>>(0x4D, "EOR")
      .add<AbsoluteX<Eor>>(0x5D, "EOR")
      .add<AbsoluteY<Eor>>(0x59, "EOR")
      .add<IndirectZeroPageX<Eor>>(0x41, "EOR")
      .add<IndirectZeroPageY<Eor>>(0x51, "EOR")

      // LDA instructions
      .add<Immediate<LDA>>(0xA9, "LDA")
      .add<ZeroPage<LDA>>(0xA5, "LDA")
      .add<ZeroPageX<LDA>>(0xB5, "LDA")
      .add<Absolute<LDA>>(0xAD, "LDA")
      .add<AbsoluteX<LDA>>(0xBD, "LDA")
      .add<AbsoluteY<LDA>>(0xB9, "LDA")
      .add<IndirectZeroPageX<LDA>>(0xA1, "LDA")
      .add<IndirectZeroPageY<LDA>>(0xB1, "LDA")

      // LDX instructions
      .add<Immediate<LDX>>(0xA2, "LDX")
      .add<ZeroPage<LDX>>(0xA6, "LDX")
      .add<ZeroPageY<LDX>>(0xB6, "LDX")
      .add<Absolute<LDX>>(0xAE, "LDX")
      .add<AbsoluteY<LDX>>(0xBE, "LDX")

      // LDY instructions
      .add<Immediate<LDY>>(0xA0, "LDY")
      .add<ZeroPage<LDY>>(0xA4, "LDY")
      .add<ZeroPageX<LDY>>(0xB4, "LDY")
      .add<Absolute<LDY>>(0xAC, "LDY")
      .add<AbsoluteX<LDY>>(0xBC, "LDY")

      // STA variations
      .add<ZeroPage<STA>>(0x85, "STA")
      .add<ZeroPageX<STA>>(0x95, "STA")
      .add<Absolute<STA>>(0x8D, "STA")
      .add<AbsoluteX<STA>>(0x9D, "STA")
      .add<AbsoluteY<STA>>(0x99, "STA")
      .add<IndirectZeroPageX<STA>>(0x81, "STA")
      .add<IndirectZeroPageY<STA>>(0x91, "STA")

      // STX variations
      .add<ZeroPage<STX>>(0x86, "STX")
      .add<ZeroPageY<STX>>(0x96, "STX")
      .add<Absolute<STX>>(0x8E, "STX")
      // STX has no absolute indexed modes

      // STY variations
      .add<ZeroPage<STY>>(0x84, "STY")
      .add<ZeroPageX<STY>>(0x94, "STY")
      .add<Absolute<STY>>(0x8C, "STY")
      // STY has no absolute indexed modes
      // STY has no indirect modes

      // ORA variations
      .add<Immediate<Ora>>(0x09, "ORA")
      .add<Absolute<Ora>>(0x0D, "ORA")
      .add<IndirectZeroPageX<Ora>>(0x01, "ORA")
      .add<IndirectZeroPageY<Ora>>(0x11, "ORA")
      .add<ZeroPage<Ora>>(0x05, "ORA")
      .add<ZeroPageX<Ora>>(0x15, "ORA")
      .add<AbsoluteY<Ora>>(0x19, "ORA")
      .add<AbsoluteX<Ora>>(0x1D, "ORA")

      // SBC instructions - all addressing modes
      .add<Immediate<Subtract>>(0xE9, "SBC")
      // .add<ZeroPage<Subtract>>(0xE5, "SBC")
      // .add<ZeroPageX<Subtract>>(0xF5, "SBC")
      .add<Absolute<Subtract>>(0xED, "SBC")
      //.add<AbsoluteX<Subtract>>(0xFD, "SBC")
      //.add<AbsoluteY<Subtract>>(0xF9, "SBC")
      // .add<IndirectZeroPageX<Subtract>>(0xE1, "SBC")
      // .add<IndirectZeroPageY<Subtract>>(0xF1, "SBC")

      // Branch instructions
      .add<Relative<BNE>>(0xD0, "BNE")
      .add<Relative<BEQ>>(0xF0, "BEQ")
      .add<Relative<BPL>>(0x10, "BPL")
      .add<Relative<BMI>>(0x30, "BMI")
      .add<Relative<BCC>>(0x90, "BCC")
      .add<Relative<BCS>>(0xB0, "BCS")
      .add<Relative<BVC>>(0x50, "BVC")
      .add<Relative<BVS>>(0x70, "BVS")

      .add<ZeroPage<IncrementMemory>>(0xE6, "INC")  // INC $nn
      .add<ZeroPageX<IncrementMemory>>(0xF6, "INC")  // INC $nn,X
      .add<Absolute<IncrementMemory>>(0xEE, "INC")  // INC $nnnn
      .add<AbsoluteX<IncrementMemory>>(0xFE, "INC")  // INC $nnnn,X
      .add<ZeroPage<DecrementMemory>>(0xC6, "DEC")  // DEC $nn
      .add<ZeroPageX<DecrementMemory>>(0xD6, "DEC")  // DEC $nn,X
      .add<Absolute<DecrementMemory>>(0xCE, "DEC")  // DEC $nnnn
      .add<AbsoluteX<DecrementMemory>>(0xDE, "DEC")  // DEC $nnnn,X

      .add<ZeroPage<Bit>>(0x24, "BIT")
      .add<Absolute<Bit>>(0x2C, "BIT")

      // JMP Absolute and JMP Indirect
      .add<JumpAbsolute>(0x4C, "JMP")
      .add<Absolute<JumpIndirect>>(0x6C, "JMP")
      .add<JumpSubroutine>(0x20, "JSR")  // Note: JSR uses absolute addressing for the target

      //
      ;
  return table;
}(/* immediate execution */);

mos6502::Microcode mos6502::fetchNextOpcode(State& cpu, BusToken bus) noexcept
{
  auto opcode = bus.read(cpu.pc++);
  const Instruction& instr = c_instructions[opcode];
  auto microcode = instr.op;
  return microcode;
}

#if 0

FixedFormatter& operator<<(FixedFormatter& formatter, std::pair<const State&, std::span<Common::Byte, 3>> stateAndBytes) noexcept
{
  const auto& [state, bytes] = stateAndBytes;
  formatter << (cpu.pc - 1) << " : ";

  const Byte opcode = bytes[0];
  const Instruction& instr = c_instructions[opcode];

  assert(instr.format.numberOfOperands < std::size(bytes));

  // Add operand bytes
  formatter << opcode << ' ';
  (instr.format.numberOfOperands > 0 ? (formatter << bytes[1]) : (formatter << "  ")) << ' ';
  (instr.format.numberOfOperands > 1 ? (formatter << bytes[2]) : (formatter << "  ")) << ' ';

  // Mnemonic
  formatter << "  " << instr.mnemonic << ' ';

  // Operand formatting based on addressing mode
  // The maximum length of the operand field is 7 characters (e.g. "$xxxx,X")
  // We will pad with spaces if the operand is shorter
  size_t currentLength = formatter.finalize().length();

  formatter << instr.format.prefix;

  // Special case for branch instructions to show target address
  // Branch instructions have opcodes 0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0

  if (instr.format.numberOfOperands == 2)
  {
    // Need to output an address
    formatter << bytes[2] << bytes[1];
  }
  else if ((opcode & 0x1F) == 0x10)
  {
    // Calculate target address for branch
    int32_t offset = static_cast<int8_t>(bytes[1]);
    int32_t target = static_cast<int32_t>(cpu.pc) + 1 + offset;  // PC + instruction length + offset
    formatter << Address{static_cast<uint16_t>(target)};
  }
  else if (instr.format.numberOfOperands == 1)
  {
    formatter << bytes[1];
  }

  formatter << instr.format.suffix;

  // Pad to 9 characters
  size_t neededSpaces = formatter.finalize().length() - currentLength;
  static constexpr std::string_view padding = "         ";  // 9 spaces

  formatter << padding.substr(0, 9 - neededSpaces);

  // Add registers: A, X, Y, SP, P
  formatter << " A:" << cpu.a;
  formatter << " X:" << cpu.x;
  formatter << " Y:" << cpu.y;
  formatter << " SP:" << cpu.sp;
  formatter << " P:" << cpu.p;
  formatter << ' ';
  cpu6502::flagsToStr(formatter, cpu.p);

  return formatter;
}
#endif

}  // namespace cpu6502
