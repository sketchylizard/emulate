#pragma once

#include <array>
#include <cctype>
#include <functional>
#include <span>
#include <string_view>

#include "common/address.h"
#include "common/fixed_formatter.h"

namespace common
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

  Logger() noexcept = default;

  // Send output through the configured output function
  void output(std::string_view str);
};

// Convenience macros that compile to nothing if logging disabled

// For normal execution - reads from memory span
#define LOG(str)                         \
  if constexpr (common::logging_enabled) \
  {                                      \
    common::Logger logger;               \
    logger.output(str);                  \
  }                                      \
  ((void) 0)  // Force semicolon

}  // namespace common
