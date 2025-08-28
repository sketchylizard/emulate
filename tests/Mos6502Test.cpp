#include <bitset>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte
#include <cstdint>  // for std::uint8_t
#include <utility>  // for std::pair
#include <variant>  // for std::variant
#include <vector>  // for std::variant

#include "KlausFunctional.h"
#include "common/Bus.h"
#include "common/address.h"
#include "common/hex.h"
#include "common/memory.h"
#include "core65xx/core65xx.h"

using namespace Common;

#if 0

struct Mapping
{
  Address start;
  Address end;
  std::variant<RamSpan, RomSpan> span;

  // Constructors
  Mapping(Address start_addr, Address end_addr, RamSpan ram)
    : start(start_addr)
    , end(end_addr)
    , span(ram)
  {
  }

  Mapping(Address start_addr, Address end_addr, RomSpan rom)
    : start(start_addr)
    , end(end_addr)
    , span(rom)
  {
  }

  // Convenience constructor - deduce end from start + span size
  Mapping(Address start_addr, RamSpan ram)
    : start(start_addr)
    , end(start_addr + static_cast<uint16_t>(ram.size() - 1))
    , span(ram)
  {
  }

  Mapping(Address start_addr, RomSpan rom)
    : start(start_addr)
    , end(start_addr + static_cast<uint16_t>(rom.size() - 1))
    , span(rom)
  {
  }

  bool Contains(Address addr) const
  {
    return addr >= start && addr <= end;
  }

  size_t GetOffset(Address addr) const
  {
    return static_cast<size_t>(addr - start);
  }

  bool IsWritable() const
  {
    return std::holds_alternative<RamSpan>(span);
  }

  // Direct access methods
  Byte& operator[](Address addr)
  {
    if (!Contains(addr))
    {
      throw std::out_of_range("Address not in mapping range");
    }

    size_t offset = GetOffset(addr);
    return std::visit(
        [offset](auto& s) -> Byte&
        {
          using T = std::decay_t<decltype(s)>;
          if constexpr (std::is_same_v<T, RamSpan>)
          {
            return s[offset];
          }
          else
          {
            throw std::runtime_error("Attempt to get writable reference to ROM");
          }
        },
        span);
  }

  Byte operator[](Address addr) const
  {
    if (!Contains(addr))
    {
      throw std::out_of_range("Address not in mapping range");
    }

    size_t offset = GetOffset(addr);
    return std::visit([offset](const auto& s) -> Byte { return s[offset]; }, span);
  }

  Byte Read(Address addr) const
  {
    if (!Contains(addr))
    {
      throw std::out_of_range("Address not in mapping range");
    }

    size_t offset = GetOffset(addr);
    return std::visit([offset](const auto& s) -> Byte { return s[offset]; }, span);
  }

  void Write(Address addr, Byte value)
  {
    if (!Contains(addr) || !IsWritable())
    {
      throw std::out_of_range("Address not in mapping range");
    }

    size_t offset = GetOffset(addr);
    std::get<RamSpan>(span)[offset] = value;
  }

  std::optional<BusResponse> Tick(const BusRequest& input)
  {
    if (!Contains(input.address))
    {
      return std::nullopt;  // Not our address
    }

    if (input.isRead())
    {
      size_t offset = GetOffset(input.address);
      Byte data = std::visit([offset](const auto& s) -> Byte { return s[offset]; }, span);
      return BusResponse{data, 0};  // Return data, no control flags
    }
    else if (input.isWrite() && IsWritable())
    {
      size_t offset = GetOffset(input.address);
      std::get<RamSpan>(span)[offset] = input.data;
      return BusResponse{0, 0};  // Write acknowledged
    }

    return std::nullopt;  // Unknown operation
  }
};

class SimpleMemoryMap
{
public:
  SimpleMemoryMap(std::span<Mapping> mappings)
    : m_mappings(mappings)
  {
  }

