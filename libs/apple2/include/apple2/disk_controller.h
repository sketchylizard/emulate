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

  bool loadDisk(const std::string& filename);

  Common::Byte read(Address address);
  void write(Address address, Common::Byte data);

private:
  std::vector<Common::Byte> m_diskData;
  bool m_diskLoaded = false;
  int32_t m_bytesToRead = 0;
  int32_t m_currentTrack = 0;
  int32_t m_currentSector = 0;

  Common::Byte readCurrentSector();
};

}  // namespace apple2
