#include "Mos6502/Tracer.h"

// helps ensure that Tracer.h is self-contained

#if MOS6502_TRACE

#include <iomanip>
#include <iostream>

void Tracer::addRegistersImpl(Address pc, Byte a, Byte x, Byte y, Byte p, Byte sp) noexcept
{
  // Output previous instruction if complete
  if (current_frame.has_registers && current_frame.has_instruction)
  {
    outputTrace();
  }

  // Start new frame
  current_frame = {};
  current_frame.pc = pc;
  current_frame.a = a;
  current_frame.x = x;
  current_frame.y = y;
  current_frame.p = p;
  current_frame.sp = sp;
  current_frame.has_registers = true;
}

void Tracer::addInstructionImpl(Byte opcode, std::string_view mnemonic) noexcept
{
  current_frame.opcode = opcode;
  current_frame.mnemonic = mnemonic;
  current_frame.has_instruction = true;
}

void Tracer::addAddressModeImpl(OperandType operandType, Byte lo, Byte hi) noexcept
{
  current_frame.operand_type = operandType;
  current_frame.operand_lo = lo;
  current_frame.operand_hi = hi;
  current_frame.has_addressing = true;

  // Option 2: Output immediately after addressing mode
  // Uncomment these lines for immediate output:
  // outputTrace();
  // current_frame = {};
}

void Tracer::flushImpl() noexcept
{
  if (current_frame.has_registers && current_frame.has_instruction)
  {
    outputTrace();
    current_frame = {};
  }
}

void Tracer::outputTrace() noexcept
{
  // Format: PC   A  X  Y  SP  P   OPCODE MNEMONIC  OPERANDS
  std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
            << static_cast<uint16_t>(current_frame.pc) << "  " << std::setw(2) << static_cast<int>(current_frame.a)
            << " " << std::setw(2) << static_cast<int>(current_frame.x) << " " << std::setw(2)
            << static_cast<int>(current_frame.y) << " " << std::setw(2) << static_cast<int>(current_frame.sp) << "  "
            << std::setw(2) << static_cast<int>(current_frame.p) << "   " << std::setw(2)
            << static_cast<int>(current_frame.opcode) << " " << current_frame.mnemonic;

  // Add operands if available
  if (current_frame.has_addressing && current_frame.operand_type != OperandType::None)
  {
    std::cout << " ";
    formatOperand();
  }

  std::cout << std::endl;
}

void Tracer::formatOperand() noexcept
{
  auto type = current_frame.operand_type;
  auto lo = current_frame.operand_lo;
  auto hi = current_frame.operand_hi;

  switch (type)
  {
    case OperandType::Immediate: std::cout << "#$" << std::hex << std::setw(2) << static_cast<int>(lo); break;
    case OperandType::ZeroPage: std::cout << "$" << std::hex << std::setw(2) << static_cast<int>(lo); break;
    case OperandType::ZeroPageX: std::cout << "$" << std::hex << std::setw(2) << static_cast<int>(lo) << ",X"; break;
    case OperandType::ZeroPageY: std::cout << "$" << std::hex << std::setw(2) << static_cast<int>(lo) << ",Y"; break;
    case OperandType::Absolute:
      std::cout << "$" << std::hex << std::setw(4) << (static_cast<int>(hi) << 8 | static_cast<int>(lo));
      break;
    case OperandType::AbsoluteX:
      std::cout << "$" << std::hex << std::setw(4) << (static_cast<int>(hi) << 8 | static_cast<int>(lo)) << ",X";
      break;
    case OperandType::AbsoluteY:
      std::cout << "$" << std::hex << std::setw(4) << (static_cast<int>(hi) << 8 | static_cast<int>(lo)) << ",Y";
      break;
    case OperandType::Indirect:
      std::cout << "($" << std::hex << std::setw(4) << (static_cast<int>(hi) << 8 | static_cast<int>(lo)) << ")";
      break;
    case OperandType::IndirectX: std::cout << "($" << std::hex << std::setw(2) << static_cast<int>(lo) << ",X)"; break;
    case OperandType::IndirectY: std::cout << "($" << std::hex << std::setw(2) << static_cast<int>(lo) << "),Y"; break;
    case OperandType::Relative:
    {
      // For relative addressing, show target address
      int8_t offset = static_cast<int8_t>(lo);
      uint16_t pc = static_cast<uint16_t>(current_frame.pc);
      uint16_t target = static_cast<uint16_t>(pc + 2 + offset);  // PC + instruction length + offset
      std::cout << "$" << std::hex << std::setw(4) << target;
      break;
    }
    case OperandType::Accumulator: std::cout << "A"; break;
    case OperandType::None:
      // No operand to display
      break;
  }
}

#endif  // MOS6502_TRACE
