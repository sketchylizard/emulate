#include "cpu6502/mos6502.h"

#include "common/address.h"
#include "common/bus.h"
#include "common/fixed_formatter.h"
#include "cpu6502/address_mode.h"
#include "cpu6502/state.h"

using namespace Common;

namespace cpu6502
{

static MicrocodeResponse branchTaken(State& cpu, Common::BusResponse /*response*/);
static MicrocodeResponse branchPageFixup(State& cpu, Common::BusResponse /*response*/);

static MicrocodeResponse nop(State& cpu, Common::BusResponse /*response*/)
{
  // No operation; used to consume a cycle
  return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
}

// Evaluate condition and decide whether to branch
template<State::Flag flag, bool condition>
static MicrocodeResponse branch(State& cpu, Common::BusResponse /*response*/)
{
  // Check if the flag matches the desired condition
  bool flagSet = cpu.has(flag);
  bool shouldBranch = (flagSet == condition);

  Microcode next = nullptr;

  if (shouldBranch)
  {
    // Branch taken - continue to step 1
    next = branchTaken;
  }

  return {BusRequest::Read(cpu.pc++), next};
}

static MicrocodeResponse branchTaken(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the branch offset (signed)
  cpu.lo = response.data;
  int8_t offset = static_cast<int8_t>(response.data);

  if (offset == -2)
  {  // Self-branch detected
    throw TrapException(cpu.pc - 2);
  }

  // Add offset to PC low byte
  auto tmp = static_cast<int16_t>(static_cast<uint16_t>(cpu.pc) & 0xFF) + offset;
  // Detect if page boundary is crossed
  bool carry = tmp < 0 || tmp > 0xFF;

  // Update PC with new low byte (keep high byte for now)
  cpu.pc = MakeAddress(static_cast<uint8_t>(tmp), HiByte(cpu.pc));

  Microcode nextStage = nullptr;
  if (carry)
  {
    // Page boundary crossed - need step 2 for fixup
    nextStage = branchPageFixup;
  }

  // Dummy read to consume cycle
  return {BusRequest::Read(cpu.pc), nextStage};
}

static MicrocodeResponse branchPageFixup(State& cpu, Common::BusResponse /*response*/)
{
  // Apply the carry/borrow to the high byte
  if (cpu.lo & 0x80)
  {
    // Negative offset - decrement high byte
    cpu.pc -= 0x100;
  }
  else
  {
    // Positive offset - increment high byte
    cpu.pc += 0x100;
  }
  // 4 cycles total, we're done
  // Dummy read to consume cycle
  return {BusRequest::Read(cpu.pc)};
}

template<Common::Byte State::* reg>
static MicrocodeResponse load(State& cpu, Common::BusResponse response)
{
  // This is a generic load operation for A, X, or Y registers.

  auto data = (cpu.*reg) = response.data;
  cpu.setZN(data);
  return MicrocodeResponse{};
}

template<Common::Byte State::* reg>
static MicrocodeResponse store(State& cpu, Common::BusResponse /*response*/)
{
  // This is a generic store operation for A, X, or Y registers.

  return {BusRequest::Write(Common::MakeAddress(cpu.lo, cpu.hi), (cpu.*reg))};
}

struct Add
{
  static MicrocodeResponse step0(State& cpu, Common::BusResponse response)
  {
    assert(cpu.has(State::Flag::Decimal) == false);  // BCD mode not supported

    const Byte a = cpu.a;
    const Byte m = response.data;
    const Byte c = cpu.has(State::Flag::Carry) ? 1 : 0;

    const uint16_t sum = uint16_t(a) + uint16_t(m) + uint16_t(c);
    const Byte result = Byte(sum & 0xFF);

    cpu.set(State::Flag::Carry, (sum & 0x100) != 0);
    cpu.set(State::Flag::Overflow, ((~(a ^ m) & (a ^ result)) & 0x80) != 0);
    cpu.setZN(result);

    cpu.a = result;
    return MicrocodeResponse{};
  }
  static constexpr Microcode ops[] = {&step0};
};

struct Subtract
{

  static MicrocodeResponse step0(State& cpu, Common::BusResponse response)
  {
    // SBC: A = A - M - (1 - C) = A + (~M) + C
    Common::Byte operand = ~response.data;  // Invert the operand for two's complement
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

    return MicrocodeResponse{};
  }

  static constexpr Microcode ops[] = {&step0};
};

struct And
{
  static MicrocodeResponse step0(State& cpu, Common::BusResponse response)
  {
    cpu.a &= response.data;  // A ← A ∧ M
    cpu.setZN(cpu.a);  // Set N and Z flags based on result
    return MicrocodeResponse{};
  }

