#pragma once

#include <cstdint>
#include <span>

#include "common/bus.h"

namespace apple2
{
class TextVideoDevice
{
public:
  using Address = Common::Address;
  using Byte = Common::Byte;
  using Line = std::span<char, 40>;
  using Screen = std::array<Line, 24>;

  static constexpr size_t c_size{0x0400};  // 1KB
  static constexpr Address c_baseAddress{0x0400};

  explicit TextVideoDevice(std::span<Byte, c_size> videoMemory) noexcept;

  // Bus interface methods
  Byte read(Address address) const;
  void write(Address address, Byte value);

  bool isDirty() const noexcept
  {
    return m_dirty;
  }

  const Screen& screen() const noexcept;

private:
  std::span<Byte, c_size> m_videoMemory;
  Screen m_screen;
  mutable bool m_dirty = true;
};

}  // namespace apple2
