#include <catch2/catch_test_macros.hpp>
#include <format>
#include <fstream>
#include <span>

#include "common/address.h"
#include "core65xx/address_mode.h"
#include "core65xx/core65xx.h"

using namespace Common;

static bool wasStartOperationCalled = false;

struct Cycle
{
  BusResponse input;  // Data provided TO the addressing mode
  BusRequest expected;  // Address the addressing mode should REQUEST
};

static BusRequest TestReadOperation(Core65xx& cpu, BusResponse /*response*/)
{
  static_cast<void>(cpu);
  wasStartOperationCalled = true;
  return BusRequest::Read(0xffff_addr);  // Dummy read to simulate operation
}

static BusRequest TestWriteOperation(Core65xx& cpu, BusResponse /*response*/)
{
  wasStartOperationCalled = true;

  return BusRequest::Write(cpu.getEffectiveAddress(), cpu.regs.a);
}

// Helper function for multi-step execution
// Memory is a span of Cycle structures, each containing the data to push on the BusRequest as input to the
// CPU for that step, and the expected BusRequest the addressing mode should produce.
void executeAddressingMode(const Core65xx::Instruction& instruction, Core65xx& cpu, std::span<Cycle> memory)
{
  cpu.setInstruction(instruction);

  BusRequest request;

  wasStartOperationCalled = false;

  for (auto stepData : memory)
  {
    REQUIRE_FALSE(wasStartOperationCalled);

    request = cpu.Tick(BusResponse{stepData.input});
    REQUIRE(request == stepData.expected);
  }

  CHECK(wasStartOperationCalled);
}

TEST_CASE("Absolute addressing mode", "[addressing][absolute]")
{
  Core65xx cpu;
  // Setup: LDA $1234 (3 bytes: opcode, $34, $12)
  cpu.regs = Core65xx::Regs{0x8000_addr};

  SECTION("Basic functionality")
  {
    Cycle cycles[] = {
        //
        {BusResponse{}, BusRequest::Read(0x8000_addr)},
        {BusResponse{0x34}, BusRequest::Read(0x8001_addr)},
        {BusResponse{0x12}, BusRequest::Read(0x1234_addr)},
        {BusResponse{0x99}, BusRequest::Read(0xffff_addr)},
    };

    executeAddressingMode({"RD", {AddressMode::abs, &AddressMode::Fetch, TestReadOperation}}, cpu, cycles);
  }
}

TEST_CASE("Absolute,X addressing mode", "[addressing][absolute][indexed]")
{
  Core65xx cpu;
  cpu.regs = Core65xx::Regs{0x8000_addr};

  Core65xx::Instruction TestReadInstruction{"RD", {AddressMode::absX, &AddressMode::Fetch, TestReadOperation}};

  SECTION("No page boundary crossing")
  {
    cpu.regs.x = 0x05;

    Address addressToRead = 0x1012_addr;
    Address expectedIndexedAddress = addressToRead + 0x05;

    Cycle cycles[] = {
        {BusResponse{0}, BusRequest::Read(0x8000_addr)},
        {BusResponse{LoByte(addressToRead)}, BusRequest::Read(0x8001_addr)},
        {BusResponse{HiByte(addressToRead)}, BusRequest::Read(expectedIndexedAddress)},
        {BusResponse{0x99}, BusRequest::Read(0xffff_addr)},
    };

    executeAddressingMode(TestReadInstruction, cpu, cycles);
  }

  SECTION("Page boundary crossing")
  {
    Address addressToRead = 0x20FF_addr;
    Address expectedIndexedAddress = addressToRead + 0x10;
    Address wrongAddress = 0x200F_addr;  // Due to page boundary bug
    Address startingPc = 0x8000_addr;

    cpu.regs = Core65xx::Regs{.pc = startingPc, .x = 0x10};

    // Define the test steps as a Cycle array
    Cycle memory[] = {
        // Step 0: Read low byte
        {{0x00}, BusRequest::Read(startingPc)},
        // Step 1: Read high byte
        {{LoByte(addressToRead)}, BusRequest::Read(startingPc + 1)},
        // Step 2: Read from wrong address (page boundary bug)
        {{HiByte(addressToRead)}, BusRequest::Read(wrongAddress)},
        // Step 3: Read from correct address
        {{0x42}, BusRequest::Read(expectedIndexedAddress)},
        // Step 4: Start operation (dummy read to 0xffff)
        {{0xff}, BusRequest::Read(0xffff_addr)},
    };

    executeAddressingMode(TestReadInstruction, cpu, memory);
  }
}