  // Non-const operator[] - returns reference for read/write access
  Byte& operator[](Address addr)
  {
    if (auto* mapping = FindWriteable(addr))
    {
      return (*mapping)[addr];
    }
    throw std::runtime_error(
        "Attempt to get writable reference to ROM at address " + std::to_string(static_cast<uint16_t>(addr)));
  }

  // Const operator[] - returns value for read-only access
  Byte operator[](Address addr) const
  {
    if (const auto* mapping = Find(addr))
    {
      return (*mapping)[addr];
    }
    throw std::out_of_range("Address " + std::to_string(static_cast<uint16_t>(addr)) + " is not mapped");
  }

  Byte Read(Address addr) const
  {
    if (const auto* mapping = Find(addr))
    {
      return (*mapping)[addr];
    }
    throw std::out_of_range("Address " + std::to_string(static_cast<uint16_t>(addr)) + " is not mapped");
  }

  void Write(Address addr, Byte value)
  {
    if (auto* mapping = FindWriteable(addr))
    {
      (*mapping)[addr] = value;
      return;
    }
    throw std::out_of_range("Address " + std::to_string(static_cast<uint16_t>(addr)) + " is not writable");
  }

  std::optional<BusResponse> Tick(const BusRequest& input)
  {
    for (auto& mapping : m_mappings)
    {
      if (auto response = mapping.Tick(input))
      {
        return response;  // First mapping that handles the address wins
      }
    }
    return std::nullopt;  // Address not mapped to any component
  }

private:
  const Mapping* Find(Address addr) const
  {
    for (const auto& mapping : m_mappings)
    {
      if (mapping.Contains(addr))
      {
        return &mapping;
      }
    }
    return nullptr;
  }

  Mapping* FindWriteable(Address addr)
  {
    for (auto& mapping : m_mappings)
    {
      if (mapping.Contains(addr) && std::holds_alternative<RamSpan>(mapping.span))
      {
        return &mapping;
      }
    }
    return nullptr;
  }

  std::span<Mapping> m_mappings;
};

TEST_CASE("Core65xx: Functional_tests", "[.]")
{
  auto runTest = []()
  {
    auto data = Klaus__6502_functional_test::data();  // Ensure the data is linked in

    std::vector<Byte> ram(data.begin(), data.end());  // 64 KiB RAM initialized to zero
    Mapping memory{Address{0x0000}, Address{0xFFFF}, std::span<Byte>(ram)};

    Core65xx cpu(mos6502::GetInstructions());

    auto programStart = Address{0x0400};

    // Set the reset vector to 0x0400
    cpu.regs.pc = programStart;

    BusRequest request;
    BusResponse response;

    std::bitset<0x10000> breakpoints;
    breakpoints.set(static_cast<size_t>(0x05a7));
    breakpoints.set(static_cast<size_t>(0x0594));

    try
    {
      Address lastInstructionStart{0xFFFF};

      for (;;)
      {
        do
        {
          request = cpu.Tick(response);

          auto newResponse = memory.Tick(request);
          if (newResponse)
          {
            response = *newResponse;
          }
        } while (!request.isSync());

        // We are at an instruction boundary. This is a good place to check for infinite loops or breakpoints.

        if (request.address == lastInstructionStart)
        {
          // See if it is one of the known trap points
          if (request.address == Klaus__6502_functional_test::success)
          {
            return true;  // Test passed
          }
          else if (std::ranges::find(Klaus__6502_functional_test::errors, request.address) !=
                   std::end(Klaus__6502_functional_test::errors))
          {
            throw std::runtime_error(std::format("Test failed at address ${:04X}\n", request.address));
          }
          else
          {
            throw std::runtime_error(std::format("Infinite loop detected at address ${:04X}\n", request.address));
          }
        }
        lastInstructionStart = request.address;

        // See if the requested address is a breakpoint
        if (breakpoints.test(static_cast<size_t>(request.address)))
        {
          std::cout << std::format("User breakpoint at address ${:04X}\n", request.address);
        }
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << '\n';
      return false;
    }
  };
  CHECK(runTest() == true);
}

#endif
