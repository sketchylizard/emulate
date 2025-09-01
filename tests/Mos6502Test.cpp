#include "cpu6502/mos6502.h"

#include <bitset>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "KlausFunctional.h"
#include "common/address.h"
#include "common/bus.h"
#include "common/hex.h"
#include "common/memory.h"
#include "common/microcode_pump.h"

using namespace Common;

TEST_CASE("MicrocodePump: Functional_tests", "[.]")
{

  auto runTest = []()
  {
    auto data = Klaus__6502_functional_test::data();  // Ensure the data is linked in

    Byte memory[0x10000] = {};

    std::ranges::copy(data, memory);

    MemoryDevice memDevice(memory);

    MicrocodePump<cpu6502::mos6502> executionPipeline;

    cpu6502::State cpu;

    auto programStart = Address{0x0400};

    // Set the reset vector to 0x0400
    cpu.pc = programStart;

    BusRequest request;
    BusResponse response;

    std::bitset<0x10000> breakpoints;
    breakpoints.set(static_cast<size_t>(0x056b));

    char buffer[80];

    auto executeOneInstruction = [&]()
    {
      // Execute until we hit a sync (instruction boundary)
      do
      {
        request = executionPipeline.tick(cpu, response);

        response = memDevice.tick(request);
      } while (!request.isSync());

      // This is logging the **next** instruction to be executed.
      Common::Byte* opcode = &memory[static_cast<size_t>(request.address)];
      std::span<Common::Byte, 3> bytes{opcode, 3};
      cpu6502::mos6502::disassemble(cpu, request.address, bytes, buffer);
      std::cout << buffer << '\n';
    };

    try
    {
      for (;;)
      {
        executeOneInstruction();

        // We are at an instruction boundary. This is a good place to check for breakpoints.
        if (breakpoints.test(static_cast<size_t>(request.address)))
        {
          std::cout << std::format("User breakpoint at address ${:04X}\n", request.address);
        }
      }
    }
    catch (const cpu6502::TrapException& e)
    {
      // See if it is one of the known trap points
      auto trapAddress = e.address();

      if (trapAddress == Klaus__6502_functional_test::success)
      {
        return true;  // Test passed
      }
      std::cerr << std::format("TrapException at address ${:04X}\n", static_cast<uint16_t>(trapAddress));
      return false;
    }
    catch (const std::exception& e)
    {
      std::cerr << e.what() << '\n';
      return false;
    }
  };
  CHECK(runTest() == true);
}
