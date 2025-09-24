#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <sstream>

#include "common/address.h"
#include "common/bus.h"
#include "common/logger.h"

namespace apple2
{

class SlotController
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;
  using RomSpan = std::span<const Common::Byte, 0x100>;

  static constexpr size_t c_numSlots{7};
  static constexpr size_t c_size{0x0100};  // 256 bytes per slot
  static constexpr Address c_baseAddress{0xC100};
  static constexpr Address c_endAddress = c_baseAddress + c_numSlots * c_size;

  SlotController() noexcept = default;

  std::pair<int8_t, uint8_t> slotAndOffset(Address addr) const
  {
    if (addr < c_baseAddress || addr >= c_endAddress)
      return {-1, 0};  // Out of slot range

    auto adjustedAddress = static_cast<size_t>(addr - c_baseAddress);

    auto slot = adjustedAddress / c_size;
    assert(slot < c_numSlots);

    auto offset = adjustedAddress % c_size;
    assert(offset < c_size);
    return {static_cast<int8_t>(slot), static_cast<uint8_t>(offset)};
  }

  bool contains(Address addr) const
  {
    auto [slot, offset] = slotAndOffset(addr);
    if (slot == -1 || m_rom[slot] == nullptr)
      return false;  // Out of slot range

    return true;
  }

  Byte read(Address addr) const
  {
    // std::stringstream stream;
    // stream << "SlotController::read($" << std::hex << static_cast<uint16_t>(addr) << std::dec << ")\n";
    // LOG(stream.str());

    auto [slot, offset] = slotAndOffset(addr);
    assert(slot != -1 && static_cast<size_t>(slot) < c_numSlots);  // Out of slot range
    return m_rom[slot][offset];
  }

  void write(Address addr, Byte data)
  {
    // Most slot ROMs are read-only
    static_cast<void>(addr);
    static_cast<void>(data);
  }

  void loadRom(int slot, RomSpan romData)
  {
    if (slot < 0 || slot >= static_cast<int>(c_numSlots))
      throw std::out_of_range("Slot number must be between 0 and 7");

    m_rom[slot - 1] = romData.data();
  }

private:
  // All ROM spans are the same size (256 bytes), and initially empty (nullptr).
  const Byte* m_rom[c_numSlots] = {};
};

}  // namespace apple2
