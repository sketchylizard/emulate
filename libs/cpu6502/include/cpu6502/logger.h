#pragma once

#include <array>
#include <cctype>
#include <functional>
#include <span>
#include <string_view>

#include "common/address.h"
#include "common/fixed_formatter.h"
#include "cpu6502/state.h"

namespace cpu6502
{

enum class LogLevel
{
  None,  // No logging output
  Minimal,  // Instruction disassembly only
  Verbose  // Instruction disassembly + bus cycles
};

#ifdef EMULATE_ENABLE_LOGGING
constexpr bool logging_enabled = true;
#else
constexpr bool logging_enabled = false;
#endif

class Logger
{
public:
  // Function pointer type for log output
  using OutputFunc = void (*)(std::string_view output);

  // Runtime level control
  static void setLevel(LogLevel level) noexcept;
  static LogLevel level() noexcept;

  // Override output destination (default is stdout)
  static void setOutputFunc(OutputFunc func) noexcept;

  // Default output function (to stdout)
  static void defaultLogOutput(std::string_view output);

  Logger() noexcept;

  // Parse hex string and log (for test harness)
  void logInstructionFromHex(const VisibleState& cpu, std::string_view hexStr);

  // Disassembly from raw bytes (internal use)
  void logInstruction(const VisibleState& cpu, std::span<const Common::Byte, 3> bytes);

  // Disassembly from memory span (for normal execution)
  void logInstructionFromMemory(const VisibleState& cpu, std::span<const Common::Byte> memory);

  // Bus cycle logging (for verbose mode)
  void logBusRead(Common::Address address, Common::Byte data);
  void logBusWrite(Common::Address address, Common::Byte data);

private:
  char buffer[120];
  Common::FixedFormatter formatter_;

  // Helper to parse hex string to bytes
  std::array<Common::Byte, 3> parseHexBytes(std::string_view hexStr);

  // Send output through the configured output function
  void output();
};

// Convenience macros that compile to nothing if logging disabled

// For test harness - takes hex string like "A9 42 00"
#define LOG_INSTRUCTION_BYTES(cpu, hexStr)       \
  if constexpr (logging_enabled)                 \
  {                                              \
    if (Logger::level() >= LogLevel::Minimal)    \
    {                                            \
      static thread_local Logger logger;         \
      logger.logInstructionFromHex(cpu, hexStr); \
    }                                            \
  }                                              \
  ((void) 0)  // Force semicolon

// For normal execution - reads from memory span
#define LOG_INSTRUCTION_MEMORY(cpu, memory_span)         \
  if constexpr (logging_enabled)                         \
  {                                                      \
    if (Logger::level() >= LogLevel::Minimal)            \
    {                                                    \
      static thread_local Logger logger;                 \
      logger.logInstructionFromMemory(cpu, memory_span); \
    }                                                    \
  }                                                      \
  ((void) 0)  // Force semicolon

// For verbose bus operation logging
#define LOG_BUS_READ(address, data)           \
  if constexpr (logging_enabled)              \
  {                                           \
    if (Logger::level() >= LogLevel::Verbose) \
    {                                         \
      static thread_local Logger logger;      \
      logger.logBusRead(address, data);       \
    }                                         \
  }                                           \
  ((void) 0)  // Force semicolon

#define LOG_BUS_WRITE(address, data)          \
  if constexpr (logging_enabled)              \
  {                                           \
    if (Logger::level() >= LogLevel::Verbose) \
    {                                         \
      static thread_local Logger logger;      \
      logger.logBusWrite(address, data);      \
    }                                         \
  }                                           \
  ((void) 0)  // Force semicolon

}  // namespace cpu6502
