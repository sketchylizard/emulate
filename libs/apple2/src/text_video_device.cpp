#include "apple2/text_video_device.h"

namespace apple2
{

namespace
{

auto getRow(std::span<Common::Byte, TextVideoDevice::c_size> videoMemory, size_t row)
{
  // Each group of 8 rows is interleaved in memory
  size_t group = row / 8;
  size_t lineInGroup = row % 8;
  // Apple II memory layout: group offset + line spacing
  auto offset = (group * 0x28) + (lineInGroup * 0x80);

  return std::span<char, 40>(reinterpret_cast<char*>(videoMemory.data()) + offset, 40);
}

auto initializeScreen(std::span<Common::Byte, TextVideoDevice::c_size> videoMemory)
{
  // Initialize screen lines to point into video memory. The Apple 2 text screen is 24 lines of 40
  // characters each, but they are interleaved in memory. They are organized in three groups, with 8
  // lines in each group. The lines of each group are interleaved, so they are (0, 8, 16, 1, 9, 17,
  // 2, 10, 18, ...).

  std::array<TextVideoDevice::Line, 24> screen{getRow(videoMemory, 0), getRow(videoMemory, 1), getRow(videoMemory, 2),
      getRow(videoMemory, 3), getRow(videoMemory, 4), getRow(videoMemory, 5), getRow(videoMemory, 6), getRow(videoMemory, 7),
      getRow(videoMemory, 8), getRow(videoMemory, 9), getRow(videoMemory, 10), getRow(videoMemory, 11),
      getRow(videoMemory, 12), getRow(videoMemory, 13), getRow(videoMemory, 14), getRow(videoMemory, 15),
      getRow(videoMemory, 16), getRow(videoMemory, 17), getRow(videoMemory, 18), getRow(videoMemory, 19),
      getRow(videoMemory, 20), getRow(videoMemory, 21), getRow(videoMemory, 22), getRow(videoMemory, 23)};

  return screen;
}
}  // namespace

TextVideoDevice::TextVideoDevice(std::span<Byte, c_size> videoMemory) noexcept
  : m_videoMemory(videoMemory)
  , m_screen(initializeScreen(m_videoMemory))
{
}

Common::Byte TextVideoDevice::read(Address address) const
{
  assert(address >= c_baseAddress);

  size_t index = static_cast<size_t>(address) - c_size;
  if (index < m_videoMemory.size())
  {
    return m_videoMemory[index];
  }
  return 0;  // Out of range reads return 0
}

void TextVideoDevice::write(Address address, Byte value)
{
  assert(address >= c_baseAddress);

  m_dirty = true;

  size_t index = static_cast<size_t>(address) - c_size;
  if (index < m_videoMemory.size())
  {
    m_videoMemory[index] = value;
  }
}

const TextVideoDevice::Screen& TextVideoDevice::screen() const noexcept
{
  m_dirty = false;
  return m_screen;
}

}  // namespace apple2
