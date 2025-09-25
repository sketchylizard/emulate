#include "common/bus.h"

namespace Common
{

Bus::Bus(std::array<Entry, c_maxDevices> devices) noexcept
  : m_devices(std::move(devices))
  , m_cycles{}
  , m_cycleIndex(0)
{
}

Byte Bus::read(Address address) const
{
  Byte result = 0;

  auto it = std::ranges::find_if(
      m_devices, [address](const Entry& entry) { return address >= entry.start && address <= entry.end; });

  if (it != m_devices.end() && it->device != nullptr)
  {
    auto normalizedAddress = Address{address - it->start};
    result = it->device->read(address, normalizedAddress);
    m_cycles[m_cycleIndex] = Cycle{address, result, true};
    m_cycleIndex = (m_cycleIndex + 1) % c_maxCycles;
  }
  return result;
}

void Bus::write(Address address, Byte value)
{
  auto it = std::ranges::find_if(
      m_devices, [address](const Entry& entry) { return address >= entry.start && address <= entry.end; });

  if (it != m_devices.end() && it->device != nullptr)
  {
    auto normalizedAddress = Address{address - it->start};
    it->device->write(address, normalizedAddress, value);
    m_cycles[m_cycleIndex] = Cycle{address, value, false};

    m_cycleIndex = (m_cycleIndex + 1) % c_maxCycles;
  }
}

}  // namespace Common