TEST_CASE("Absolute write addressing mode", "[addressing][absolute][write]")
{
  Core65xx cpu;

  Core65xx::Instruction instruction{"WR", {AddressMode::abs, TestWriteOperation}};

  wasStartOperationCalled = false;

  Address startingPc = 0x8000_addr;
  Address addressToRead = 0x1234_addr;

  SECTION("Basic write operation")
  {
    cpu.regs.pc = startingPc;
    cpu.regs.a = 0x99;

    Cycle cycles[] = {
        {BusResponse{}, BusRequest::Read(startingPc)},
        {BusResponse{LoByte(addressToRead)}, BusRequest::Read(startingPc + 1)},
        {BusResponse{HiByte(addressToRead)}, BusRequest::Write(addressToRead, 0x99)},
    };

    executeAddressingMode(instruction, cpu, cycles);
  }
}

TEST_CASE("Absolute addressing edge cases", "[addressing][absolute][edge]")
{
  Core65xx cpu;

  Core65xx::Instruction TestReadInstruction{"RD", {AddressMode::absX, &AddressMode::Fetch, TestReadOperation}};

  SECTION("Address wrapping at 16-bit boundary")
  {
    Address startingPc = 0xFFFF_addr;
    Address wrappedPc = 0x0000_addr;
    Address addressToRead = 0xFFFF_addr;  // Starting address to read
    Address wrongAddress = 0xFF00_addr;  // Low byte wrapped, but high byte not incremented yet
    Address expectedAddress = 0x0000_addr;  // Will wrap to $0000 when indexed

    cpu.regs.pc = startingPc;
    cpu.regs.x = 0x01;

    Cycle cycles[] = {
        {BusResponse{}, BusRequest::Read(startingPc)},
        {BusResponse{LoByte(addressToRead)}, BusRequest::Read(wrappedPc)},
        {BusResponse{HiByte(addressToRead)}, BusRequest::Read(wrongAddress)},
        {BusResponse{0x75}, BusRequest::Read(expectedAddress)},
        {BusResponse{0x88}, BusRequest::Read(0xffff_addr)},
    };

    executeAddressingMode(TestReadInstruction, cpu, cycles);
  }

  SECTION("Zero page crossing with index")
  {
    Address startingPc = 0x8000_addr;
    Address addressToRead = 0x00FF_addr;  // Will wrap to $0000 when indexed
    Address wrongAddress = 0x0000_addr;  // After wrapping, but before carry
    Address expectedIndexedAddress = 0x0100_addr;  // After wrapping

    cpu.regs.pc = startingPc;
    cpu.regs.x = 0x01;

    Cycle cycles[] = {
        {BusResponse{}, BusRequest::Read(startingPc)},
        {BusResponse{LoByte(addressToRead)}, BusRequest::Read(startingPc + 1)},
        {BusResponse{0x0}, BusRequest::Read(wrongAddress)},
        {BusResponse{0x09}, BusRequest::Read(expectedIndexedAddress)},
        {BusResponse{0xAB}, BusRequest::Read(0xffff_addr)},

    };
    executeAddressingMode(TestReadInstruction, cpu, cycles);
  }
}

// Tests for AddressMode::abs
// Add these to your AddressModeTest.cpp file

// Test absolute write mode (no indexing)
TEST_CASE("AddressMode::abs<Index::None> - Basic functionality")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::abs, TestWriteOperation}};

  Core65xx cpu;
  cpu.regs.pc = 0x1000_addr;
  cpu.regs.a = 0x29;  // Set accumulator to a test value
  wasStartOperationCalled = false;

  Cycle cycles[] = {
      {BusResponse{}, BusRequest::Read(0x1000_addr)},
      {BusResponse{0x34}, BusRequest::Read(0x1001_addr)},
      {BusResponse{0x12}, BusRequest::Write(0x1234_addr, 0x29)},

  };

  executeAddressingMode(instruction, cpu, cycles);
}

