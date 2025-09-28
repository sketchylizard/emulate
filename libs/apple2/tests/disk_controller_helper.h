#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "apple2/disk_controller.h"
#include "common/address.h"
#include "common/bus.h"

using Common::Address;
using Common::Byte;

namespace apple2
{
// Helper class that provides DOS 3.3-style disk operations
// All operations go through the bus interface like real DOS would
class DiskControllerHelper
{
public:
  explicit DiskControllerHelper(DiskController& controller)
    : m_controller(controller)
  {
  }

  // Seek to track 0 by hitting the mechanical stop
  // This is how Apple DOS 3.3 calibrates - it moves inward many times
  void seekTrack0()
  {
    // Move inward 80 half-tracks to guarantee we hit track 0
    // Real DOS does this by repeatedly activating phases to move toward track 0
    stepInward(80);
    m_halfTrack = 0;
  }

  // Seek to a specific track
  void seekTrack(int32_t targetTrack)
  {
    int32_t targetHalfTrack = targetTrack * 2;

    if (m_halfTrack < targetHalfTrack)
    {
      stepOutward(targetHalfTrack - m_halfTrack);
    }
    else
    {
      stepInward(m_halfTrack - targetHalfTrack);
    }
  }

  // Move one half-track inward (toward track 0)
  void stepInward(int delta)
  {
    assert(delta > 0);

    auto phase = m_halfTrack % 4;
    while (delta-- > 0)
    {
      activatePhase(phase);
      deactivatePhase(phase);
      phase = (phase + 3) % 4;  // Move to next phase inward
      m_halfTrack = std::max<int8_t>(m_halfTrack - 1, 0);
    }
  }

  // Move one half-track outward (away from track 0)
  void stepOutward(int delta)
  {
    assert(delta > 0);
    auto phase = m_halfTrack % 4;
    while (delta-- > 0)
    {
      activatePhase(phase);
      deactivatePhase(phase);
      phase = (phase + 1) % 4;  // Move to next phase outward
      m_halfTrack = std::min<int8_t>(m_halfTrack + 1, 68);
    }
  }

  // Turn motor on (like DOS 3.3 does before seeking)
  void motorOn()
  {
    m_controller.read(Address{0xc0e9}, Address{0x09});  // $C0E9 - Motor on
  }

  // Turn motor off
  void motorOff()
  {
    m_controller.read(Address{0xc0e8}, Address{0x08});  // $C0E8 - Motor off
  }

  // Select drive (0 or 1)
  void selectDrive(int drive)
  {
    if (drive == 0)
      m_controller.read(Address{0xc0ea}, Address{0x0A});  // $C0EA - Drive 1
    else
      m_controller.read(Address{0xc0eb}, Address{0x0B});  // $C0EB - Drive 2
  }

private:
  DiskController& m_controller;
  int8_t m_halfTrack{34 * 2};  // Start at track 34 (max track)

  void activatePhase(int phase)
  {
    if (phase >= 0 && phase < 4)
    {
      // phase on values are odd offsets from $C0E0
      uint16_t offset = static_cast<uint16_t>(phase * 2) + 1;
      Address base{static_cast<uint16_t>(0xc0e0 + offset)};
      m_controller.read(base, Address{offset});
    }
  }

  void deactivatePhase(int phase)
  {
    if (phase >= 0 && phase < 4)
    {
      uint16_t offset = static_cast<uint16_t>(phase * 2);
      Address base{static_cast<uint16_t>(0xc0e0 + offset)};
      m_controller.read(base, Address{offset});
    }
  }
};

}  // namespace apple2
