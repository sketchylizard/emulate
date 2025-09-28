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

Byte encodeNibble(Byte value) noexcept
{
  // Simplified disk-safe encoding (high bit set)
  return 0x80 | (value & 0x7F);
}

size_t calculateDiskOffset(int track, int sector, int byteIndex) noexcept
{
  // DOS 3.3 sector interleaving
  static constexpr int dosOrder[] = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
  int physicalSector = dosOrder[sector];

  return (static_cast<size_t>(track) * 16 + static_cast<size_t>(physicalSector)) * 256 + static_cast<size_t>(byteIndex);
}

std::array<Byte, 8> generateAddressField(int track, int sector)
{
  // Simplified 4-and-4 encoding for now
  Byte volume = 254;
  Byte checksum = static_cast<Byte>(volume ^ track ^ sector);

  std::array<Byte, 8> buffer{};

  // Each value becomes 2 nibbles (simplified encoding)
  buffer[0] = encodeNibble((volume >> 1) & 0x0F);
  buffer[1] = encodeNibble(volume & 0x0F);
  buffer[2] = encodeNibble((track >> 1) & 0x0F);
  buffer[3] = encodeNibble(track & 0x0F);
  buffer[4] = encodeNibble((sector >> 1) & 0x0F);
  buffer[5] = encodeNibble(sector & 0x0F);
  buffer[6] = encodeNibble((checksum >> 1) & 0x0F);
  buffer[7] = encodeNibble(checksum & 0x0F);

  return buffer;
}

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

