#pragma once

#include <cstdint>
#include <iostream>
#include <queue>

#include "common/address.h"

namespace apple2
{

// Forward declare to avoid circular dependency
class Apple2System;

class IoDevice
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;

  using ReadHandler = Common::Byte (Apple2System::*)(Common::Address);
  using WriteHandler = void (Apple2System::*)(Common::Address, Common::Byte);

  explicit IoDevice(Apple2System* system) noexcept;

  // Bus interface methods
  Byte read(Address address) const;
  void write(Address address, Byte data);

  // Register handlers for specific addresses
  void registerReadHandler(Address address, ReadHandler handler);
  void registerWriteHandler(Address address, WriteHandler handler);

  // Register handlers for address ranges
  void registerReadRange(Address start, Address end, ReadHandler handler);
  void registerWriteRange(Address start, Address end, WriteHandler handler);

private:
  static constexpr size_t c_size{0x100};
  static constexpr Address c_baseAddress{0xC000};

  // Validate address is in I/O range
  static constexpr bool isValidIoAddress(Common::Address address) noexcept
  {
    return address >= c_baseAddress && address < (c_baseAddress + c_size);
  }

  // Helper to convert address to array index
  static constexpr size_t addressToIndex(Common::Address address) noexcept
  {
    return static_cast<size_t>(address) - static_cast<size_t>(c_baseAddress);
  }

  Apple2System* m_system = nullptr;

  // Simple arrays - all lookups are O(1)!
  std::array<ReadHandler, c_size> m_readHandlers;
  std::array<WriteHandler, c_size> m_writeHandlers;
};

}  // namespace apple2
