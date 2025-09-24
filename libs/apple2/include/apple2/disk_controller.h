#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "common/address.h"

namespace apple2
{

class DiskController
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;

  static bool loadRom(std::span<const Byte, 256> romData);

  bool loadDisk(const std::string& filename);

  Byte read(Address address) const;
  void write(Address address, Byte data);

private:
  static Byte c_rom[256];

  std::vector<Byte> m_diskData;
  bool m_diskLoaded = false;
  int32_t m_bytesToRead = 0;
  int32_t m_currentTrack = 0;
  int32_t m_currentSector = 0;

  Byte readCurrentSector() const;
};

}  // namespace apple2
