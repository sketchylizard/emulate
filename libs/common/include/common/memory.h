#pragma once

#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "common/address.h"

namespace Common
{

//! RAM is just a writable span of bytes
using RamSpan = std::span<Byte>;

//! ROM is a read-only span of bytes
using RomSpan = std::span<const Byte>;

// Load file into memory
std::vector<Byte> LoadFile(const std::string_view& filename) noexcept;

// Load file into memory span. You cannot load into a RomSpan, but you can write into a block of
// bytes and then create a RomSpan over it, presenting a read-only view of the data.
void Load(RamSpan memory, const std::string& filename, Address start_addr = Address{0});

}  // namespace Common
