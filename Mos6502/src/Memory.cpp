#include "Mos6502/Memory.h"

#include <algorithm>
#include <ranges>
#include <span>

Bus Memory::Tick(Bus bus) noexcept
{
  if ((bus.control & Control::Read) != Control::None)
  {
    // Read operation
    bus.data = m_ram[static_cast<uint16_t>(bus.address)];
  }
  else
  {
    // Write operation
    m_ram[static_cast<uint16_t>(bus.address)] = bus.data;
  }

  return bus;
}

void Memory::Load(Address address, std::span<const Byte> bytes) noexcept
{
  assert(std::size(m_ram) - bytes.size() > static_cast<std::size_t>(address));

  auto offset = std::begin(m_ram) + static_cast<std::size_t>(address);
  std::ranges::copy(bytes, offset);
}
