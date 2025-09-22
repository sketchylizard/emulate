// main.cpp or test_apple2.cpp
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "apple2/apple2system.h"

using namespace Common;

namespace
{
char appleToAscii(Byte data)
{
  // Apple II character set conversion
  Byte c = data & 0x7F;  // Strip high bit

  // Normal ASCII range
  if (c >= 0x20 && c <= 0x5F)
    return static_cast<char>(c);

  // Apple II "flashing" characters (high bit set, bits 6-5 = 00)
  if ((data & 0xC0) == 0x80)
    return static_cast<char>(c);

  // Apple II inverse characters (bits 7-6 = 00)
  if ((data & 0xC0) == 0x00)
    return static_cast<char>(c + 0x40);

  // Control characters
  if (c < 0x20)
    return ' ';  // Convert controls to space for now

  return static_cast<char>(c);
}

class KeyboardHandler
{
public:
  KeyboardHandler()
  {
    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &m_originalTermios);

    // Set raw mode
    struct termios raw = m_originalTermios;
    raw.c_lflag &= static_cast<uint32_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;  // Non-blocking
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }

  ~KeyboardHandler()
  {
    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &m_originalTermios);
  }

  std::optional<char> getKey()
  {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
    {
      return c;
    }
    return std::nullopt;
  }

private:
  struct termios m_originalTermios;
};

}  // namespace

int main()
{
  try
  {
    std::cout << "Creating Apple II system...\n";

    Byte ram[0xc000] = {};  // 48KB RAM
    Byte rom[0x3000] = {};  // 12KB ROM (placeholder
    Byte langBank0[0x1000] = {};  // Language Card Bank 0 (4KB)
    Byte langBank1[0x1000] = {};  // Language Card Bank

    KeyboardHandler keyboard;

    Common::Load(std::span(rom), "/home/jason/Downloads/apple.rom");

    apple2::Apple2System system{std::span(ram), std::span(rom), std::span(langBank0), std::span(langBank1)};

    std::cout << "System created successfully!\n";

    system.reset();

    Address resetVector = Common::MakeAddress(rom[0x2ffc], rom[0x2ffd]);
    std::cout << "Reset vector: $" << std::hex << static_cast<uint16_t>(resetVector) << std::dec << "\n";

    // Set up a loop that runs roughly at 1MHz (1 microsecond per cycle)

    while (true)
    {
      auto start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i != 16667; ++i)  // Roughly 1MHz
      {
        system.clock();
      }
      auto elapsed = std::chrono::high_resolution_clock::now() - start;
      if (elapsed < std::chrono::microseconds(16'667))
      {
        std::this_thread::sleep_for(std::chrono::microseconds(16'667) - (elapsed));
      }
      if (auto key = keyboard.getKey(); key.has_value())
      {
        auto c = key.value();
        if (c == 27)
          break;  // ESC to quit

        // Convert Enter to carriage return for Apple II
        if (c == '\n' || c == '\r')
        {
          c = '\r';  // Apple II expects CR (0x0D)
        }
        system.pressKey(c);
      }

      if (system.isScreenDirty())
      {
        auto screen = system.getScreen();
        std::cout << "\033[2J\033[H";  // Clear screen, move cursor to top
        std::cout << "\n--- Screen Update ---\n";
        for (const auto& line : screen)
        {
          for (char c : line)
          {
            std::cout << appleToAscii(static_cast<Byte>(c));
          }
          std::cout << "\n";
        }
      }
    }

    //    std::cout << "Test completed successfully!\n";
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
