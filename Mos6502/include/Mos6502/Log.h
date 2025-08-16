#pragma once

#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>

#include "Mos6502/Bus.h"

#define MOS6502_TRACE 1

struct LogBuffer
{
#ifdef MOS6502_TRACE
  char buffer[256];
  char* pcPos;  // Position for PC
  char* bytesPos;  // Position for raw bytes
  char* mnemonicPos;  // Position for mnemonic
  char* operandPos;  // Position for operand
  char* registersPos;  // Position for registers
#endif

  void reset(Address pcStart);
  void addByte(Byte byte, size_t position);

  void setInstruction(Byte opcode, std::string_view mnemonic);

  void setOperand(std::string_view operandStr);

  void setRegisters(Byte a, Byte x, Byte y, Byte p, Byte sp);

  void print();
};
