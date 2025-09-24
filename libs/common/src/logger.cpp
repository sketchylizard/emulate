#include "common/logger.h"

#include <iostream>

namespace common
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

void Logger::output(std::string_view str)
{
  auto func = GetOutputFunc();
  assert(func != nullptr);

  func(str);
}

}  // namespace common
