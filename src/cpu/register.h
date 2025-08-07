#pragma once

#include <concepts>
#include <cstddef> // for std::byte

template<typename Tag, std::byte DefaultValue = std::byte{0}>
class Register
{
  std::byte m_value = DefaultValue;

public:
  constexpr Register() noexcept = default;

  constexpr std::byte read() const noexcept
  {
    return m_value;
  }
  constexpr void write(std::byte value) noexcept
  {
    m_value = value;
  }
};
