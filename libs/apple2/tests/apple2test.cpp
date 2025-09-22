// main.cpp or test_apple2.cpp
#include <iostream>
#include <thread>

#include "apple2/apple2system.h"

using namespace Common;

int main()
{
  try
  {
    std::cout << "Creating Apple II system...\n";

    Byte ram[0xc000] = {};  // 48KB RAM
    Byte rom[0x3000] = {};  // 12KB ROM (placeholder
    Byte langBank0[0x1000] = {};  // Language Card Bank 0 (4KB)
    Byte langBank1[0x1000] = {};  // Language Card Bank

    Common::Load(std::span(rom), "/home/jason/Downloads/appleiigo.rom");

    // Write a simple test program to memory
    // Write a test program that outputs "HELLO" and then waits for key input
    rom[0x1FFC] = 0x00;  // Reset vector low byte
    rom[0x1FFD] = 0xd0;  // Reset vector high byte

    apple2::Apple2System system{std::span(ram), std::span(rom), std::span(langBank0), std::span(langBank1)};

    std::cout << "System created successfully!\n";
    std::cout << "Starting execution (will print HELLO then echo keys)...\n";
    std::cout << "Type characters (they will be echoed), Ctrl+C to exit.\n\n";

    system.reset();

    // Simulate some key presses after a delay
    std::thread input_thread(
        [&system]()
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          system.pressKey('A');
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          system.pressKey('B');
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          system.pressKey('C');
        });

    std::cout << "Starting execution...\n";

    // Run the system
    for (int i = 0; i < 10000; ++i)  // Run for a while
    {
      system.step();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    input_thread.join();

    std::cout << "Test completed successfully!\n";
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
