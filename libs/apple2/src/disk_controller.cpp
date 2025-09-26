#include "apple2/disk_controller.h"

#include <bit>
#include <iostream>
#include <sstream>

#include "common/logger.h"

namespace apple2
{
using Common::Address;
using Common::Byte;

namespace
{
std::array<int8_t, 256> c_stepTable = []()
{
  std::array<int8_t, 256> table{};
  table.fill(0);
  table[0x18] = table[0x84] = table[0x42] = table[0x21] = -1;  // Move outward
  table[0x81] = table[0x12] = table[0x24] = table[0x48] = 1;  // Move inward
  return table;
}();

}  // namespace

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
  return true;
}

Common::Byte DiskController::read(Address address, Address normalizedAddress) const
{
  // Disk controller ROM is mapped at $C600-$C6FF
  // Control/status registers are at $C0E0-$C0EF

  if (address < c_slot1Rom)
  {
    // Control/status registers
    Control control{static_cast<Byte>(normalizedAddress)};
    switch (control)
    {
      case Control::Phase0_Off: m_status &= ~Phase0Mask; break;
      case Control::Phase0_On: m_status |= Phase0Mask; break;
      case Control::Phase1_Off: m_status &= ~Phase1Mask; break;
      case Control::Phase1_On: m_status |= Phase1Mask; break;
      case Control::Phase2_Off: m_status &= ~Phase2Mask; break;
      case Control::Phase2_On: m_status |= Phase2Mask; break;
      case Control::Phase3_Off: m_status &= ~Phase3Mask; break;
      case Control::Phase3_On: m_status |= Phase3Mask; break;
      case Control::Motor_Off: m_status &= ~MotorMask; break;
      case Control::Motor_On: m_status |= MotorMask; break;
      case Control::SelectDrive0: m_status &= ~DriveSelectMask; break;
      case Control::SelectDrive1: m_status |= DriveSelectMask; break;
      case Control::Q6Low: m_status &= ~Q6Mask; break;
      case Control::Q6High: m_status |= Q6Mask; break;
      case Control::Q7Low: m_status &= ~Q7Mask; break;
      case Control::Q7High: m_status |= Q7Mask; break;
    }
    if (m_status & MotorMask)
    {
      auto currentPhase = static_cast<uint8_t>(m_status & 0x0f);
      if (std::has_single_bit(currentPhase))
      {
        auto action = static_cast<Byte>((m_lastPhase << 4) | currentPhase);
        auto step = c_stepTable[action];
        m_halfTrack = std::clamp<int8_t>(static_cast<int8_t>(m_halfTrack + step), 0, 68);
        std::stringstream stream;
        stream << std::hex << static_cast<int>(action) << std::dec << " : " << static_cast<int>(step)
               << " track: " << static_cast<int>(m_halfTrack) << "\n";
        LOG(stream.str());
        m_lastPhase = currentPhase;
      }
    }
    return m_status;
  }
  else
  {
    size_t index = static_cast<size_t>(normalizedAddress);
    assert(index < std::size(c_rom));
    return c_rom[index];
  }
}

void DiskController::write(Address address, Address normalizedAddress, Common::Byte /*data*/)
{
  if (address < c_slot1Rom)
  {
    size_t offset = static_cast<size_t>(normalizedAddress);
    switch (offset)
    {
      case 0x01:
      case 0x03:
      case 0x05:
      case 0x07:  // Track selection
        break;
      case 0x09:
      case 0x0B:  // Sector selection
        break;
    }
  }
  else
  {
    // ROM area - ignore writes
  }
}

Common::Byte DiskController::readCurrentSector() const
{
  return 0;
}

}  // namespace apple2
