#include "cpu6502/mos6502.h"

#include "common/Bus.h"
#include "common/address.h"
#include "cpu6502/address_mode.h"
#include "cpu6502/state.h"

using namespace Common;

namespace cpu6502
{

// Evaluate condition and decide whether to branch
template<State::Flag flag, bool condition>
static Common::BusRequest branch(State& cpu, Common::BusResponse /*response*/)
{
  // Check if the flag matches the desired condition
  bool flagSet = cpu.has(flag);
  bool shouldBranch = (flagSet == condition);

  if (!shouldBranch)
  {
    // Branch not taken - 2 cycles total, we're done
    return BusRequest::Read(cpu.pc);  // ????
  }
  else
  {
    // Branch taken - continue to step 1
    cpu.next = [](State& cpu1, Common::BusResponse /*response1*/)
    {
      int8_t offset = static_cast<int8_t>(cpu1.lo);

      // Add offset to PC low byte
      auto tmp = static_cast<int16_t>(static_cast<uint16_t>(cpu1.pc) & 0xFF) + offset;
      bool carry = tmp < 0 || tmp > 0xFF;  // Detect if page boundary is crossed

      // Update PC with new low byte (keep high byte for now)
      cpu1.pc = MakeAddress(static_cast<uint8_t>(tmp), HiByte(cpu1.pc));

      if (!carry)
      {
        // No page boundary crossed - 3 cycles total, we're done
        // PC is already correct
        return BusRequest::Read(cpu1.pc);  // ????
      }
      else
      {
        // Page boundary crossed - need step 2 for fixup
        cpu1.next = [](State& cpu2, Common::BusResponse /*response2*/)
        {
          // Apply the carry/borrow to the high byte
          if (cpu2.lo & 0x80)
          {
            // Negative offset - decrement high byte
            cpu2.pc -= 0x100;
          }
          else
          {
            // Positive offset - increment high byte
            cpu2.pc += 0x100;
          }
          // 4 cycles total, we're done
          return Common::BusRequest::Read(cpu2.pc);  // Dummy read to consume cycle
        };
        return Common::BusRequest::Read(cpu1.pc);  // Dummy read to consume cycle
      }
    };
    return BusRequest::Read(cpu.pc);
  }
}

template<Common::Byte State::* reg>
static Common::BusRequest load(State& cpu, Common::BusResponse response)
{
  // This is a generic load operation for A, X, or Y registers.

  auto data = (cpu.*reg) = response.data;
  cpu.setZN(data);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

template<Common::Byte State::* reg>
static Common::BusRequest store(State& cpu, Common::BusResponse /*response*/)
{
  // This is a generic store operation for A, X, or Y registers.

  return BusRequest::Write(Common::MakeAddress(cpu.lo, cpu.hi), (cpu.*reg));
}

static Common::BusRequest adc(State& cpu, Common::BusResponse response)
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
  return BusRequest::Read(cpu.pc);  // ???? this is wrong Somehow we need to signal completion
}

template<State::Flag Flag, bool Set>
static Common::BusRequest flagOp(State& cpu, Common::BusResponse /*response*/)
{
  cpu.set(Flag, Set);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}


static Common::BusRequest txs(State& cpu, Common::BusResponse /*response*/)
{
  cpu.sp = cpu.x;
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest ora(State& cpu, Common::BusResponse response)
{
  // Perform OR with accumulator
  cpu.a |= response.data;
  cpu.set(State::Flag::Zero, cpu.a == 0);  // Set zero flag
  cpu.set(State::Flag::Negative, cpu.a & 0x80);  // Set negative flag
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest jmpAbsolute2(State& cpu, Common::BusResponse response)
{
  // Step 2
  cpu.hi = response.data;
  cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest jmpAbsolute1(State& cpu, Common::BusResponse response)
{
  cpu.lo = response.data;

  cpu.next = jmpAbsolute2;
  return Common::BusRequest::Read(cpu.pc++);  // Fetch high byte
}

static Common::BusRequest jmpAbsolute(State& cpu, Common::BusResponse /*response*/)
{
  cpu.next = jmpAbsolute1;
  return Common::BusRequest::Read(cpu.pc++);  // Fetch low byte
}

static Common::BusRequest jmpIndirect4(State& cpu, Common::BusResponse response)
{
  cpu.hi = response.data;
  cpu.pc = Common::MakeAddress(cpu.lo, cpu.hi);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest jmpIndirect3(State& cpu, Common::BusResponse response)
{
  cpu.lo = response.data;

  // 6502 bug: high byte read wraps within the *same* page as ptr
  cpu.hi = 0;  // this is wrong
  cpu.next = jmpIndirect4;
  return BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi));  // read target high (buggy wrap)
}

static Common::BusRequest jmpIndirect2(State& cpu, Common::BusResponse response)
{
  cpu.hi = response.data;
  cpu.next = jmpIndirect3;
  return BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi));  // Read target low byte
}

static Common::BusRequest jmpIndirect1(State& cpu, Common::BusResponse response)
{
  cpu.lo = response.data;
  cpu.next = jmpIndirect2;
  return BusRequest::Read(cpu.pc++);  // fetch ptr high
}

static Common::BusRequest jmpIndirect(State& cpu, Common::BusResponse /*response*/)
{
  cpu.next = jmpIndirect1;
  // Fetch indirect address low byte
  return Common::BusRequest::Read(cpu.pc++);
}

template<Common::Byte State::* reg>
  requires(reg != &State::a)
static Common::BusRequest increment(State& cpu, Common::BusResponse /*response*/)
{
  // Handle increment operation for X or Y registers
  auto& r = (cpu.*reg);
  ++r;
  cpu.setZN(r);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

template<Common::Byte State::* reg>
  requires(reg != &State::a)
static Common::BusRequest decrement(State& cpu, Common::BusResponse /*response*/)
{
  auto& r = (cpu.*reg);
  --r;
  cpu.setZN(r);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static bool subtractWithBorrow(Byte& reg, Byte value) noexcept
{
  uint16_t diff = uint16_t(reg) - uint16_t(value);
  reg = Byte(diff & 0xFF);
  bool borrow = (diff & 0x100) != 0;
  return borrow;
}

template<Common::Byte State::* reg>
static Common::BusRequest compare(State& cpu, Common::BusResponse response)
{
  auto r = (cpu.*reg);
  bool borrow = subtractWithBorrow(r, response.data);
  cpu.set(State::Flag::Carry, !borrow);
  cpu.setZN(r);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

template<Common::Byte State::* src, Common::Byte State::* dst>
  requires(src != dst)
static Common::BusRequest transfer(State& cpu, Common::BusResponse /*response*/)
{
  auto s = (cpu.*src);
  auto& d = (cpu.*dst);
  d = s;
  cpu.setZN(d);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest eor(State& cpu, Common::BusResponse response)
{
  cpu.a ^= response.data;  // A ← A ⊕ M
  cpu.setZN(cpu.a);
  return BusRequest::Read(cpu.pc);  // ??? this is wrong
}

static Common::BusRequest pha(State& cpu, Common::BusResponse /*response*/)
{
  cpu.next = [](State& cpu1, Common::BusResponse /*response1*/)
  {
    // todo cpu1.next = &CoreCpu::nextOp;
    cpu1.lo = cpu1.sp--;
    cpu1.hi = 0x01;
    return Common::BusRequest::Write(Common::MakeAddress(cpu1.lo, cpu1.hi), cpu1.a);
  };
  return Common::BusRequest::Read(cpu.pc);  // Dummy read to consume cycle
}

static Common::BusRequest pla(State& cpu, Common::BusResponse /*response*/)
{
  cpu.next = [](State& cpu1, Common::BusResponse /*response1*/)
  {
    cpu1.next = [](State& cpu2, Common::BusResponse response2)
    {
      cpu2.a = response2.data;
      cpu2.setZN(cpu2.a);
      return BusRequest::Read(cpu2.pc);  // ??? this is wrong
    };
    cpu1.lo = cpu1.sp;
    cpu1.hi = 0x01;
    return Common::BusRequest::Write(Common::MakeAddress(cpu1.lo, cpu1.hi), cpu1.a);
  };
  cpu.sp++;
  return Common::BusRequest::Read(cpu.pc);  // Dummy read to consume cycle
}

////////////////////////////////////////////////////////////////////////////////
// CPU implementation
////////////////////////////////////////////////////////////////////////////////

namespace
{

using InstructionTable = std::array<State::Instruction, 256>;

template<State::AddressModeType mode>
constexpr void add(Common::Byte opcode, const char* mnemonic, std::initializer_list<const State::Microcode> operations,
    InstructionTable& table)
{
  // Get the addressing mode microcode based on template parameter
  std::span<const State::Microcode> addressingOps = AddressMode::getAddressingOps<mode>();

  // Check total size at compile time
  constexpr size_t maxOps = sizeof(Instruction::ops) / sizeof(Instruction::ops[0]);
  if (addressingOps.size() + operations.size() > maxOps)
  {
    throw std::runtime_error("Too many microcode operations for instruction");
  }

  Instruction& instr = table[opcode];
  instr.opcode = opcode;
  instr.mnemonic = mnemonic;
  instr.addressMode = mode;

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
  for (; index < maxOps; ++index)
  {
    instr.ops[index] = nullptr;
  }
}

}  // namespace

static constexpr auto c_instructions = []()
{
  std::array<Instruction, 256> table{};

  using AM = State::AddressModeType;
  using Flag = State::Flag;

  // Insert actual instructions by opcode
  // add(0x00, "BRK", table, {brk<false>});

  add<AM::Implied>(0xEA, "NOP", {}, table);

  // ADC instructions
  add<AM::Immediate>(0x69, "ADC", {adc}, table);
  add<AM::ZeroPage>(0x65, "ADC", {adc}, table);
  add<AM::ZeroPageX>(0x75, "ADC", {adc}, table);
  add<AM::Absolute>(0x6D, "ADC", {adc}, table);
  add<AM::AbsoluteX>(0x7D, "ADC", {adc}, table);
  add<AM::AbsoluteY>(0x79, "ADC", {adc}, table);
  // add<AM::IndirectZpX>(0x61, "ADC", {adc}, table);
  // add<AM::IndirectZpY>(0x71, "ADC", {adc}, table);

  // LDA instructions
  add<AM::Immediate>(0xA9, "LDA", {load<&State::a>}, table);
  add<AM::ZeroPage>(0xA5, "LDA", {load<&State::a>}, table);
  add<AM::ZeroPageX>(0xB5, "LDA", {load<&State::a>}, table);
  add<AM::Absolute>(0xAD, "LDA", {load<&State::a>}, table);
  add<AM::AbsoluteX>(0xBD, "LDA", {load<&State::a>}, table);
  add<AM::AbsoluteY>(0xB9, "LDA", {load<&State::a>}, table);
  // add<AM::IndirectX>(0xA1, "LDA", {load<&State::a>}, table);
  // add<AM::IndirectY>(0xB1, "LDA", {load<&State::a>}, table);

  // LDX instructions
  add<AM::Immediate>(0xA2, "LDX", {load<&State::x>}, table);
  add<AM::ZeroPage>(0xA6, "LDX", {load<&State::x>}, table);
  add<AM::ZeroPageY>(0xB6, "LDX", {load<&State::x>}, table);
  add<AM::Absolute>(0xAE, "LDX", {load<&State::x>}, table);
  add<AM::AbsoluteY>(0xBE, "LDX", {load<&State::x>}, table);

  // LDY instructions
  add<AM::Immediate>(0xA0, "LDY", {load<&State::y>}, table);
  add<AM::ZeroPage>(0xA4, "LDY", {load<&State::y>}, table);
  add<AM::ZeroPageX>(0xB4, "LDY", {load<&State::y>}, table);
  add<AM::Absolute>(0xAC, "LDY", {load<&State::y>}, table);
  add<AM::AbsoluteX>(0xBC, "LDY", {load<&State::y>}, table);

  // Flag operations
  add<AM::Implied>(0x18, "CLC", {flagOp<Flag::Carry, false>}, table);
  add<AM::Implied>(0x38, "SEC", {flagOp<Flag::Carry, true>}, table);
  add<AM::Implied>(0x58, "CLI", {flagOp<Flag::Interrupt, false>}, table);
  add<AM::Implied>(0x78, "SEI", {flagOp<Flag::Interrupt, true>}, table);
  add<AM::Implied>(0xB8, "CLV", {flagOp<Flag::Overflow, false>}, table);
  add<AM::Implied>(0xD8, "CLD", {flagOp<Flag::Decimal, false>}, table);
  add<AM::Implied>(0xF8, "SED", {flagOp<Flag::Decimal, true>}, table);

  // STA variations
  add<AM::ZeroPage>(0x85, "STA", {store<&State::a>}, table);
  add<AM::ZeroPageX>(0x95, "STA", {store<&State::a>}, table);
  add<AM::Absolute>(0x8D, "STA", {store<&State::a>}, table);
  add<AM::AbsoluteX>(0x9D, "STA", {store<&State::a>}, table);
  add<AM::AbsoluteY>(0x99, "STA", {store<&State::a>}, table);
  // add<AM::IndirectZpX>(0x81, "STA", {store<&State::a>}, table);
  // add<AM::IndirectZpY>(0x91, "STA", {store<&State::a>}, table);

  // ORA variations
  // add<AM::IndirectZpX>(0x01, "ORA", {ora}, table);
  add<AM::ZeroPage>(0x05, "ORA", {ora}, table);
  add<AM::Absolute>(0x0D, "ORA", {ora}, table);
  // add<AM::IndirectZpY>(0x11, "ORA", {ora}, table);
  add<AM::ZeroPageX>(0x15, "ORA", {ora}, table);
  add<AM::AbsoluteY>(0x19, "ORA", {ora}, table);
  add<AM::AbsoluteX>(0x1D, "ORA", {ora}, table);

  // JMP Absolute and JMP Indirect
  add<AM::Implied>(0x4C, "JMP", {jmpAbsolute}, table);
  add<AM::Implied>(0x6C, "JMP", {jmpIndirect}, table);

  // Branch instructions
  add<AM::Relative>(0xD0, "BNE", {branch<Flag::Zero, false>}, table);
  add<AM::Relative>(0xF0, "BEQ", {branch<Flag::Zero, true>}, table);
  add<AM::Relative>(0x10, "BPL", {branch<Flag::Negative, false>}, table);
  add<AM::Relative>(0x30, "BMI", {branch<Flag::Negative, true>}, table);
  add<AM::Relative>(0x90, "BCC", {branch<Flag::Carry, false>}, table);
  add<AM::Relative>(0xB0, "BCS", {branch<Flag::Carry, true>}, table);
  add<AM::Relative>(0x50, "BVC", {branch<Flag::Overflow, false>}, table);
  add<AM::Relative>(0x70, "BVS", {branch<Flag::Overflow, true>}, table);

  // Increment and Decrement instructions
  add<AM::Implied>(0xE8, "INX", {increment<&State::x>}, table);
  add<AM::Implied>(0xC8, "INY", {increment<&State::y>}, table);
  add<AM::Implied>(0xCA, "DEX", {decrement<&State::x>}, table);
  add<AM::Implied>(0x88, "DEY", {decrement<&State::y>}, table);

  // CMP — Compare Accumulator
  add<AM::Immediate>(0xC9, "CMP", {compare<&State::a>}, table);
  add<AM::ZeroPage>(0xC5, "CMP", {compare<&State::a>}, table);
  add<AM::ZeroPageX>(0xD5, "CMP", {compare<&State::a>}, table);
  add<AM::Absolute>(0xCD, "CMP", {compare<&State::a>}, table);
  add<AM::AbsoluteX>(0xDD, "CMP", {compare<&State::a>}, table);
  add<AM::AbsoluteY>(0xD9, "CMP", {compare<&State::a>}, table);
  // add<AM::IndirectZpX>(0xC1, "CMP", {compare<&State::a>}, table);
  // add<AM::IndirectZpY>(0xD1, "CMP", {compare<&State::a>}, table);

  // CPX — Compare X Register
  add<AM::Immediate>(0xE0, "CPX", {compare<&State::x>}, table);
  add<AM::ZeroPage>(0xE4, "CPX", {compare<&State::x>}, table);
  add<AM::Absolute>(0xEC, "CPX", {compare<&State::x>}, table);

  // CPY — Compare Y Register
  add<AM::Immediate>(0xC0, "CPY", {compare<&State::y>}, table);
  add<AM::ZeroPage>(0xC4, "CPY", {compare<&State::y>}, table);
  add<AM::Absolute>(0xCC, "CPY", {compare<&State::y>}, table);

  add<AM::Implied>(0x98, "TYA", {transfer<&State::y, &State::a>}, table);
  add<AM::Implied>(0xA8, "TAY", {transfer<&State::a, &State::y>}, table);
  add<AM::Implied>(0x8A, "TXA", {transfer<&State::x, &State::a>}, table);
  add<AM::Implied>(0xAA, "TAX", {transfer<&State::a, &State::x>}, table);
  add<AM::Implied>(0x9A, "TXS", {txs}, table);
  add<AM::Implied>(0xBA, "TSX", {transfer<&State::sp, &State::x>}, table);

  add<AM::Immediate>(0x49, "EOR", {eor}, table);
  add<AM::ZeroPage>(0x45, "EOR", {eor}, table);
  add<AM::ZeroPageX>(0x55, "EOR", {eor}, table);
  add<AM::Absolute>(0x4D, "EOR", {eor}, table);
  add<AM::AbsoluteX>(0x5D, "EOR", {eor}, table);
  add<AM::AbsoluteY>(0x59, "EOR", {eor}, table);
  // add<AM::IndirectZpX>(0x41, "EOR", {eor}, table);
  // add<AM::IndirectZpY>(0x51, "EOR", {eor}, table);

  add<AM::Implied>(0x48, "PHA", {pha}, table);
  add<AM::Implied>(0x68, "PLA", {pla}, table);

  // Add more instructions as needed

  return table;
}(/* immediate execution */);

std::span<const Instruction, 256> mos6502::GetInstructions() noexcept
{
  return c_instructions;
}

}  // namespace cpu6502
