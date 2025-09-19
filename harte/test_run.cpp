#include "test_run.h"

#include <ranges>

#include "simdjson.h"

Common::Byte SparseMemory::read(Common::Address address)
{
  auto it = std::ranges::find_if(mem, [=](const auto& pair) { return pair.address == address; });
  if (it == mem.end())
  {
    throw std::runtime_error("Memory read of uninitialized address " + std::to_string(static_cast<uint16_t>(address)));
  }
  return it->value;
}

void SparseMemory::write(Common::Address address, Common::Byte data)
{
  // Write
  auto it = std::ranges::find_if(mem, [=](const auto& pair) { return pair.address == address; });
  if (it == mem.end())
  {
    mem.emplace_back(address, data);
  }
  it->value = data;
}

void reportError(std::string_view name, const Snapshot& expected, const Snapshot& actual)
{
  std::cout << "\n" << name << " mismatch\n";
  std::cout << "| | Expected | | Actual   |\n";
  std::cout << "|-|----------|-|----------|\n";
  std::cout << std::format(
      "|PC|{:#04x}| |{:#04x}|\n", static_cast<uint16_t>(expected.regs.pc), static_cast<uint16_t>(actual.regs.pc));
  std::cout << std::format("|A |{:#02x}| |{:#02x}|\n", expected.regs.a, actual.regs.a);
  std::cout << std::format("|X |{:#02x}| |{:#02x}|\n", expected.regs.x, actual.regs.x);
  std::cout << std::format("|Y |{:#02x}| |{:#02x}|\n", expected.regs.y, actual.regs.y);
  std::cout << std::format("|S |{:#02x}| |{:#02x}|\n", expected.regs.sp, actual.regs.sp);

  char buffer[16];
  Common::FixedFormatter formatter(buffer);
  cpu6502::flagsToStr(formatter, expected.regs.p);
  cpu6502::flagsToStr(formatter, actual.regs.p);
  std::cout << std::format("|P |{}| |{}|\n", std::string_view{&buffer[0], 8}, std::string_view{&buffer[8], 8});

  std::cout << "\nMemory differences:\n";
  for (const auto& [addr, val] : expected.memory)
  {
    auto it = std::ranges::find_if(actual.memory, [addr](const auto& pair) { return pair.address == addr; });
    if (it == actual.memory.end())
    {
      std::cout << std::format("Address {:#04x} missing in actual\n", static_cast<uint16_t>(addr));
    }
    else if (it->value != val)
    {
      std::cout << std::format("Address {:#04x} expected {:#02x} got {:#02x}\n", static_cast<uint16_t>(addr), val, it->value);
    }
  }

  std::cout << "\nCycle differences:\n";
  auto minSize = std::min(expected.cycles.size(), actual.cycles.size());
  for (size_t i = 0; i < minSize; ++i)
  {
    const auto& exp = expected.cycles[i];
    const auto& act = actual.cycles[i];
    std::cout << std::format("| Cycle {} | {:04x} : {:02x} {} | {:04x} : {:02x} {} |\n", i,
        static_cast<uint16_t>(exp.address), exp.data, (exp.isRead ? "read" : "write"),
        static_cast<uint16_t>(act.address), act.data, (act.isRead ? "read" : "write"));
  }
}
