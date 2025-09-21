#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include "common/address.h"
#include "common/bank_switcher.h"
#include "common/memory.h"
#include "cpu6502/cpu6502_types.h"
#include "cpu6502/mos6502.h"

namespace apple2
{

class Apple2System
{
public:
  using Processor = cpu6502::mos6502;
  using Address = Common::Address;
  using Byte = Common::Byte;


  Apple2System(std::span<Common::Byte, 0xc000> memory, std::span<const Common::Byte, 0x2000> rom,
      std::span<Common::Byte, 0x1000> langBank0, std::span<Common::Byte, 0x1000> langBank1);

  ~Apple2System() = default;

  void reset();

  // Returns true if instruction is still executing, false if instruction completed
  bool clock();

  // Execute one instruction
  void step();

  // Execute until some condition (useful for debugging)
  void run(size_t maxCycles = 1000000);

  // CPU state access
  const auto& cpu() const
  {
    return m_cpu;
  }

private:
  using RamDevice = Common::MemoryDevice<Common::Byte>;
  using RomDevice = Common::MemoryDevice<const Common::Byte>;
  using LanguageCardDevice = Common::BankSwitcher<2, 0x1000>;

  using Apple2Bus = Common::Bus<Processor,
      RamDevice,  // Lower RAM
      LanguageCardDevice,  // Language Card
      RomDevice>;  // Upper ROM

  Processor m_cpu;
  MicrocodePump<Processor> m_pump;
  LanguageCardDevice m_languageCard;

  Apple2Bus m_bus;
};

}  // namespace apple2
