#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <utility>

#include "common/address.h"

namespace Common
{
template<typename Processor>
class Bus : public Processor::BusInterface
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;

  static constexpr size_t c_maxDevices{16};

  class Device
  {
  public:
    virtual ~Device() = default;
    virtual Byte read(Address address) const = 0;
    virtual void write(Address address, Byte value) = 0;
  };

  template<typename T>
  class DeviceWrapper : public Device
  {
  public:
    explicit DeviceWrapper(T& device)
      : m_device(device)
    {
    }
    Byte read(Address address) const override
    {
      return m_device.read(address);
    }
    void write(Address address, Byte value) override
    {
      m_device.write(address, value);
    }

  private:
    T& m_device;
  };

  struct Entry
  {
    bool normalize;
    uint16_t start;  // inclusive start
    uint32_t end;  // exclusive end
    std::unique_ptr<Device> device;
  };

  static Entry makeEntry(bool normalize, uint16_t start, uint32_t end, auto& device)
  {
    return Entry{normalize, start, end, std::make_unique<DeviceWrapper<decltype(device)>>(device)};
  }

  explicit Bus(std::array<Entry, c_maxDevices> devices) noexcept
    : m_devices(std::move(devices))
  {
  }

  Byte read(Address address) override
  {
    Byte result = 0;

    auto it = std::ranges::find_if(m_devices, [address](const Entry& entry)
        { return static_cast<uint16_t>(address) >= entry.start && static_cast<uint32_t>(address) < entry.end; });

    if (it != m_devices.end() && it->device != nullptr)
    {
      if (it->normalize)
      {
        address = Address{static_cast<uint16_t>(address - static_cast<uint16_t>(it->start))};
      }
      result = it->device->read(address);
    }
    return result;
  }

  void write(Address address, Byte value) override
  {
    auto it = std::ranges::find_if(m_devices, [address](const Entry& entry)
        { return static_cast<uint16_t>(address) >= entry.start && static_cast<uint32_t>(address) < entry.end; });

    if (it != m_devices.end() && it->device != nullptr)
    {
      if (it->normalize)
      {
        address = Address{static_cast<uint16_t>(address - static_cast<uint16_t>(it->start))};
      }
      it->device->write(address, value);
    }
  }

private:
  std::array<Entry, c_maxDevices> m_devices;
};

}  // namespace Common
