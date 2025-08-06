#pragma once

#include <cstddef> // for std::byte

template<typename Tag>
class Register {
  std::byte value = std::byte{0};

public:
  constexpr Register() noexcept = default;

  constexpr std::byte read() const noexcept { return value; }
  constexpr void write(std::byte v) noexcept { value = v; }
  constexpr void reset() noexcept { value = std::byte{0}; }
};