TEST_CASE("AddressMode::abs<Index::X> - No page boundary crossing")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absX, TestWriteOperation}};

  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x2010_addr;  // Base address for testing
  Address expectedIndexedAddress = baseAddress + 0x05;

  Core65xx cpu;
  cpu.regs.pc = programCounter;
  cpu.regs.x = 0x05;

  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Request for lo byte (data is ignored)
      {{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},  // Request for hi byte (data is low byte)
      {{HiByte(baseAddress)}, BusRequest::Write(expectedIndexedAddress, 0x00)},  // Request to write (data is high
                                                                                 // byte)
  };

  executeAddressingMode(instruction, cpu, memory);
}

TEST_CASE("AddressMode::abs<Index::X> - Page boundary crossing")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absX, TestWriteOperation}};

  Core65xx cpu;
  cpu.regs.pc = 0x1000_addr;
  cpu.regs.x = 0x10;
  cpu.regs.a = 0x9a;

  Cycle cycles[] = {
      {BusResponse{}, BusRequest::Read(0x1000_addr)},
      {BusResponse{0xFF}, BusRequest::Read(0x1001_addr)},
      {BusResponse{0x10}, BusRequest::Read(0x100F_addr)},  // wrong address because of page crossing
      {BusResponse{0x55}, BusRequest::Write(0x110F_addr, 0x9a)},
  };

  executeAddressingMode(instruction, cpu, cycles);
}

TEST_CASE("AddressMode::abs<Index::Y> - No page boundary crossing")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absY, TestWriteOperation}};

  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x3830_addr;
  Address expectedIndexedAddress = baseAddress + 0x08;

  Core65xx cpu;
  cpu.regs.pc = programCounter;
  cpu.regs.y = 0x08;
  cpu.regs.a = 0x14;

  Cycle cycles[] = {
      {BusResponse{}, BusRequest::Read(programCounter)},
      {BusResponse{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},
      {BusResponse{HiByte(baseAddress)}, BusRequest::Write(expectedIndexedAddress, 0x14)},
  };

  executeAddressingMode(instruction, cpu, cycles);
}

TEST_CASE("AddressMode::abs<Index::Y> - Page boundary crossing")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absY, TestWriteOperation}};

  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x00F0_addr;
  Address expectedWrongAddress = 0x0010_addr;
  Address expectedIndexedAddress = baseAddress + 0x20;

  Core65xx cpu;
  cpu.regs.pc = programCounter;
  cpu.regs.y = 0x20;  // Index value
  cpu.regs.a = 0x99;  // test value


  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Request for lo byte (data is ignored)
      {{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},  // Request for hi byte (data is low byte)
      {{HiByte(baseAddress)}, BusRequest::Read(expectedWrongAddress)},  // Request to read wrong address (data is high
                                                                        // byte)
      {{0x99}, BusRequest::Write(expectedIndexedAddress, 0x99)},  // Request to write operand (data is 0x99)
  };

  executeAddressingMode(instruction, cpu, memory);
}

TEST_CASE("AddressMode::abs<Index::X> - Edge case: X register is 0")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absX, TestWriteOperation}};

  Core65xx cpu;
  cpu.regs.pc = 0x2000_addr;
  cpu.regs.x = 0x00;
  cpu.regs.a = 0x81;

  Cycle cycles[] = {
      {BusResponse{}, BusRequest::Read(0x2000_addr)},  //
      {BusResponse{0x50}, BusRequest::Read(0x2001_addr)},  //
      {BusResponse{0x60}, BusRequest::Write(0x6050_addr, 0x81)},  // No change since X = 0
  };

  // Should behave like non-indexed mode when X = 0
  executeAddressingMode(instruction, cpu, cycles);
}

