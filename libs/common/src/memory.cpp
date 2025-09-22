#include "common/memory.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

#include "common/address.h"

namespace Common
{

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

void Load(std::span<Byte> memory, const std::string& filename, Address start_addr)
{
  std::ifstream file(filename, std::ios::binary);
  if (!file)
  {
    throw std::runtime_error("Cannot open file: " + filename);
  }

  size_t offset = static_cast<size_t>(start_addr);
  if (offset >= memory.size())
  {
    throw std::out_of_range("Start address beyond memory bounds");
  }

  auto bytesToRead = static_cast<std::streamsize>(memory.size() - offset);
  file.read(reinterpret_cast<char*>(memory.data() + offset), bytesToRead);

  if (file.bad())
  {
    throw std::runtime_error("Error reading file: " + filename);
  }
}

}  // namespace Common
