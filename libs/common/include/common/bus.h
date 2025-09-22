#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <format>
#include <tuple>
#include <utility>

#include "common/address.h"

namespace Common
{
template<typename T>
concept BusDevice = requires(T device, Address addr, Byte value) {
  { device.read(addr) } -> std::convertible_to<Byte>;
  { device.write(addr, value) } -> std::same_as<void>;
  { device.contains(addr) } -> std::convertible_to<bool>;  // To check if device handles this address
};

template<typename Processor, BusDevice... Devices>
class Bus : public Processor::BusInterface
{
public:
  Bus(Devices&... devices)
    : m_devices(devices...)
  {
  }

  Byte read(Address address) override
  {
    Byte result = 0;
    bool found = false;

    std::apply([&](auto&... devices)
        { ((devices.contains(address) && !found ? (result = devices.read(address), found = true) : false), ...); },
        m_devices);

    if (!found)
    {
      throw std::out_of_range("No device handles this address");
    }
    return result;
  }

  void write(Address address, Byte value) override
  {
    bool found = false;

    std::apply([&](auto&... devices)
        { ((devices.contains(address) && !found ? (devices.write(address, value), found = true) : false), ...); },
        m_devices);

    if (!found)
    {
      throw std::out_of_range("No device handles this address");
    }
  }

private:
  std::tuple<Devices&...> m_devices;
};

}  // namespace Common