TEST_CASE("AddressMode::abs<Index::Y> - Edge case: Y register is 0xFF")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::absY, TestWriteOperation}};

  Core65xx cpu;
  cpu.regs.pc = 0x3000_addr;
  cpu.regs.y = 0xFF;
  cpu.regs.a = 0x24;

  Cycle cycles[] = {
      {BusResponse{}, BusRequest::Read(0x3000_addr)},  //
      {BusResponse{0x01}, BusRequest::Read(0x3001_addr)},
      // 0x01 + 0xFF = 0x100_addr, so page boundary is crossed
      {BusResponse{0x10}, BusRequest::Read(0x1000_addr)},  // wrong
      {BusResponse{0x20}, BusRequest::Write(0x1100_addr, 0x24)},  // correct
  };

  executeAddressingMode(instruction, cpu, cycles);
}

////////////////////////////////////////////////////////////////////////////////
// Zero Page Addressing Modes
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Zero Page Read addressing mode", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zp, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;
  cpu.regs.a = 0xc5;

  Address zpAddress = 0x0042_addr;

  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
      {{0x42}, BusRequest::Write(zpAddress, 0xC5)},  // Target operand value
  };

  executeAddressingMode(instruction, cpu, memory);
}

TEST_CASE("Zero Page Read addressing mode X", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zpX, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;


  SECTION("X Read addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;

    cpu.regs.x = 0x05;
    cpu.regs.a = 0xE2;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(0x0047_addr, 0xE2)},  // Target operand value
    };

    executeAddressingMode(instruction, cpu, memory);
  }

  SECTION("X Read addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.regs.x = 0x05;
    cpu.regs.a = 0xD0;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0xD0)},  // Target operand value
    };

    executeAddressingMode(instruction, cpu, memory);
  }
}

TEST_CASE("Zero Page Read addressing mode Y", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zpY, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;

  SECTION("Zero Page,Y write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.regs.y = 0x03;
    cpu.regs.a = 0xBB;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0xBB)},  // Target operand value
    };

    executeAddressingMode(instruction, cpu, memory);
  }

  SECTION("Zero Page,Y write addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.regs.y = 0x03;
    cpu.regs.a = 0xB4;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0xB4)},  // Target operand value
    };

    executeAddressingMode(instruction, cpu, memory);
  }
}

TEST_CASE("Zero Page Write addressing mode", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zp, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;
  cpu.regs.a = 0xFE;

  Address zpAddress = 0x0042_addr;
  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
      {{0x42}, BusRequest::Write(zpAddress, 0xFE)},  // Write to zero page address
  };

  executeAddressingMode(instruction, cpu, memory);
}

TEST_CASE("Zero Page, X Write addressing mode", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zpX, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;

  SECTION("Zero Page,X Write addressing mode - no wrap")
  {
    cpu.regs.a = 0xFE;
    cpu.regs.x = 0x05;

    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0047_addr;  // 0x42 + 0x05

    cpu.regs.x = 0x05;
    cpu.regs.a = 0x05;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x05)},  // Write to expected address with X offset
    };

    executeAddressingMode(instruction, cpu, memory);
  }

  SECTION("Zero Page,X Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.regs.x = 0x05;
    cpu.regs.a = 0x07;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x07)},  // Write to expected address with X offset
    };

    executeAddressingMode(instruction, cpu, memory);
  }
}

TEST_CASE("Zero Page, Y Write addressing mode", "[addressing][zeropage]")
{
  Core65xx::Instruction instruction{"WR", {AddressMode::zpY, TestWriteOperation}};

  Address programCounter = 0x8000_addr;

  Core65xx cpu;
  cpu.regs.pc = programCounter;

  SECTION("Zero Page,Y Write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.regs.y = 0x03;
    cpu.regs.a = 0x33;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x33)},  // Write to expected address with Y offset
    };

    executeAddressingMode(instruction, cpu, memory);
  }

  SECTION("Zero Page,Y Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.regs.y = 0x03;
    cpu.regs.a = 0x13;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x13)},  // Write to wrapped address with Y offset
    };

    executeAddressingMode(instruction, cpu, memory);
  }
}
