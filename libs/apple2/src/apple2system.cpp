#include "apple2/apple2system.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <span>

#include "common/bus.h"
#include "common/logger.h"
#include "common/memory.h"
#include "cpu6502/mos6502.h"

namespace apple2
{

using Common::Bus;

Apple2System::Apple2System(  //
    RamSpan<0xc000> memory,  // Main memory
    RomSpan<0x3000> rom,  // upper ROM
    RamSpan<0x1000> langBank0,  // language card bank 0
    RamSpan<0x1000> langBank1  // language card bank 1
    )
  : m_textVideo(RamSpan<0x400>(memory.data() + 0x400, 0x400))  // $0400-$07FF
  , m_ram(memory)
  , m_rom(rom)
  , m_io(this)
  , m_languageCard(std::span(langBank0), std::span(langBank1))
  , m_bus{{
        Bus::Entry{Address{0x0400}, Address{0x07FF}, &m_textVideo},
        Bus::Entry{Address{0xC0E0}, Address{0xC0EF}, &m_disk},  // Disk (slot 6, control switches)
        Bus::Entry{Address{0xC600}, Address{0xC6FF}, &m_disk},  // Disk (slot 6, ROM)
        Bus::Entry{Address{0xC000}, Address{0xC0FF}, &m_io},  // I/O and soft switches
        Bus::Entry{Address{0x0000}, Address{0x0000}, &m_languageCard},
        Bus::Entry{Address{0x0000}, Address{0xBFFF}, &m_ram},
        Bus::Entry{Address{0xD000}, Address{0xFFFF}, &m_rom},
    }}
{
  setupIoHandlers();
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

void Apple2System::updateKeyboard()
{
  if (!m_keyBuffer.empty() && (m_keyboardData & 0x80) == 0)
  {
    // Get next key and set ready flag
    char key = m_keyBuffer.front();
    m_keyBuffer.pop();
    m_keyboardData = static_cast<Common::Byte>(key & 0x7F) | 0x80;
  }
}

void Apple2System::pressKey(char c)
{
  // Convert to uppercase and store in buffer
  if (c >= 'a' && c <= 'z')
  {
    c = c - 'a' + 'A';
  }
  m_keyBuffer.push(c);
}

char Apple2System::appleToAscii(Byte data) const noexcept
{
  // Apple II has some encoding quirks
  Byte c = data & 0x7F;  // Strip high bit
  if (c >= 0x20 && c <= 0x5F)
    return static_cast<char>(c);
  if (c >= 0x60 && c <= 0x7F)
    return static_cast<char>(c - 0x40);  // Lowercase
  return ' ';  // Default for control chars
}

void Apple2System::setupIoHandlers()
{
  // Individual addresses (same as before)
  m_io.registerReadHandler(Address{0xC000}, &Apple2System::handleKeyboardRead);
  m_io.registerReadHandler(Address{0xC010}, &Apple2System::handleKeyboardStrobeRead);

  // Speaker I/O
  m_io.registerReadHandler(Address{0xC030}, &Apple2System::handleSpeakerRead);
  m_io.registerWriteHandler(Address{0xC030}, &Apple2System::handleSpeakerWrite);

  // Range registrations - much simpler now!
  // Language card soft switches - ALL 16 addresses call the same handler
  m_io.registerReadRange(Address{0xC080}, Address{0xC08F}, &Apple2System::handleLanguageCardRead);
  m_io.registerWriteRange(Address{0xC080}, Address{0xC08F}, &Apple2System::handleLanguageCardWrite);

  // Future additions would be equally simple:
  // m_io.registerReadRange(Address{0xC020}, Address{0xC02F}, &Apple2System::handleCassetteRead);
  // m_io.registerWriteRange(Address{0xC020}, Address{0xC02F}, &Apple2System::handleCassetteWrite);

  // Graphics soft switches
  // m_io.registerReadRange(Address{0xC050}, Address{0xC05F}, &Apple2System::handleGraphicsRead);
  // m_io.registerWriteRange(Address{0xC050}, Address{0xC05F}, &Apple2System::handleGraphicsWrite);
}

Common::Byte Apple2System::handleKeyboardRead(Address /*address*/)
{
  updateKeyboard();
  return m_keyboardData;
}

Common::Byte Apple2System::handleKeyboardStrobeRead(Address /*address*/)
{
  // Reading the strobe clears the key ready flag (bit 7)
  m_keyboardData &= 0x7F;
  return m_keyboardData;
}

// The handler functions examine the specific address to determine behavior
Common::Byte Apple2System::handleLanguageCardRead(Common::Address address)
{
  uint16_t addr = static_cast<uint16_t>(address);
  uint16_t offset = addr - static_cast<uint16_t>(0xC080);  // 0-15 for $C080-$C08F

  // All 16 addresses in the range call this same function
  // The function examines the specific address to determine what to do

  bool selectBank1 = (offset & 0x01) != 0;  // Odd addresses = bank 1
  bool readFromLC = (offset & 0x08) != 0;  // $C088-$C08F = LC mode

  if (selectBank1)
  {
    m_languageCard.selectBank(0);
  }
  else
  {
    m_languageCard.selectBank(1);
  }

  std::cout << "[DEBUG] Language card: " << (readFromLC ? "LC mode" : "ROM mode") << " bank "
            << (selectBank1 ? "1" : "2") << " at $" << std::hex << addr << std::dec << "\n";

  return 0xFF;  // Open bus
}

void Apple2System::handleLanguageCardWrite(Address address, Byte data)
{
  static_cast<void>(data);  // Unused parameter

  // Same logic as read - writes to language card switches trigger bank changes
  handleLanguageCardRead(address);  // Reuse the read logic
}

Common::Byte Apple2System::handleSpeakerRead(Address /*address*/)
{
  //  std::cout << "tick\n";
  return 0x00;  // Open bus
}

void Apple2System::handleSpeakerWrite(Address /*address*/, Byte /*data*/)
{
  handleSpeakerRead(Address{0});  // use the same logic as the read.
}

}  // namespace apple2
