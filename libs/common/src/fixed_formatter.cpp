#include "common/fixed_formatter.h"

namespace Common
{

FixedFormatter::FixedFormatter(std::span<char> buffer) noexcept
  : m_begin(buffer.data())
  , m_current(buffer.data())
  , m_end(buffer.data() + buffer.size())
{
}

FixedFormatter& FixedFormatter::operator<<(char c) noexcept
{
  if (m_current < m_end - 1)
  {  // Leave room for null terminator
    *m_current++ = c;
  }
  return *this;
}

FixedFormatter& FixedFormatter::operator<<(std::string_view str) noexcept
{
  for (auto ch : str)
  {
    if (m_current < m_end - 1)
    {  // Leave room for null terminator
      *m_current++ = ch;
    }
  }
  return *this;
}

FixedFormatter& FixedFormatter::operator<<(Common::Byte value) noexcept
{
  if (m_current < m_end - 2)
  {
    *m_current++ = hexDigits[(value >> 4) & 0x0F];
    *m_current++ = hexDigits[value & 0x0F];
  }
  return *this;
}

FixedFormatter& FixedFormatter::operator<<(Common::Address addr) noexcept
{
  uint16_t value = static_cast<uint16_t>(addr);
  return *this << static_cast<Common::Byte>((value >> 8) & 0xFF) << static_cast<Common::Byte>(value & 0xFF);
}

std::string_view FixedFormatter::finalize() noexcept
{
  return std::string_view(m_begin, m_current);
}

}  // namespace Common
