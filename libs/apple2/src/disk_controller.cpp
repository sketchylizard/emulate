#include "apple2/disk_controller.h"

#include <iostream>
#include <sstream>

#include "common/logger.h"

namespace apple2
{
using Common::Address;
using Common::Byte;

Byte DiskController::c_rom[256] = {};

bool DiskController::loadRom(std::span<const Byte, 256> romData)
{
  std::copy(romData.begin(), romData.end(), std::begin(c_rom));
  return true;
}

bool DiskController::loadDisk(const std::string& filename)
{
  std::stringstream stream;
  stream << "DiskController::loadDisk(\"" << filename << "\")\n";
  LOG(stream.str());
  std::ifstream file(filename, std::ios::binary);
  if (!file)
    return false;

  m_diskData.resize(143360);  // Standard .dsk size
  file.read(reinterpret_cast<char*>(m_diskData.data()), 143360);
  m_diskLoaded = true;
  m_bytesToRead = 256;
  return true;
}

Common::Byte DiskController::read(Address address, Address normalizedAddress) const
{
  // There are two ranges that map to the disk controller: $C0E0-$C0EF and $C600-$C6FF.
  // The second range will be normalized, so they should all be between 0x00 and 0xFF.
  if (static_cast<size_t>(normalizedAddress) < std::size(c_rom))
  {
    return c_rom[static_cast<size_t>(normalizedAddress)];
  }

  switch (static_cast<size_t>(address))
  {
    case 0x0C:  // Read data
      return readCurrentSector();
    case 0x0D:  // Write protect (return not write protected)
      return 0x00;
    case 0x0E:  // Drive status
      return m_diskLoaded && (m_bytesToRead > 0) ? 0x00 : 0x80;
    default: return 0x00;
  }
}

void DiskController::write(Address address, Address normalizedAddress, Common::Byte data)
{
  if (normalizedAddress < Address{0xC0E0})
    return;

  uint16_t offset = static_cast<uint16_t>(normalizedAddress) - 0xC0E0;

  std::stringstream stream;
  stream << "DiskController::write($" << std::hex << static_cast<uint16_t>(address) << ", " << data << std::dec << ")\n";
  LOG(stream.str());

  switch (offset)
  {
    case 0x01:
    case 0x03:
    case 0x05:
    case 0x07:  // Track selection
      m_currentTrack = (offset >> 1);
      break;
    case 0x09:
    case 0x0B:  // Sector selection
      m_currentSector = ((offset - 9) >> 1);
      break;
  }
}

Common::Byte DiskController::readCurrentSector() const
{
  if (!m_diskLoaded || m_bytesToRead <= 0)
    return 0;

  return 0;
}

}  // namespace apple2