  static constexpr Microcode ops[] = {&step0};
};

struct Bit
{
  static MicrocodeResponse step0(State& cpu, Common::BusResponse response)
  {
    // BIT instruction: Test bits in memory with accumulator
    // - Z flag: Set if (A & M) == 0
    // - N flag: Copy bit 7 of memory operand
    // - V flag: Copy bit 6 of memory operand
    // - Accumulator is NOT modified
    // - C, I, D flags are not affected

    Byte memory_value = response.data;
    Byte test_result = cpu.a & memory_value;

    // Set Zero flag based on AND result
    cpu.set(State::Flag::Zero, test_result == 0);

    // Copy bit 7 of memory to Negative flag
    cpu.set(State::Flag::Negative, (memory_value & 0x80) != 0);

    // Copy bit 6 of memory to Overflow flag
    cpu.set(State::Flag::Overflow, (memory_value & 0x40) != 0);

    // Note: Accumulator is unchanged!
    return MicrocodeResponse{};
  }

  static constexpr Microcode ops[] = {&step0};
};

template<State::Flag Flag, bool Set>
static MicrocodeResponse flagOp(State& cpu, Common::BusResponse /*response*/)
{
  cpu.set(Flag, Set);
  return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
}

static MicrocodeResponse ora(State& cpu, Common::BusResponse response)
{
  // Perform OR with accumulator
  cpu.a |= response.data;
  cpu.set(State::Flag::Zero, cpu.a == 0);  // Set zero flag
  cpu.set(State::Flag::Negative, cpu.a & 0x80);  // Set negative flag
  return MicrocodeResponse{};
}

static MicrocodeResponse jmpAbsolute(State& cpu, Common::BusResponse response)
{
  // Step 2
  cpu.hi = response.data;
  auto newPC = Common::MakeAddress(cpu.lo, cpu.hi);

  // jmp absolute requires 3 bytes, so if we are jumping to the current instruction,
  // it is a self-jump.
  if (newPC == cpu.pc - 3)
  {  // Self-jump detected
    throw TrapException(newPC);
  }

  cpu.pc = newPC;
  return MicrocodeResponse{};
}

static MicrocodeResponse jmpIndirect3(State& cpu, Common::BusResponse /*response*/)
{
  Address target = Common::MakeAddress(cpu.lo, cpu.hi);
  // JMP (indirect) requires 3 bytes, so if we are jumping to the current instruction,
  // it is a self-jump.
  if (target == cpu.pc - 3)
  {  // Self-jump detected
    throw TrapException(target);
  }
  cpu.pc = target;

  return MicrocodeResponse{};
}

static MicrocodeResponse jmpIndirect2(State& cpu, Common::BusResponse response)
{
  cpu.hi = response.data;
  return {BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi)), jmpIndirect3};  // Read target low byte
}

static MicrocodeResponse jmpIndirect1(State& cpu, Common::BusResponse response)
{
  // Save the incoming low byte returned from the pointer address read
  Byte data = response.data;

  // Read the high byte of the pointer address (with wrap bug)
  ++cpu.lo;
  auto addr = Common::MakeAddress(cpu.lo, cpu.hi);
  // Now store the low byte we got from the first read
  cpu.lo = data;
  return {BusRequest::Read(addr), jmpIndirect2};  // fetch ptr high
}

static MicrocodeResponse jmpIndirect(State& cpu, Common::BusResponse response)
{
  cpu.hi = response.data;

  // Fetch indirect address low byte
  // cpu.lo and cpu.hi already hold the two bytes that followed the opcode, the
  // pointer address. We now need to load the byte at that address, then the byte at
  // the next address (with the 6502 page wrap bug).
  return {BusRequest::Read(MakeAddress(cpu.lo, cpu.hi)), jmpIndirect1};
}

template<Common::Byte State::* reg>
  requires(reg != &State::a)
static MicrocodeResponse increment(State& cpu, Common::BusResponse /*response*/)
{
  // Handle increment operation for X or Y registers
  auto& r = (cpu.*reg);
  ++r;
  cpu.setZN(r);
  return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
}

template<Common::Byte State::* reg>
  requires(reg != &State::a)
static MicrocodeResponse decrement(State& cpu, Common::BusResponse /*response*/)
{
  auto& r = (cpu.*reg);
  --r;
  cpu.setZN(r);
  return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
}

static bool subtractWithBorrow(Byte& reg, Byte value) noexcept
{
  uint16_t diff = uint16_t(reg) - uint16_t(value);
  reg = Byte(diff & 0xFF);
  bool borrow = (diff & 0x100) != 0;
  return borrow;
}

template<Common::Byte State::* reg>
static MicrocodeResponse compare(State& cpu, Common::BusResponse response)
{
  auto r = (cpu.*reg);
  bool borrow = subtractWithBorrow(r, response.data);
  cpu.set(State::Flag::Carry, !borrow);
  cpu.setZN(r);
  return MicrocodeResponse{};
}

template<Common::Byte State::* src, Common::Byte State::* dst>
  requires(src != dst)
