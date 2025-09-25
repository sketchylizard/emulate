#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "common/address.h"

namespace Common
{

//! A simple bus that maps address ranges to devices. Each device must implement the
//! Device interface with read and write methods. The bus will route read and write
//! requests to the appropriate device based on the address.
class Bus
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;

  static constexpr size_t c_maxDevices{16};
  static constexpr size_t c_maxCycles{16};

  class Device
  {
  public:
    virtual ~Device() = default;
    virtual Byte read(Address address, Address normalizedAddress) const = 0;
    virtual void write(Address address, Address normalizedAddress, Byte value) = 0;
  };

  struct Entry
  {
    Address start;  // inclusive start
    Address end;  // exclusive end
    Device* device;
  };

  struct Cycle
  {
    Address address;
    Byte data;
    bool isRead;

    bool operator==(const Cycle& cycle) const noexcept = default;
  };

  explicit Bus(std::array<Entry, c_maxDevices> devices) noexcept;

  Byte read(Address address) const;
  void write(Address address, Byte value);

  std::span<const Cycle> cycles() const noexcept
  {
    size_t count = m_cycleIndex;
    m_cycleIndex = 0;  // Reset for next instruction
    return {m_cycles.data(), count};
  }

private:
  std::array<Entry, c_maxDevices> m_devices;
  // Longest possible instruction is 7 cycles I think.
  mutable std::array<Cycle, c_maxCycles> m_cycles;
  mutable size_t m_cycleIndex = 0;
};

}  // namespace Common
