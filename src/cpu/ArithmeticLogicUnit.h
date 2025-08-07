#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>  // for std::pair

class ArithmeticFlags
{
  uint8_t value_ = 0;

public:
  constexpr ArithmeticFlags() noexcept = default;
  constexpr explicit ArithmeticFlags(uint8_t v) noexcept
    : value_{v}
  {
  }

  constexpr explicit operator bool() const noexcept
  {
    return value_ != 0;
  }

  constexpr ArithmeticFlags& operator|=(ArithmeticFlags rhs) noexcept
  {
    value_ |= rhs.value_;
    return *this;
  }

  constexpr ArithmeticFlags& operator&=(ArithmeticFlags rhs) noexcept
  {
    value_ &= rhs.value_;
    return *this;
  }

  friend constexpr ArithmeticFlags operator|(ArithmeticFlags lhs, ArithmeticFlags rhs) noexcept
  {
    lhs |= rhs;
    return lhs;
  }

  friend constexpr ArithmeticFlags operator&(ArithmeticFlags lhs, ArithmeticFlags rhs) noexcept
  {
    lhs &= rhs;
    return lhs;
  }

  friend constexpr ArithmeticFlags operator~(ArithmeticFlags f) noexcept
  {
    return ArithmeticFlags{static_cast<uint8_t>(~f.value_)};
  }

  constexpr bool operator==(ArithmeticFlags rhs) const noexcept
  {
    return value_ == rhs.value_;
  }
};

inline constexpr ArithmeticFlags CarryFlag = ArithmeticFlags{0b0001};
inline constexpr ArithmeticFlags ZeroFlag = ArithmeticFlags{0b0010};
inline constexpr ArithmeticFlags NegativeFlag = ArithmeticFlags{0b0100};
inline constexpr ArithmeticFlags OverflowFlag = ArithmeticFlags{0b1000};

class ArithmeticLogicUnit
{
public:
  // Arithmetic

  static constexpr std::pair<std::byte, ArithmeticFlags> adc(std::byte a, std::byte b, bool carry_in) noexcept
  {
    const auto av = std::to_integer<uint16_t>(a);
    const auto bv = std::to_integer<uint16_t>(b);
    const auto cv = static_cast<uint16_t>(carry_in);

    const uint16_t sum = av + bv + cv;
    const uint8_t result = static_cast<uint8_t>(sum);

    ArithmeticFlags flags = set(CarryFlag, sum > 0xFF) | set(ZeroFlag, result == 0) | set(NegativeFlag, result & 0x80) |
                            set(OverflowFlag, (~(av ^ bv) & (av ^ result)) & 0x80);
    return {std::byte{result}, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> sbc(std::byte a, std::byte b, bool carry_in) noexcept
  {
    const auto bv = std::to_integer<std::uint8_t>(b);
    return adc(a, std::byte{static_cast<std::uint8_t>(~bv)}, carry_in);
  }

  // Logic

  static constexpr std::pair<std::byte, ArithmeticFlags> bitwise_and(std::byte a, std::byte b) noexcept
  {
    const auto result = a & b;
    const auto rv = std::to_integer<std::uint8_t>(result);
    ArithmeticFlags flags = set(ZeroFlag, rv == 0) | set(NegativeFlag, rv & 0x80);
    return {result, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> bitwise_or(std::byte a, std::byte b) noexcept
  {
    const auto result = a | b;
    const auto rv = std::to_integer<std::uint8_t>(result);
    ArithmeticFlags flags = set(ZeroFlag, rv == 0) | set(NegativeFlag, rv & 0x80);
    return {result, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> bitwise_xor(std::byte a, std::byte b) noexcept
  {
    const auto result = a ^ b;
    const auto rv = std::to_integer<std::uint8_t>(result);
    ArithmeticFlags flags = set(ZeroFlag, rv == 0) | set(NegativeFlag, rv & 0x80);
    return {result, flags};
  }

  // Shifts

  static constexpr std::pair<std::byte, ArithmeticFlags> asl(std::byte a) noexcept
  {
    const auto v = std::to_integer<std::uint8_t>(a);
    const auto r = static_cast<std::uint8_t>(v << 1);
    ArithmeticFlags flags = set(CarryFlag, v & 0x80) | set(ZeroFlag, r == 0) | set(NegativeFlag, r & 0x80);
    return {std::byte{r}, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> lsr(std::byte a) noexcept
  {
    const auto v = std::to_integer<std::uint8_t>(a);
    const auto r = static_cast<std::uint8_t>(v >> 1);
    ArithmeticFlags flags = set(CarryFlag, v & 0x01) | set(ZeroFlag, r == 0) | set(NegativeFlag, false);
    return {std::byte{r}, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> rol(std::byte a, bool carry_in) noexcept
  {
    const auto v = std::to_integer<std::uint8_t>(a);
    const auto r = static_cast<std::uint8_t>((v << 1) | (carry_in ? 1 : 0));
    ArithmeticFlags flags = set(CarryFlag, v & 0x80) | set(ZeroFlag, r == 0) | set(NegativeFlag, r & 0x80);
    return {std::byte{r}, flags};
  }

  static constexpr std::pair<std::byte, ArithmeticFlags> ror(std::byte a, bool carry_in) noexcept
  {
    const auto v = std::to_integer<std::uint8_t>(a);
    const auto r = static_cast<std::uint8_t>((v >> 1) | (carry_in ? 0x80 : 0));
    ArithmeticFlags flags = set(CarryFlag, v & 0x01) | set(ZeroFlag, r == 0) | set(NegativeFlag, r & 0x80);
    return {std::byte{r}, flags};
  }

private:
  //! Return the given flagToSet value if value is true, otherwise return ArithmeticFlags{0}
  static constexpr ArithmeticFlags set(ArithmeticFlags flagToSet, bool value) noexcept
  {
    return value ? flagToSet : ArithmeticFlags{0};
  }
};
