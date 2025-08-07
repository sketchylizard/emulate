#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>

// User-defined literal helper
template<std::size_t N>
struct HexLiteral
{
  std::array<std::byte, N / 2> data{};  // Conservative over-allocation

  static constexpr std::uint8_t ToHex(char c) noexcept
  {
    if (c >= '0' && c <= '9')
      return static_cast<std::uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f')
      return static_cast<std::uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
      return static_cast<std::uint8_t>(c - 'A' + 10);
    return 0xFF;  // Invalid
  }

  static constexpr bool IsWhitespace(char c) noexcept
  {
    return c == ' ' || c == '\t' || c == '\n';
  }

  constexpr HexLiteral(const char (&str)[N])
  {
    std::size_t index = 0;

    auto beg = &str[0];
    const auto end = beg + N;
    for (; beg + 1 != end;)
    {
      // Skip whitespace and comments
      auto it = std::find_if(beg, end, [](char c) { return !IsWhitespace(c); });
      if (it != beg)
      {
        beg = it;
        continue;
      }

      // Line comment
      if (*beg == ';')
      {
        beg = std::find(beg, end, '\n');
        continue;
      }

      const auto hi = ToHex(*beg++);
      const auto lo = ToHex(*beg++);

      if (hi == 0xFF || lo == 0xFF)
        throw std::runtime_error("Invalid hex digit in literal");

      data[index++] = std::byte{static_cast<uint8_t>((hi << 4) | lo)};
    }
  }

  constexpr auto size() const noexcept
  {
    return std::count_if(data.begin(), data.end(), [](std::byte b) { return b != std::byte{0}; });
  }

  constexpr auto begin() const noexcept
  {
    return data.begin();
  }
  constexpr auto end() const noexcept
  {
    return data.end();
  }

  constexpr operator std::span<const std::byte>() const noexcept
  {
    return std::span<const std::byte>(data.data(), size());
  }
};

// UDL entry point
template<HexLiteral lit>
consteval auto operator""_hex()
{
  return lit;
}
