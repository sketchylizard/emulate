#include "Mos6502/Log.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>
#include <utility>

LogBuffer::LogBuffer() noexcept
{
  reset(0x0000_addr);
}

void LogBuffer::reset(Address pcStart)
{
  std::fill(buffer, buffer + sizeof(buffer), ' ');
  auto it = buffer;

  // PC (5 chars: "C000 ")
  pcPos = it;
  std::format_to_n(pcPos, 5, "{:04X} ", static_cast<uint16_t>(pcStart));
  it += 5;

  opcodePos = it;
  std::format_to_n(opcodePos, 3, "XX ");
  it += 3;

  // Raw bytes (8 chars: "        " - up to 2 bytes)
  bytesPos = it;
  it += 8;

  // Mnemonic (4 chars: "LDA ")
  mnemonicPos = it;
  it += 4;

  // Operand (28 chars for alignment)
  operandPos = it;
  it += 28;

  // Registers start here
  registersPos = it;
}

void LogBuffer::addByte(Byte byte, size_t position)
{
  if (position < 3)
  {  // Max 3 bytes
    std::format_to_n(bytesPos + position * 3, 3, "{:02X}", byte);
  }
}

void LogBuffer::setInstruction(Byte opcode, std::string_view mnemonic)
{
  std::format_to_n(opcodePos, 3, "{:02X} ", opcode);
  std::format_to_n(mnemonicPos, 4, "{:<3} ", mnemonic);
}

void LogBuffer::setOperand(std::string_view operandStr)
{
  size_t len = std::min(operandStr.size(), 27ul);
  std::copy_n(operandStr.data(), len, operandPos);
}

void LogBuffer::setRegisters(Byte a, Byte x, Byte y, Byte p, Byte sp)
{
  std::format_to_n(registersPos, 30, "A:{:02X} X:{:02X} Y:{:02X} P:{:02X} SP:{:02X}", a, x, y, p, sp);
}

void LogBuffer::print()
{
  // Null terminate at end of registers
  buffer[registersPos - buffer + 29] = '\0';
  std::cout << buffer << '\n';
}
