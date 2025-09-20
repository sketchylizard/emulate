#pragma once

#include <cstdint>
#include <span>
#include <variant>

#include "common/address.h"

namespace Common
{

template<size_t BankCount, size_t BankSize>
class BankSwitcher
{
public:
  template<typename... Spans>
  constexpr BankSwitcher(size_t baseAddress, Spans... banks)
    : m_banks{banks...}
    , m_baseAddress(baseAddress)
  {
    static_assert(sizeof...(banks) == BankCount);
  }

  constexpr void selectBank(size_t bank) noexcept
  {
    if (bank < BankCount)
      m_activeBank = bank;
  }

  Byte read(Address address) const
  {
    size_t addr = static_cast<size_t>(address);
    if (addr < m_baseAddress || addr >= m_baseAddress + BankSize)
      throw std::out_of_range("Address out of range for bank switcher");

    size_t offset = addr - m_baseAddress;
    const auto& activeBank = m_banks[m_activeBank];
    if (std::holds_alternative<RamBank>(activeBank))
    {
      return std::get<RamBank>(activeBank)[offset];
    }
    else if (std::holds_alternative<RomBank>(activeBank))
    {
      return std::get<RomBank>(activeBank)[offset];
    }
    throw std::runtime_error("Invalid bank type");
  }

  void write(Address address, Byte value)
  {
    size_t addr = static_cast<size_t>(address);
    if (addr < m_baseAddress || addr >= m_baseAddress + BankSize)
      throw std::out_of_range("Address out of range for bank switcher");

    size_t offset = addr - m_baseAddress;
    auto& activeBank = m_banks[m_activeBank];
    if (std::holds_alternative<RamBank>(activeBank))
    {
      std::get<RamBank>(activeBank)[offset] = value;
    }
    else
    {
      throw std::runtime_error("Attempt to write to read-only bank");
    }
  }

  constexpr size_t size() const noexcept
  {
    return BankSize;
  }

private:
  using RamBank = std::span<Common::Byte, BankSize>;
  using RomBank = std::span<const Common::Byte, BankSize>;
  using Bank = std::variant<RamBank, RomBank>;

  std::array<Bank, BankCount> m_banks;
  size_t m_activeBank = 0;
  size_t m_baseAddress;
};

template<typename T, size_t BankSize, typename... Spans>
BankSwitcher(size_t, std::span<T, BankSize>, Spans...) -> BankSwitcher<sizeof...(Spans) + 1, BankSize>;

}  // namespace Common
