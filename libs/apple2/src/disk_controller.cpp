#include "apple2/disk_controller.h"

#include <iostream>
#include <sstream>

#include "common/logger.h"

namespace apple2
{

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

Common::Byte DiskController::read(Address address)
{
  uint16_t offset = static_cast<uint16_t>(address) - 0xC0E0;

  std::stringstream stream;
  stream << "DiskController::read($" << std::hex << static_cast<uint16_t>(address) << std::dec << ")\n";
  LOG(stream.str());

  switch (offset)
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

void DiskController::write(Address address, Common::Byte data)
{
  uint16_t offset = static_cast<uint16_t>(address) - 0xC0E0;

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

Common::Byte DiskController::readCurrentSector()
{
  if (!m_diskLoaded || m_bytesToRead <= 0)
    return 0;

  std::stringstream stream;
  stream << "DiskController::read sector(" << m_currentTrack << ", " << m_currentSector << ")\n";
  LOG(stream.str());

  size_t offset = static_cast<size_t>(m_currentTrack * 16 + m_currentSector) * 256 +
                  static_cast<size_t>(256 - m_bytesToRead--);
  // Simplified - real controller has complex state machine  ;
  return offset < m_diskData.size() ? m_diskData[offset] : 0;
}

}  // namespace apple2