static MicrocodeResponse transfer(State& cpu, Common::BusResponse /*response*/)
{
  auto s = (cpu.*src);
  auto& d = (cpu.*dst);
  d = s;
  if constexpr (dst != &State::sp)
  {
    // Only affect flags if not transferring to stack pointer
    cpu.setZN(d);
  }
  return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
}

struct Eor
{
  static MicrocodeResponse step0(State& cpu, Common::BusResponse response)
  {
    cpu.a ^= response.data;  // A ← A ⊕ M
    cpu.setZN(cpu.a);
    return {BusRequest::Read(cpu.pc)};  // Dummy read to consume cycle
  }
  static constexpr Microcode ops[] = {&step0};
};

// Templated push operation following your struct pattern
template<Common::Byte State::* SourceReg, bool SetBreakFlag = false>
struct PushOp
{
  // Step 1: Dummy read to consume cycle
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
  {
    return {BusRequest::Read(cpu.pc)};
  }

  // Step 2: Write register value to stack
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    Byte data = (cpu.*SourceReg);

    // Apply Break flag modification for PHP
    if constexpr (SetBreakFlag)
    {
      data |= static_cast<Byte>(State::Flag::Break);
    }

    return {BusRequest::Write(Common::MakeAddress(cpu.sp--, 0x01), data)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

// Similar pattern for pull operations
template<Common::Byte State::* TargetReg, bool ClearBreakFlag = false, bool SetNZFlags = false>
struct PullOp
{
  // Step 1: Dummy read, increment SP
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
  {
    ++cpu.sp;  // Pre-increment for pull
    return {BusRequest::Read(cpu.pc)};
  }

  // Step 2: Read from stack
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    return {BusRequest::Read(Common::MakeAddress(cpu.sp, 0x01))};
  }

  // Step 3: Store data and set flags if needed
  static MicrocodeResponse step3(State& cpu, Common::BusResponse response)
  {
    Byte data = response.data;

    // Apply Break flag clearing for PLP
    if constexpr (ClearBreakFlag)
    {
      data &= ~static_cast<Byte>(State::Flag::Break);
    }

    // Store in target register
    (cpu.*TargetReg) = data;

    // Update N/Z flags for PLA
    if constexpr (SetNZFlags)
    {
      cpu.setZN(data);
    }

    return {BusRequest::Read(cpu.pc)};  // Final dummy read
  }

  static constexpr Microcode ops[] = {step1, step2, step3};
};

// JSR - Jump to Subroutine (6 cycles)
struct jsr
{
  // Step 4: Push return address high byte (PC-1)
  static MicrocodeResponse step4(State& cpu, Common::BusResponse /*response*/)
  {
    // JSR pushes PC-1 where PC currently points past the JSR instruction
    Byte return_addr_high = Common::HiByte(static_cast<uint16_t>(cpu.pc - 1));
    return {BusRequest::Write(Common::MakeAddress(cpu.sp--, 0x01), return_addr_high)};
  }

  // Step 5: Push return address low byte and jump
  static MicrocodeResponse step5(State& cpu, Common::BusResponse /*response*/)
  {
    Byte return_addr_low = Common::LoByte(static_cast<uint16_t>(cpu.pc - 1));

    // Calculate jump target and perform self-jump detection
    auto newPC = Common::MakeAddress(cpu.lo, cpu.hi);
    if (newPC == cpu.pc - 3)  // JSR is 3 bytes long
    {
      throw TrapException(newPC);
    }

    // Set new PC
    cpu.pc = newPC;

    // Push return address low byte
    return {BusRequest::Write(Common::MakeAddress(cpu.sp--, 0x01), return_addr_low)};
  }

  static constexpr Microcode ops[] = {step4, step5};
};

// RTS - Return from Subroutine (6 cycles)
struct rts
{
  // Step 1: Dummy read from next instruction
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
  {
    // Increment SP in preparation for pull
    return {BusRequest::Read(Common::MakeAddress(++cpu.sp, 0x01))};
  }

  // Step 2: Dummy read from stack, increment SP
  static MicrocodeResponse step2(State& cpu, Common::BusResponse response)
  {
    cpu.lo = response.data;
    // Increment SP in preparation for pull
    return {BusRequest::Read(Common::MakeAddress(++cpu.sp, 0x01))};
  }

  // Step 3: Pull return address low byte
  static MicrocodeResponse step3(State& cpu, Common::BusResponse response)
  {
    cpu.hi = response.data;
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
    return {BusRequest::Read(cpu.pc)};
  }

  // Step 4: Store low byte, pull return address high byte
  static MicrocodeResponse step4(State& cpu, Common::BusResponse /*response*/)
  {
    cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi) + 1;
    return {BusRequest::Read(cpu.pc)};
  }

  static constexpr Microcode ops[] = {step1, step2, step3, step4};
};

