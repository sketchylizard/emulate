#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include "apple2/disk_controller.h"
#include "apple2/iodevice.h"
#include "apple2/text_video_device.h"
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

  template<size_t Size>
  using RamSpan = std::span<Byte, Size>;
  template<size_t Size>
  using RomSpan = std::span<const Byte, Size>;

  Apple2System(RamSpan<0xc000> memory, RomSpan<0x3000> rom, RamSpan<0x1000> langBank0, RamSpan<0x1000> langBank1);

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

  void pressKey(char c);

  bool isScreenDirty() const noexcept
  {
    return m_textVideo.isDirty();
  }

  TextVideoDevice::Screen getScreen() const noexcept
  {
    return m_textVideo.screen();
  }

  // Load a disk image:
  bool loadDisk(const std::string& filename)
  {
    return m_disk.loadDisk(filename);
  }

  void loadPeripheralRom(int slot, RomSpan<0x100> romData)
  {
    if (slot < 0 || slot >= 8)
      throw std::out_of_range("Slot number must be between 0 and 7");
    assert(slot == 6);  // Currently only slot 6 (disk controller) is supported
    m_disk.loadRom(romData);
  }

private:
  using RamDevice = Common::MemoryDevice<Common::Byte>;
  using RomDevice = Common::MemoryDevice<const Common::Byte>;
  using LanguageCardDevice = Common::BankSwitcher<2, 0x1000>;

  using Apple2Bus = Common::Bus<Processor>;

  void updateKeyboard();
  char appleToAscii(Byte data) const noexcept;

  void setupIoHandlers();
  Byte handleKeyboardRead(Address address);
  Byte handleKeyboardStrobeRead(Address address);
  Byte handleLanguageCardRead(Address address);
  void handleLanguageCardWrite(Address address, Byte data);
  Byte handleSpeakerRead(Address address);
  void handleSpeakerWrite(Address address, Byte data);

  Processor m_cpu;
  MicrocodePump<Processor> m_pump;

  // Memory and devices
  TextVideoDevice m_textVideo;
  RamDevice m_ram;
  RomDevice m_rom;
  IoDevice m_io;
  LanguageCardDevice m_languageCard;
  DiskController m_disk;

  Apple2Bus m_bus;

  // I/O state
  std::queue<char> m_keyBuffer;
  Common::Byte m_keyboardData = 0x00;
};

}  // namespace apple2
