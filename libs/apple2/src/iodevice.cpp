#include "apple2/iodevice.h"

#include <iostream>

#include "apple2/apple2system.h"

namespace apple2
{

IoDevice::IoDevice(Apple2System* system) noexcept
  : m_system(system)
{
  // Initialize arrays to nullptr
  m_readHandlers.fill(nullptr);
  m_writeHandlers.fill(nullptr);
}

bool IoDevice::contains(Common::Address address) const noexcept
{
  return isValidIoAddress(address);
}

Common::Byte IoDevice::read(Common::Address address)
{
  if (!isValidIoAddress(address))
  {
    return 0x00;
  }

  size_t index = addressToIndex(address);
  if (ReadHandler handler = m_readHandlers[index]; handler != nullptr)
  {
    return (m_system->*handler)(address);
  }

  // Default: return 0 for unhandled reads
  return 0x00;
}

void IoDevice::write(Common::Address address, Common::Byte data)
{
  if (!isValidIoAddress(address))
  {
    return;
  }

  // Simple O(1) array lookup - that's it!
  size_t index = addressToIndex(address);
  if (WriteHandler handler = m_writeHandlers[index]; handler != nullptr)
  {
    (m_system->*handler)(address, data);
  }

  // Default: ignore unhandled writes (no error needed)
}

void IoDevice::registerReadHandler(Common::Address address, ReadHandler handler)
{
  if (!isValidIoAddress(address))
  {
    return;  // Could throw or log error
  }

  size_t index = addressToIndex(address);
  m_readHandlers[index] = handler;
}

void IoDevice::registerWriteHandler(Common::Address address, WriteHandler handler)
{
  if (!isValidIoAddress(address))
  {
    return;  // Could throw or log error
  }

  size_t index = addressToIndex(address);
  m_writeHandlers[index] = handler;
}

void IoDevice::registerReadRange(Common::Address start, Common::Address end, ReadHandler handler)
{
  if (!isValidIoAddress(start) || !isValidIoAddress(end))
  {
    return;  // Could throw or log error
  }

  // Simply fill all addresses in the range with the same handler!
  uint16_t startAddr = static_cast<uint16_t>(start);
  uint16_t endAddr = static_cast<uint16_t>(end);

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr)
  {
    registerReadHandler(Common::Address{addr}, handler);
  }
}

void IoDevice::registerWriteRange(Common::Address start, Common::Address end, WriteHandler handler)
{
  if (!isValidIoAddress(start) || !isValidIoAddress(end))
  {
    return;  // Could throw or log error
  }

  // Simply fill all addresses in the range with the same handler!
  uint16_t startAddr = static_cast<uint16_t>(start);
  uint16_t endAddr = static_cast<uint16_t>(end);

  for (uint16_t addr = startAddr; addr <= endAddr; ++addr)
  {
    registerWriteHandler(Common::Address{addr}, handler);
  }
}

}  // namespace apple2
