#include "apple2/apple2system.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <span>

#include "common/bus.h"
#include "common/memory.h"
#include "cpu6502/mos6502.h"

namespace apple2
{

Apple2System::Apple2System(std::span<Common::Byte, 0xc000> memory, std::span<const Common::Byte, 0x2000> rom,
    std::span<Common::Byte, 0x1000> langBank0, std::span<Common::Byte, 0x1000> langBank1)
  : m_languageCard({0xD000, std::span(langBank0), std::span(langBank1)})
  , m_bus(RamDevice(memory, Address{0x0000}),  //
        m_languageCard,  //
        RomDevice(std::as_const(rom), Address{0xe000}))
{
}

void Apple2System::reset()
{
  // Read reset vector from $FFFC-$FFFD
  Common::Byte lowByte = m_bus.read(Address{0xFFFC});
  Common::Byte highByte = m_bus.read(Address{0xFFFD});
  Address resetAddress = Address{static_cast<uint16_t>((highByte << 8) | lowByte)};

  // Set CPU state
  m_cpu.registers.pc = resetAddress;
  m_cpu.set(Processor::Flag::Interrupt, true);  // Disable interrupts
  m_cpu.registers.sp = 0xFF;  // Initialize stack pointer

  // Reset the microcode pump
  m_pump = MicrocodePump<Processor>();
}

bool Apple2System::clock()
{
  return m_pump.tick(m_cpu, Processor::BusToken{&m_bus});
}

void Apple2System::step()
{
  // Execute one instruction (multiple clocks)
  do
  {
    // Keep clocking until instruction completes
  } while (clock());
}

void Apple2System::run(size_t maxCycles)
{
  size_t cycles = 0;
  while (cycles < maxCycles)
  {
    if (!clock())
    {
      break;  // Instruction completed
    }
    ++cycles;
  }
}

}  // namespace apple2
