#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "common/address.h"
#include "common/bus.h"

namespace apple2
{

class DiskController : public Common::Bus::Device
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;

  friend class DiskControllerHelper;

  static constexpr int8_t c_maxTracks{35};

  static bool loadRom(std::span<const Byte, 256> romData);

  bool loadDisk(const std::string& filename);

  Byte read(Address address, Address normalizedAddress) const override;
  void write(Address address, Address normalizedAddress, Byte data) override;

  // Return actual track (0-34), rounds down if on half track.
  int getCurrentTrack() const
  {
    return m_halfTrack / 2;
  }

  Byte readStatus() const
  {
    return m_status;
  }

  bool isMotorOn() const
  {
    return (m_status & MotorMask) != 0;
  }

private:
  // All controller cards use ROM memory in the range of $C0n0-$C0nF where n is the slot number.
  // The disk controller is typically in slot 6, so its ROM is at $C600-$C6FF.
  // The slot's corresponding I/O space is at $C0x0-$C0xF, where x = slot number + 8.
  static constexpr Address c_slot1Rom{0xC100};

  enum class Control : Byte
  {
    Phase0_Off = 0,
    Phase0_On = 1,
    Phase1_Off = 2,
    Phase1_On = 3,
    Phase2_Off = 4,
    Phase2_On = 5,
    Phase3_Off = 6,
    Phase3_On = 7,
    Motor_Off = 8,
    Motor_On = 9,
    SelectDrive0 = 0x0A,
    SelectDrive1 = 0x0B,
    Q6Low = 0x0C,
    Q6High = 0x0D,
    Q7Low = 0x0E,
    Q7High = 0x0F
  };

  static constexpr Byte Phase0Mask{0x01};
  static constexpr Byte Phase1Mask{0x02};
  static constexpr Byte Phase2Mask{0x04};
  static constexpr Byte Phase3Mask{0x08};
  static constexpr Byte MotorMask{0x10};
  static constexpr Byte DriveSelectMask{0x20};
  static constexpr Byte Q6Mask{0x40};
  static constexpr Byte Q7Mask{0x80};

  // Size of encoded address fields: 4 fields × 2 nibbles
  static constexpr size_t c_addressFieldSize = 4 * 2;

  using State = Byte (DiskController::*)() const;

  struct TrackPosition
  {
    int16_t step = 0;  // Step within current state
    int16_t nibblePos = 0;  // Overall position in 6656-byte track
    Byte sectorIndex = 0;  // Which sector (0-15)
    State state = &DiskController::AddressPrologue;
  };

  static Byte c_rom[256];

  std::vector<Byte> m_diskData;
  bool m_diskLoaded = false;
  mutable Byte m_status = 0x00;
  mutable Byte m_lastPhase = 0x00;
  mutable int8_t m_halfTrack = 34 * 2;
  // Track currently being read
  mutable Byte m_currentTrack = 0xff;
  mutable TrackPosition m_trackPos;
  mutable Byte m_lastGeneratedTrack = 0xff;
  mutable Byte m_lastGeneratedSector = 0xff;
  mutable std::array<Byte, c_addressFieldSize> m_addressBuffer{};

  Byte updateMotor() const;
  Byte handleControlLines() const;
  Byte readDiskData() const;
  Byte getEncodedSectorByte(int track, int sector, int byteIndex) const;

  // State functions
  void Transition(State newState) const
  {
    m_trackPos.state = newState;
  }

  // Sync Bytes (FF) D5 AA 96
  Byte AddressPrologue() const;
  // Volume, track, sector, checksum (4-and-4 encoded)
  Byte AddressData() const;
  // DE AA EB + sync bytes + D5 AA AD
  Byte DataPrologue() const;
  // 256 bytes of sector data (encoded)
  Byte DataPayload() const;
  // DE AA EB
  Byte DataEpilogue() const;
  // Fill to end of track
  Byte TrackGap() const;
};

}  // namespace apple2
