#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "common/address.h"

namespace Common
{

// Fixed-buffer formatter that never allocates
class FixedFormatter
{
private:
  char* m_begin;
  char* m_current;
  char* m_end;

  static constexpr char hexDigits[] = "0123456789ABCDEF";

public:
  explicit FixedFormatter(std::span<char> buffer) noexcept;

  // Write a single character
  FixedFormatter& operator<<(char c) noexcept;

  // Write a string literal
  FixedFormatter& operator<<(const char* str) noexcept;

  // Write a byte as 2 hex digits
  FixedFormatter& operator<<(Common::Byte value) noexcept;

  // Write an address as 4 hex digits
  FixedFormatter& operator<<(Common::Address addr) noexcept;

  // Null-terminate and return used length
  std::string_view finalize() noexcept;
};

}  // namespace Common
