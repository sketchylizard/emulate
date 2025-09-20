#include "cpu6502/logger.h"

#include <iostream>

#include "cpu6502/mos6502.h"

namespace cpu6502
{

namespace
{

LogLevel& GetLogLevel() noexcept
{
  static LogLevel level = LogLevel::None;
  return level;
}

Logger::OutputFunc& GetOutputFunc() noexcept
{
  static Logger::OutputFunc outputFunc = Logger::defaultLogOutput;
  return outputFunc;
}

}  // namespace

void Logger::setLevel(LogLevel level) noexcept
{
  GetLogLevel() = level;
}

LogLevel Logger::level() noexcept
{
  return GetLogLevel();
}

void Logger::setOutputFunc(OutputFunc func) noexcept
{
  GetOutputFunc() = func == nullptr ? defaultLogOutput : func;
}

void Logger::defaultLogOutput(std::string_view output)
{
  std::cout << output << '\n';
}

Logger::Logger() noexcept
  : formatter_(std::span<char>{buffer})
{
  // Clear the buffer
  formatter_ << "";
}

std::array<Common::Byte, 3> Logger::parseHexBytes(std::string_view hexStr)
{
  std::array<Common::Byte, 3> bytes{};

  // Skip whitespace and parse up to 3 hex bytes
  size_t byteIndex = 0;
  for (size_t i = 0; i < hexStr.length() && byteIndex < 3;)
  {
    // Skip whitespace
    while (i < hexStr.length() && std::isspace(hexStr[i]))
    {
      ++i;
    }

    if (i + 1 < hexStr.length())
    {
      // Parse two hex digits
      char highNibble = hexStr[i];
      char lowNibble = hexStr[i + 1];

      auto parseNibble = [](char c) -> uint8_t
      {
        if (c >= '0' && c <= '9')
          return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
          return static_cast<std::uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F')
          return static_cast<std::uint8_t>(c - 'A' + 10);
        return 0xFF;  // Invalid
      };

      bytes[byteIndex] = static_cast<uint8_t>(static_cast<uint8_t>(parseNibble(highNibble)) << 4) |
                         static_cast<uint8_t>(parseNibble(lowNibble));
      ++byteIndex;
      i += 2;
    }
    else
    {
      break;
    }
  }

  return bytes;
}

void Logger::output()
{
  auto func = GetOutputFunc();
  assert(func != nullptr);

  func(formatter_.finalize());
}

void Logger::logInstructionFromHex(const Registers& cpu, std::string_view hexStr)
{
  auto bytes = parseHexBytes(hexStr);
  logInstruction(cpu, std::span<const Common::Byte, 3>{bytes});
}

void Logger::logInstruction(const Registers& cpu, std::span<const Common::Byte, 3> bytes)
{
  // Use the static disassemble function from mos6502
  mos6502::disassemble(cpu, bytes, formatter_);
  output();
}

void Logger::logInstructionFromMemory(const Registers& cpu, std::span<const Common::Byte> memory)
{
  std::array<Common::Byte, 3> bytes{};

  auto pc_addr = static_cast<size_t>(cpu.pc);

  // Read up to 3 bytes from memory, handling bounds safely
  for (size_t i = 0; i < 3; ++i)
  {
    if (pc_addr + i < memory.size())
    {
      bytes[i] = memory[pc_addr + i];
    }
    else
    {
      bytes[i] = 0x00;  // Default for out-of-bounds reads
    }
  }

  logInstruction(cpu, std::span<const Common::Byte, 3>{bytes});
}

void Logger::logBusRead(Common::Address address, Common::Byte data)
{
  formatter_ << "BUS: R " << address << " = " << data;
  output();
}

void Logger::logBusWrite(Common::Address address, Common::Byte data)
{
  formatter_ << "BUS: W " << address << " = " << data;
  output();
}

}  // namespace cpu6502
