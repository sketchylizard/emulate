#include "Mos6502/Memory.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

std::vector<Byte> LoadFile(const std::string_view& filename) noexcept
{
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file)
  {
    // Handle error
    return {};
  }

  // Read the file contents into a buffer
  std::vector<Byte> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return buffer;
}
