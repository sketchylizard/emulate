// main.cpp or test_apple2.cpp
#include <iostream>

#include "apple2/apple2system.h"

using namespace Common;

int main()
{
  try
  {
    std::cout << "Creating Apple II system...\n";

    Byte ram[0xc000] = {};  // 48KB RAM
    Byte rom[0x2000] = {};  // 8KB ROM (placeholder
    Byte langBank0[0x1000] = {};  // Language Card Bank 0 (4KB)
    Byte langBank1[0x1000] = {};  // Language Card Bank

    // Write a simple test program to memory
    // LDA #$42 (A9 42) - Load accumulator with $42
    // NOP      (EA)    - No operation
    // JMP $8000 (4C 00 80) - Jump back to start
    ram[0x8000] = 0xA9;
    ram[0x8001] = 0x42;
    ram[0x8002] = 0xEA;
    ram[0x8003] = 0x4C;
    ram[0x8004] = 0x00;

    rom[0x1FFC] = 0x00;  // Reset vector low byte
    rom[0x1FFD] = 0x80;  // Reset vector high byte

    apple2::Apple2System system{std::span(ram), std::span(rom), std::span(langBank0), std::span(langBank1)};

    std::cout << "System created successfully!\n";

    system.reset();

    std::cout << "Starting execution...\n";

    system.step();  // Execute LDA #$42
    system.step();  // Execute NOP

    // Check that accumulator is now $42
    std::cout << "CPU A register: $" << std::hex << static_cast<int32_t>(system.cpu().registers.a) << "\n";

    std::cout << "Test completed successfully!\n";
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