// IncrementMemory - Increment Memory Location
struct IncrementMemory
{
  // Step 1: Read from memory, write unmodified value back (6502 quirk)
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    // 6502 quirk: Write the unmodified value back during the modify cycle
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write incremented value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    ++cpu.operand;  // Increment the stored value
    cpu.setZN(cpu.operand);  // Set N and Z flags based on result

    // Reconstruct effective address and write modified value
    // Note: This assumes addressing mode left address info in reconstructible form
    return {BusRequest::Write(Common::MakeAddress(cpu.lo, cpu.hi), cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

// DecrementMemory - Decrement Memory Location
struct DecrementMemory
{
  // Step 1: Read from memory, write unmodified value back (6502 quirk)
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value for step2
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    // 6502 quirk: Write the unmodified value back during the modify cycle
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write decremented value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    --cpu.operand;  // Decrement the stored value
    cpu.setZN(cpu.operand);  // Set N and Z flags based on result

    // Reconstruct effective address and write modified value
    return {BusRequest::Write(Common::MakeAddress(cpu.lo, cpu.hi), cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

////////////////////////////////////////////////////////////////////////////////
// Shift/Rotate Instructions - Accumulator Mode (2 cycles)
////////////////////////////////////////////////////////////////////////////////

// ASL A - Arithmetic Shift Left Accumulator
struct ShiftLeftAccumulator
{
  // Step 1: Shift accumulator left, dummy read
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
  {
    // C ← [7][6][5][4][3][2][1][0] ← 0
    bool bit7 = (cpu.a & 0x80) != 0;
    cpu.a <<= 1;  // Shift left, bit 0 becomes 0

    cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags

    return {BusRequest::Read(cpu.pc)};  // Dummy read
  }

  static constexpr Microcode ops[] = {step1};
};

// LSR A - Logical Shift Right Accumulator
struct ShiftRightAccumulator
{
  // Step 1: Shift accumulator right, dummy read
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
  {
    // 0 → [7][6][5][4][3][2][1][0] → C
    bool bit0 = (cpu.a & 0x01) != 0;
    cpu.a >>= 1;  // Shift right, bit 7 becomes 0

    cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
    cpu.setZN(cpu.a);  // Set N and Z flags (N will always be 0)

    return {BusRequest::Read(cpu.pc)};  // Dummy read
  }

  static constexpr Microcode ops[] = {step1};
};

// ROL A - Rotate Left Accumulator
struct RotateLeftAccumulator
{
  // Step 1: Rotate accumulator left through carry, dummy read
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
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

    return {BusRequest::Read(cpu.pc)};  // Dummy read
  }

  static constexpr Microcode ops[] = {step1};
};

// ROR A - Rotate Right Accumulator
struct RotateRightAccumulator
{
  // Step 1: Rotate accumulator right through carry, dummy read
  static MicrocodeResponse step1(State& cpu, Common::BusResponse /*response*/)
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

    return {BusRequest::Read(cpu.pc)};  // Dummy read
  }

  static constexpr Microcode ops[] = {step1};
};

////////////////////////////////////////////////////////////////////////////////
// Shift/Rotate Instructions - Memory Mode (Read-Modify-Write)
////////////////////////////////////////////////////////////////////////////////

// ASL - Arithmetic Shift Left Memory
struct ShiftLeft
{
  // Step 1: Read from memory, write unmodified value back (6502 quirk)
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value for step2
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    // 6502 quirk: Write the unmodified value back during the modify cycle
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write shifted value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    // C ← [7][6][5][4][3][2][1][0] ← 0
    bool bit7 = (cpu.operand & 0x80) != 0;
    cpu.operand <<= 1;  // Shift left, bit 0 becomes 0

    cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
    cpu.setZN(cpu.operand);  // Set N and Z flags

    // Write modified value back to memory
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

// LSR - Logical Shift Right Memory
struct ShiftRight
{
  // Step 1: Read from memory, write unmodified value back
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value for step2
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write shifted value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    // 0 → [7][6][5][4][3][2][1][0] → C
    bool bit0 = (cpu.operand & 0x01) != 0;
    cpu.operand >>= 1;  // Shift right, bit 7 becomes 0

    cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
    cpu.setZN(cpu.operand);  // Set N and Z flags (N will always be 0)

    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

// ROL - Rotate Left Memory
struct RotateLeft
{
  // Step 1: Read from memory, write unmodified value back
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value for step2
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write rotated value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    // C ← [7][6][5][4][3][2][1][0] ← C
    bool bit7 = (cpu.operand & 0x80) != 0;
    bool old_carry = cpu.has(State::Flag::Carry);

    cpu.operand <<= 1;  // Shift left
    if (old_carry)
    {
      cpu.operand |= 0x01;  // Carry → Bit 0
    }

    cpu.set(State::Flag::Carry, bit7);  // Bit 7 → Carry
    cpu.setZN(cpu.operand);  // Set N and Z flags

    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

// ROR - Rotate Right Memory
struct RotateRight
{
  // Step 1: Read from memory, write unmodified value back
  static MicrocodeResponse step1(State& cpu, Common::BusResponse response)
  {
    // Store original value for step2
    cpu.operand = response.data;
    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);

    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  // Step 2: Write rotated value back to memory
  static MicrocodeResponse step2(State& cpu, Common::BusResponse /*response*/)
  {
    // C → [7][6][5][4][3][2][1][0] → C
    bool bit0 = (cpu.operand & 0x01) != 0;
    bool old_carry = cpu.has(State::Flag::Carry);

    cpu.operand >>= 1;  // Shift right
    if (old_carry)
    {
      cpu.operand |= 0x80;  // Carry → Bit 7
    }

    cpu.set(State::Flag::Carry, bit0);  // Bit 0 → Carry
    cpu.setZN(cpu.operand);  // Set N and Z flags

    Address effective_addr = Common::MakeAddress(cpu.lo, cpu.hi);
    return {BusRequest::Write(effective_addr, cpu.operand)};
  }

  static constexpr Microcode ops[] = {step1, step2};
};

////////////////////////////////////////////////////////////////////////////////
// CPU implementation
////////////////////////////////////////////////////////////////////////////////

namespace
{

using InstructionTable = std::array<Instruction, 256>;

template<typename Mode>
constexpr void add(Common::Byte opcode, std::string_view mnemonic, std::initializer_list<const Microcode> operations,
    InstructionTable& table)
{
  // Get the addressing mode microcode based on template parameter
  std::span<const Microcode> addressingOps = Mode::ops;

  // Check total size at compile time
  if (addressingOps.size() + operations.size() > Instruction::c_maxOperations)
  {
    throw std::runtime_error("Too many microcode operations for instruction");
  }

  Instruction& instr = table[opcode];
  instr.opcode = opcode;
  for (size_t i = 0; i != std::size(instr.mnemonic); ++i)
  {
    instr.mnemonic[i] = (i < mnemonic.size()) ? mnemonic[i] : 0;
  }
  instr.format = Mode::format;

  // Copy addressing microcode first
  size_t index = 0;
  for (auto op : addressingOps)
  {
    instr.ops[index++] = op;
  }

  // Copy operation microcode
  for (auto op : operations)
  {
    instr.ops[index++] = op;
  }

  // Fill remaining slots with nullptr
  for (; index < Instruction::c_maxOperations; ++index)
  {
    instr.ops[index] = nullptr;
  }
}

struct Builder
{
  InstructionTable& table;

  template<typename Mode, typename Cmd>
  constexpr Builder& add(Common::Byte opcode, std::string_view mnemonic)
  {
    // Get the addressing mode microcode based on template parameter
    std::span<const Microcode> addressingOps = Mode::ops;
    std::span<const Microcode> operations = Cmd::ops;

    // Check total size at compile time
    if (addressingOps.size() + operations.size() > Instruction::c_maxOperations)
    {
      throw std::runtime_error("Too many microcode operations for instruction");
    }

    Instruction& instr = table[opcode];
    instr.opcode = opcode;
    for (size_t i = 0; i != std::size(instr.mnemonic); ++i)
    {
      instr.mnemonic[i] = (i < mnemonic.size()) ? mnemonic[i] : 0;
    }
    instr.format = Mode::format;

    // Copy addressing microcode first
    size_t index = 0;
    for (auto op : addressingOps)
    {
      instr.ops[index++] = op;
    }

    // Copy operation microcode
    for (auto op : operations)
    {
      instr.ops[index++] = op;
    }

    // Fill remaining slots with nullptr
    for (; index < Instruction::c_maxOperations; ++index)
    {
      instr.ops[index] = nullptr;
    }
    return *this;
  }
};

}  // namespace

static constexpr auto c_instructions = []()
{
  std::array<Instruction, 256> table{};

  using Flag = State::Flag;

  // Insert actual instructions by opcode
  // add(0x00, "BRK", table, {brk<false>});

  add<Implied>(0xEA, "NOP", {nop}, table);

  // LDA instructions
  add<Immediate>(0xA9, "LDA", {load<&State::a>}, table);
  add<ZeroPage>(0xA5, "LDA", {load<&State::a>}, table);
  add<ZeroPageX>(0xB5, "LDA", {load<&State::a>}, table);
  add<Absolute>(0xAD, "LDA", {load<&State::a>}, table);
  add<AbsoluteX>(0xBD, "LDA", {load<&State::a>}, table);
  add<AbsoluteY>(0xB9, "LDA", {load<&State::a>}, table);
  add<IndirectZeroPageX>(0xA1, "LDA", {load<&State::a>}, table);
  add<IndirectZeroPageY>(0xB1, "LDA", {load<&State::a>}, table);

  // LDX instructions
  add<Immediate>(0xA2, "LDX", {load<&State::x>}, table);
  add<ZeroPage>(0xA6, "LDX", {load<&State::x>}, table);
  add<ZeroPageY>(0xB6, "LDX", {load<&State::x>}, table);
  add<Absolute>(0xAE, "LDX", {load<&State::x>}, table);
  add<AbsoluteY>(0xBE, "LDX", {load<&State::x>}, table);

  // LDY instructions
  add<Immediate>(0xA0, "LDY", {load<&State::y>}, table);
  add<ZeroPage>(0xA4, "LDY", {load<&State::y>}, table);
  add<ZeroPageX>(0xB4, "LDY", {load<&State::y>}, table);
  add<Absolute>(0xAC, "LDY", {load<&State::y>}, table);
  add<AbsoluteX>(0xBC, "LDY", {load<&State::y>}, table);

  // Flag operations
  add<Implied>(0x18, "CLC", {flagOp<Flag::Carry, false>}, table);
  add<Implied>(0x38, "SEC", {flagOp<Flag::Carry, true>}, table);
  add<Implied>(0x58, "CLI", {flagOp<Flag::Interrupt, false>}, table);
  add<Implied>(0x78, "SEI", {flagOp<Flag::Interrupt, true>}, table);
  add<Implied>(0xB8, "CLV", {flagOp<Flag::Overflow, false>}, table);
  add<Implied>(0xD8, "CLD", {flagOp<Flag::Decimal, false>}, table);
  add<Implied>(0xF8, "SED", {flagOp<Flag::Decimal, true>}, table);

  // STA variations
  add<ZeroPage>(0x85, "STA", {store<&State::a>}, table);
  add<ZeroPageX>(0x95, "STA", {store<&State::a>}, table);
  add<Absolute>(0x8D, "STA", {store<&State::a>}, table);
  add<AbsoluteX>(0x9D, "STA", {store<&State::a>}, table);
  add<AbsoluteY>(0x99, "STA", {store<&State::a>}, table);
  add<IndirectZeroPageX>(0x81, "STA", {store<&State::a>}, table);
  add<IndirectZeroPageY>(0x91, "STA", {store<&State::a>}, table);

  // STX variations
  add<ZeroPage>(0x86, "STX", {store<&State::x>}, table);
  add<ZeroPageY>(0x96, "STX", {store<&State::x>}, table);
  add<Absolute>(0x8E, "STX", {store<&State::x>}, table);
  // STX has no absolute indexed modes

  // STY variations
  add<ZeroPage>(0x84, "STY", {store<&State::y>}, table);
  add<ZeroPageX>(0x94, "STY", {store<&State::y>}, table);
  add<Absolute>(0x8C, "STY", {store<&State::y>}, table);
  // STY has no absolute indexed modes
  // STY has no indirect modes

  // ORA variations
  add<IndirectZeroPageX>(0x01, "ORA", {ora}, table);
  add<ZeroPage>(0x05, "ORA", {ora}, table);
  add<Absolute>(0x0D, "ORA", {ora}, table);
  add<IndirectZeroPageY>(0x11, "ORA", {ora}, table);
  add<ZeroPageX>(0x15, "ORA", {ora}, table);
  add<AbsoluteY>(0x19, "ORA", {ora}, table);
  add<AbsoluteX>(0x1D, "ORA", {ora}, table);

  // JMP Absolute and JMP Indirect
  add<AbsoluteJmp>(0x4C, "JMP", {jmpAbsolute}, table);
  add<AbsoluteIndirectJmp>(0x6C, "JMP", {jmpIndirect}, table);

  // Branch instructions
  add<Relative>(0xD0, "BNE", {branch<Flag::Zero, false>}, table);
  add<Relative>(0xF0, "BEQ", {branch<Flag::Zero, true>}, table);
  add<Relative>(0x10, "BPL", {branch<Flag::Negative, false>}, table);
  add<Relative>(0x30, "BMI", {branch<Flag::Negative, true>}, table);
  add<Relative>(0x90, "BCC", {branch<Flag::Carry, false>}, table);
  add<Relative>(0xB0, "BCS", {branch<Flag::Carry, true>}, table);
  add<Relative>(0x50, "BVC", {branch<Flag::Overflow, false>}, table);
  add<Relative>(0x70, "BVS", {branch<Flag::Overflow, true>}, table);

  // Increment and Decrement instructions
  add<Implied>(0xE8, "INX", {increment<&State::x>}, table);
  add<Implied>(0xC8, "INY", {increment<&State::y>}, table);
  add<Implied>(0xCA, "DEX", {decrement<&State::x>}, table);
  add<Implied>(0x88, "DEY", {decrement<&State::y>}, table);

  // CMP — Compare Accumulator
  add<Immediate>(0xC9, "CMP", {compare<&State::a>}, table);
  add<ZeroPage>(0xC5, "CMP", {compare<&State::a>}, table);
  add<ZeroPageX>(0xD5, "CMP", {compare<&State::a>}, table);
  add<Absolute>(0xCD, "CMP", {compare<&State::a>}, table);
  add<AbsoluteX>(0xDD, "CMP", {compare<&State::a>}, table);
  add<AbsoluteY>(0xD9, "CMP", {compare<&State::a>}, table);
  add<IndirectZeroPageX>(0xC1, "CMP", {compare<&State::a>}, table);
  add<IndirectZeroPageY>(0xD1, "CMP", {compare<&State::a>}, table);

  // CPX — Compare X Register
  add<Immediate>(0xE0, "CPX", {compare<&State::x>}, table);
  add<ZeroPage>(0xE4, "CPX", {compare<&State::x>}, table);
  add<Absolute>(0xEC, "CPX", {compare<&State::x>}, table);

  // CPY — Compare Y Register
  add<Immediate>(0xC0, "CPY", {compare<&State::y>}, table);
  add<ZeroPage>(0xC4, "CPY", {compare<&State::y>}, table);
  add<Absolute>(0xCC, "CPY", {compare<&State::y>}, table);

  add<Implied>(0x98, "TYA", {transfer<&State::y, &State::a>}, table);
  add<Implied>(0xA8, "TAY", {transfer<&State::a, &State::y>}, table);
  add<Implied>(0x8A, "TXA", {transfer<&State::x, &State::a>}, table);
  add<Implied>(0xAA, "TAX", {transfer<&State::a, &State::x>}, table);
  add<Implied>(0x9A, "TXS", {transfer<&State::x, &State::sp>}, table);
  add<Implied>(0xBA, "TSX", {transfer<&State::sp, &State::x>}, table);

  Builder builder{table};
  builder  //
      .add<Implied, PullOp<&State::a, false, true>>(0x68, "PLA")
      .add<Implied, PushOp<&State::a, false>>(0x48, "PHA")
      .add<Implied, PullOp<&State::p, true, false>>(0x28, "PLP")
      .add<Implied, PushOp<&State::p, true>>(0x08, "PHP")
      .add<Absolute, jsr>(0x20, "JSR")  // Note: JSR uses absolute addressing for the target
      .add<Implied, rts>(0x60, "RTS")
      .add<ZeroPage, IncrementMemory>(0xE6, "INC")  // INC $nn
      .add<ZeroPageX, IncrementMemory>(0xF6, "INC")  // INC $nn,X
      .add<Absolute, IncrementMemory>(0xEE, "INC")  // INC $nnnn
      .add<AbsoluteX, IncrementMemory>(0xFE, "INC")  // INC $nnnn,X
      .add<ZeroPage, DecrementMemory>(0xC6, "DEC")  // DEC $nn
      .add<ZeroPageX, DecrementMemory>(0xD6, "DEC")  // DEC $nn,X
      .add<Absolute, DecrementMemory>(0xCE, "DEC")  // DEC $nnnn
      .add<AbsoluteX, DecrementMemory>(0xDE, "DEC")  // DEC $nnnn,X
      // Accumulator mode (2 cycles):
      .add<Implied, ShiftLeftAccumulator>(0x0A, "ASL")
      .add<Implied, ShiftRightAccumulator>(0x4A, "LSR")
      .add<Implied, RotateLeftAccumulator>(0x2A, "ROL")
      .add<Implied, RotateRightAccumulator>(0x6A, "ROR")
      // Memory modes (5-7 cycles):
      .add<ZeroPage, ShiftLeft>(0x06, "ASL")  // ASL $nn
      .add<ZeroPageX, ShiftLeft>(0x16, "ASL")  // ASL $nn,X
      .add<Absolute, ShiftLeft>(0x0E, "ASL")  // ASL $nnnn
      .add<AbsoluteX, ShiftLeft>(0x1E, "ASL")  // ASL $nnnn,X
      .add<ZeroPage, ShiftRight>(0x46, "LSR")  // LSR $nn
      .add<ZeroPageX, ShiftRight>(0x56, "LSR")  // LSR $nn,X
      .add<Absolute, ShiftRight>(0x4E, "LSR")  // LSR $nnnn
      .add<AbsoluteX, ShiftRight>(0x5E, "LSR")  // LSR $nnnn,X
      .add<ZeroPage, RotateLeft>(0x26, "ROL")  // ROL $nn
      .add<ZeroPageX, RotateLeft>(0x36, "ROL")  // ROL $nn,X
      .add<Absolute, RotateLeft>(0x2E, "ROL")  // ROL $nnnn
      .add<AbsoluteX, RotateLeft>(0x3E, "ROL")  // ROL $nnnn,X
      .add<ZeroPage, RotateRight>(0x66, "ROR")  // ROR $nn
      .add<ZeroPageX, RotateRight>(0x76, "ROR")  // ROR $nn,X
      .add<Absolute, RotateRight>(0x6E, "ROR")  // ROR $nnnn
      .add<AbsoluteX, RotateRight>(0x7E, "ROR")  // ROR $nnnn,X
      // ADC instructions
      .add<Immediate, Add>(0x69, "ADC")
      .add<ZeroPage, Add>(0x65, "ADC")
      .add<ZeroPageX, Add>(0x75, "ADC")
      .add<Absolute, Add>(0x6D, "ADC")
      .add<AbsoluteX, Add>(0x7D, "ADC")
      .add<AbsoluteY, Add>(0x79, "ADC")
      .add<IndirectZeroPageX, Add>(0x61, "ADC")
      .add<IndirectZeroPageY, Add>(0x71, "ADC")
      // SBC instructions - all addressing modes
      .add<Immediate, Subtract>(0xE9, "SBC")
      .add<ZeroPage, Subtract>(0xE5, "SBC")
      .add<ZeroPageX, Subtract>(0xF5, "SBC")
      .add<Absolute, Subtract>(0xED, "SBC")
      .add<AbsoluteX, Subtract>(0xFD, "SBC")
      .add<AbsoluteY, Subtract>(0xF9, "SBC")
      .add<IndirectZeroPageX, Subtract>(0xE1, "SBC")
      .add<IndirectZeroPageY, Subtract>(0xF1, "SBC")
      // AND instructions - all addressing modes
      .add<Immediate, And>(0x29, "AND")  // AND #$nn
      .add<ZeroPage, And>(0x25, "AND")  // AND $nn
      .add<ZeroPageX, And>(0x35, "AND")  // AND $nn,X
      .add<Absolute, And>(0x2D, "AND")  // AND $nnnn
      .add<AbsoluteX, And>(0x3D, "AND")  // AND $nnnn,X
      .add<AbsoluteY, And>(0x39, "AND")  // AND $nnnn,Y
      .add<IndirectZeroPageX, And>(0x21, "AND")  // AND ($nn,X)
      .add<IndirectZeroPageY, And>(0x31, "AND")  // AND ($nn),Y
      // EOR instructions - all addressing modes (exclusive OR)
      .add<Immediate, Eor>(0x49, "EOR")
      .add<ZeroPage, Eor>(0x45, "EOR")
      .add<ZeroPageX, Eor>(0x55, "EOR")
      .add<Absolute, Eor>(0x4D, "EOR")
      .add<AbsoluteX, Eor>(0x5D, "EOR")
      .add<AbsoluteY, Eor>(0x59, "EOR")
      .add<IndirectZeroPageX, Eor>(0x41, "EOR")
      .add<IndirectZeroPageY, Eor>(0x51, "EOR")
      // BIT only supports 2 addressing modes
      .add<ZeroPage, Bit>(0x24, "BIT")
      .add<Absolute, Bit>(0x2C, "BIT")
      // Add more instructions as needed
      ;

  return table;
}(/* immediate execution */);

mos6502::Response mos6502::fetchNextOpcode(State& state, BusResponse) noexcept
{
  return {BusRequest::Fetch(state.pc++)};
}

std::pair<Microcode*, Microcode*> mos6502::decodeOpcode(uint8_t opcode) noexcept
{
  const Instruction& instr = c_instructions[opcode];
  return {const_cast<Microcode*>(instr.ops), const_cast<Microcode*>(instr.ops) + sizeof(instr.ops) / sizeof(instr.ops[0])};
}

FixedFormatter& operator<<(FixedFormatter& formatter, std::pair<const State&, std::span<Common::Byte, 3>> stateAndBytes) noexcept
{
  const auto& [state, bytes] = stateAndBytes;
  formatter << state.pc << " : ";

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
    int32_t target = static_cast<int32_t>(state.pc) + 2 + offset;  // PC + instruction length + offset
    formatter << Address{static_cast<uint16_t>(target)};
  }
  else if (instr.format.numberOfOperands <= 1)
  {
    formatter << bytes[1];
  }

  formatter << instr.format.suffix;

  // Pad to 9 characters
  size_t neededSpaces = formatter.finalize().length() - currentLength;
  static constexpr std::string_view padding = "         ";  // 9 spaces

  formatter << padding.substr(0, 9 - neededSpaces);

  // Add registers: A, X, Y, SP, P
  formatter << " A:" << state.a;
  formatter << " X:" << state.x;
  formatter << " Y:" << state.y;
  formatter << " SP:" << state.sp;
  formatter << " P:" << state.p;
  formatter << ' ';
  formatter << (state.has(State::Flag::Negative) ? 'N' : '-');
  formatter << (state.has(State::Flag::Overflow) ? 'O' : '-');
  formatter << '-';
  formatter << '-';
  formatter << (state.has(State::Flag::Break) ? 'B' : '-');
  formatter << (state.has(State::Flag::Decimal) ? 'D' : '-');
  formatter << (state.has(State::Flag::Interrupt) ? 'I' : '-');
  formatter << (state.has(State::Flag::Zero) ? 'Z' : '-');
  formatter << (state.has(State::Flag::Carry) ? 'C' : '-');

  return formatter;
}

}  // namespace cpu6502
