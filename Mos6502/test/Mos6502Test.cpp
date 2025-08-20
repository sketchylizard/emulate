#include "Mos6502/Mos6502.h"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>  // for std::byte
#include <cstdint>  // for std::uint8_t
#include <utility>  // for std::pair
#include <variant>  // for std::variant

#include "common/Bus.h"
#include "common/Memory.h"
#include "util/hex.h"

using namespace Common;

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

  std::optional<BusResponse> Tick(const Bus& input)
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

  std::optional<BusResponse> Tick(const Bus& input)
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

TEST_CASE("Mos6502: ADC Immediate", "[cpu][adc]")
{
  // Setup
  // Write instruction to memory
  auto program = R"(
    69 22  ; ADC #$22
  )"_hex;

  Byte ram[0xFFFF];

  Mapping memoryMap[] = {
      // Put the ROM in the map first so it will be found first and hide the memory.
      {Address{0x1000}, Address{0x1002}, RomSpan{program}},  // ROM
      {Address{0x0000}, Address{0xFFFF}, RamSpan{ram}},  // RAM
  };

  SimpleMemoryMap memory = {memoryMap};

  Bus bus = {};

  Mos6502 cpu;
  cpu.set_a(0x10);  // A = 0x10
  cpu.set_status(0);  // Make sure Carry is cleared (C = 0)

  auto tick = [&](Bus bus)
  {
    bus = cpu.Tick(bus);
    auto response = memory.Tick(bus);
    if (response)
    {
      bus.data = response->data;
    }
    return bus;
  };

  auto programStart = Address{0x1000};

  // Set the reset vector to 0x1000
  memory[Mos6502::c_brkVector] = LoByte(programStart);
  memory[Mos6502::c_brkVector + 1] = HiByte(programStart);

  // Execute (it takes 7 cycles for the reset + 2 for ADC immediate)
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  // Reset should be done
  bus = tick(bus);
  bus = tick(bus);
  bus = tick(bus);
  // ADC should be done

  // Verify result: 0x10 + 0x22 + 0 = 0x32
  CHECK(cpu.a() == 0x32);
  // CHECK_FALSE(cpu.status() & CarryFlag);
  // CHECK_FALSE(cpu.status() & OverflowFlag);
  // CHECK_FALSE(cpu.status() & ZeroFlag);
  // CHECK_FALSE(cpu.status() & NegativeFlag);
}

TEST_CASE("Mos6502: Functional_tests")
{
  auto file = LoadFile("/home/jason/projects/6502_65C02_functional_tests/bin_files/6502_functional_test.bin");

  Mapping memory{Address{0x0000}, Address{0xFFFF}, RamSpan{file}};

  Bus bus = {};

  Mos6502 cpu;

  auto step = [&](Bus bus)
  {
    do
    {
      bus = cpu.Tick(bus);
      auto response = memory.Tick(bus);
      if (response)
      {
        bus.data = response->data;
      }
    } while (!(bus.control & Control::Sync));
    return bus;
  };

  auto programStart = Address{0x0400};

  // Set the reset vector to 0x0400
  cpu.set_pc(programStart);

  Address lastProgramCounter = programStart;

  for (int i = 0; i < 0xffff; ++i)
  {
    bus = step(bus);
    if (cpu.pc() == lastProgramCounter)
    {
      // If the PC hasn't changed, the program might be stuck; break to avoid infinite loop
      std::cout << "Program counter stuck at: " << std::hex << static_cast<uint16_t>(cpu.pc()) << "\n";
      break;
    }
    lastProgramCounter = cpu.pc();
  }
}
