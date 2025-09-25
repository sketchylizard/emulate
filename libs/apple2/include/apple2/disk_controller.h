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

  static bool loadRom(std::span<const Byte, 256> romData);

  bool loadDisk(const std::string& filename);

  Byte read(Address address, Address normalizedAddress) const override;
  void write(Address address, Address normalizedAddress, Byte data) override;

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

  static Byte c_rom[256];

  std::vector<Byte> m_diskData;
  bool m_diskLoaded = false;
  mutable Byte m_status = 0x00;
  mutable Byte m_lastPhase = 0x00;
  mutable int8_t m_track = 34 * 2;  // Actually half track (0-68)

  Byte readCurrentSector() const;
};

}  // namespace apple2
