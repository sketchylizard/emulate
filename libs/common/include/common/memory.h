#pragma once

#include <span>
#include <string>
#include <vector>

#include "common/address.h"

namespace Common
{

using RamSpan = std::span<Byte>;
using RomSpan = std::span<const Byte>;

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span
void Load(RamSpan memory, const std::string& filename, Address start_addr = Address{0});

}  // namespace Common
