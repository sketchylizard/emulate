#pragma once

#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>

#include "common/Bus.h"

#define MOS6502_TRACE 1

struct LogBuffer
{
#ifdef MOS6502_TRACE
  char buffer[256];
  char* pcPos = nullptr;  // Position for PC
  char* opcodePos = nullptr;  // Position for opcode
  char* bytesPos = nullptr;  // Position for raw bytes
  char* mnemonicPos = nullptr;  // Position for mnemonic
  char* operandPos = nullptr;  // Position for operand
  char* registersPos = nullptr;  // Position for registers
#endif

  LogBuffer() noexcept;

  void reset(Common::Address pcStart);
  void addByte(Common::Byte byte, size_t position);

  void setInstruction(Common::Byte opcode, std::string_view mnemonic);

  void setOperand(std::string_view operandStr);

  void setRegisters(Common::Byte a, Common::Byte x, Common::Byte y, Common::Byte p, Common::Byte sp);

  void print();
};