Byte DiskController::read(Address address, Address normalizedAddress) const
{
  // Disk controller ROM is mapped at $C600-$C6FF
  // Control/status registers are at $C0E0-$C0EF

  if (address < c_slot1Rom)
  {
    // Control/status registers
    Control control{static_cast<Byte>(normalizedAddress)};
    switch (control)
    {
      case Control::Phase0_Off: m_status &= ~Phase0Mask; return updateMotor();
      case Control::Phase0_On: m_status |= Phase0Mask; return updateMotor();
      case Control::Phase1_Off: m_status &= ~Phase1Mask; return updateMotor();
      case Control::Phase1_On: m_status |= Phase1Mask; return updateMotor();
      case Control::Phase2_Off: m_status &= ~Phase2Mask; return updateMotor();
      case Control::Phase2_On: m_status |= Phase2Mask; return updateMotor();
      case Control::Phase3_Off: m_status &= ~Phase3Mask; return updateMotor();
      case Control::Phase3_On: m_status |= Phase3Mask; return updateMotor();
      case Control::Motor_Off: m_status &= ~MotorMask; return updateMotor();
      case Control::Motor_On: m_status |= MotorMask; return updateMotor();
      case Control::SelectDrive0: m_status &= ~DriveSelectMask; return updateMotor();
      case Control::SelectDrive1: m_status |= DriveSelectMask; return updateMotor();
      case Control::Q6Low: m_status &= ~Q6Mask; return handleControlLines();
      case Control::Q6High: m_status |= Q6Mask; return handleControlLines();
      case Control::Q7Low: m_status &= ~Q7Mask; return handleControlLines();
      case Control::Q7High: m_status |= Q7Mask; return handleControlLines();
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

void DiskController::write(Address address, Address normalizedAddress, Byte /*data*/)
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

Byte DiskController::updateMotor() const
{
  if (m_status & MotorMask)
  {
    auto currentPhase = static_cast<uint8_t>(m_status & 0x0f);
    if (std::has_single_bit(currentPhase))
    {
      auto action = static_cast<Byte>((m_lastPhase << 4) | currentPhase);
      auto step = c_stepTable[action];
      m_halfTrack = std::clamp<int8_t>(static_cast<int8_t>(m_halfTrack + step), 0, 68);
      m_lastPhase = currentPhase;
    }
  }
  return m_status;
}

Byte DiskController::handleControlLines() const
{
  switch (m_status & 0xC0)
  {
    case 0x00:  // Read data
      return readDiskData();
    case 0x40:  // Write prep
      return 0;
    case 0x80:  // Check write protect status
      return 0x80;  // bit 7 means write protected
    default:
    case 0xC0:  // Write data
      return 0;
  }
}

Byte DiskController::readDiskData() const
{
  // Return actual disk data when in read mode
  if (!m_diskLoaded || (m_status & MotorMask) == 0)
  {
    return 0x00;  // No data if no disk or motor off
  }

  int track = getCurrentTrack();

  // Reset position if we switched tracks
  if (m_currentTrack != track)
  {
    m_currentTrack = static_cast<Byte>(track);
    m_trackPos = TrackPosition();
  }

  auto lastState = state;
  auto nibble = ((*this).*(state))();
  ++m_trackPos.nibblePos;
  if (state != lastState)
    m_trackPos.step = 0;
  else
    ++m_trackPos.step;
  return nibble;
}

Byte DiskController::getEncodedSectorByte(int track, int sector, int byteIndex) const
{
  // Calculate offset into .dsk file
  size_t diskOffset = calculateDiskOffset(track, sector, byteIndex);

  if (diskOffset < m_diskData.size())
  {
    return encodeNibble(m_diskData[diskOffset]);
  }

  return 0x00;
}

Byte DiskController::AddressPrologue() const
{
  static constexpr Byte prologue[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xD5, 0xAA, 0x96};
  assert(static_cast<size_t>(m_trackPos.step) < std::size(prologue));

  if (static_cast<size_t>(m_trackPos.step) >= std::size(prologue) - 1)
    Transition(&DiskController::AddressData);

  return prologue[m_trackPos.step];
}

Byte DiskController::AddressData() const
{
  // Generate 4-and-4 encoded address field on demand
  static int lastGeneratedSector = -1;
  static int lastGeneratedTrack = -1;

  // Only regenerate if sector or track changed
  if (lastGeneratedSector != m_trackPos.currentSector || lastGeneratedTrack != m_currentTrack)
  {
    m_addressBuffer = generateAddressField(m_currentTrack, m_trackPos.currentSector);
    lastGeneratedSector = m_trackPos.currentSector;
    lastGeneratedTrack = m_currentTrack;
  }

  auto ret = m_addressBuffer[static_cast<size_t>(m_trackPos.step)];
  if (static_cast<size_t>(m_trackPos.step) >= c_addressFieldSize - 1)
    Transition(&DiskController::DataPrologue);

  return ret;
}

Byte DiskController::DataPrologue() const
{
  static constexpr Byte prologue[] = {
      0xDE, 0xAA, 0xEB,  // Data epliogue
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Sync bytes
      0xD5, 0xAA, 0xAD  // Start of data field
  };

  if (static_cast<size_t>(m_trackPos.step) >= std::size(prologue) - 1)
    Transition(&DiskController::DataPayload);

  return prologue[m_trackPos.step];
}

Byte DiskController::DataPayload() const
{
  // Return checksum (simplified for now)
  Byte ret = 0xDE;

  if (m_trackPos.step < 256)
  {
    // Return encoded sector data byte
    ret = getEncodedSectorByte(m_currentTrack, m_trackPos.currentSector, m_trackPos.step);
  }

  if (m_trackPos.step >= 259)  // 256 data + checksum
    Transition(&DiskController::DataEpilogue);

  return ret;
}

Byte DiskController::DataEpilogue() const
{
  static constexpr Byte epilogue[] = {0xDE, 0xAA, 0xEB};

  if (static_cast<size_t>(m_trackPos.step) >= std::size(epilogue) - 1)
  {
    // Move to next sector or end of track
    m_trackPos.currentSector++;
    if (m_trackPos.currentSector < 16)
    {
      Transition(&DiskController::AddressPrologue);
    }
    else
    {
      Transition(&DiskController::TrackGap);
    }
  }

  return epilogue[m_trackPos.step];
}

Byte DiskController::TrackGap() const
{
  if (m_trackPos.nibblePos >= 6656)  // Standard track size
  {
    m_trackPos = TrackPosition();  // Wrap to beginning of track
  }
  return 0xFF;  // Fill rest of track
}

}  // namespace apple2
