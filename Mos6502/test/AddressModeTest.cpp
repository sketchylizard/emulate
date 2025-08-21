#include "src/AddressMode.h"

#include <catch2/catch_test_macros.hpp>
#include <format>
#include <fstream>
#include <span>

#include "Mos6502/Mos6502.h"
#include "common/Memory.h"

using namespace Common;

static bool wasStartOperationCalled = false;

struct Cycle
{
  BusResponse input;  // Data provided TO the addressing mode
  BusRequest expected;  // Address the addressing mode should REQUEST
};

static BusRequest TestReadOperation(Mos6502& cpu, BusResponse /*response*/, Byte /*step*/)
{
  static_cast<void>(cpu);
  wasStartOperationCalled = true;
  return BusRequest::Read(0xffff_addr);  // Dummy read to simulate operation
}

static BusRequest TestWriteOperation(Mos6502& cpu, BusResponse /*response*/, Byte /*step*/)
{
  wasStartOperationCalled = true;

  return BusRequest::Write(cpu.target(), cpu.a());
}

static Mos6502::Instruction TestReadInstruction{"RD ", nullptr, TestReadOperation};
static Mos6502::Instruction TestWriteInstruction{"WR ", nullptr, TestWriteOperation};

// Helper function for multi-step execution
// Memory is a span of Cycle structures, each containing the data to push on the BusRequest as input to the
// CPU for that step, and the expected BusRequest the addressing mode should produce.
void executeAddressingMode(Mos6502::StateFunc addressingMode, Mos6502& cpu, std::span<Cycle> memory)
{
  BusRequest request;

  wasStartOperationCalled = false;

  Byte step = 0;
  for (auto stepData : memory)
  {
    REQUIRE_FALSE(wasStartOperationCalled);

    request = addressingMode(cpu, BusResponse{stepData.input}, step++);
    REQUIRE(request == stepData.expected);
  }

  CHECK(wasStartOperationCalled);
}

TEST_CASE("Absolute addressing mode - basic functionality", "[addressing][absolute]")
{
  Mos6502 cpu;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestReadInstruction);  // Set a test instruction

  // Setup: LDA $1234 (3 bytes: opcode, $34, $12)
  cpu.set_pc(0x8000_addr);

  BusRequest request;

  // Step 0: Read low byte
  request = AddressMode::absoluteRead<Index::None>(cpu, BusResponse{}, 0);
  CHECK(request.address == 0x8000_addr);
  CHECK(request.isRead());
  CHECK(cpu.pc() == 0x8001_addr);
  CHECK_FALSE(wasStartOperationCalled);

  // Simulate memory response

  // Step 1: Read high byte
  request = AddressMode::absoluteRead<Index::None>(cpu, BusResponse{0x34}, 1);
  CHECK(request.address == 0x8001_addr);
  CHECK(request.isRead());
  CHECK(cpu.pc() == 0x8002_addr);
  CHECK_FALSE(wasStartOperationCalled);

  // Step 2: Read from target address
  request = AddressMode::absoluteRead<Index::None>(cpu, BusResponse{0x12}, 2);
  CHECK(request.address == 0x1234_addr);
  CHECK(request.isRead());
  CHECK_FALSE(wasStartOperationCalled);

  // Step 3: Read from target address
  request = AddressMode::absoluteRead<Index::None>(cpu, BusResponse{}, 3);
  CHECK(request.address == Address{0xffff});
  CHECK(request.isRead());
  CHECK(wasStartOperationCalled);
}

TEST_CASE("Absolute,X addressing mode", "[addressing][absolute][indexed]")
{
  Mos6502 cpu;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestReadInstruction);  // Set a test instruction

  SECTION("No page boundary crossing")
  {
    cpu.set_pc(0x8000_addr);
    cpu.set_x(0x05);

    Address addressToRead = 0x1012_addr;
    Address expectedIndexedAddress = addressToRead + 0x05;

    BusRequest request;

    // Step 0: Read low byte
    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{}, 0);
    CHECK(request.address == 0x8000_addr);
    CHECK_FALSE(wasStartOperationCalled);

    // Step 1: Read high byte
    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{LoByte(addressToRead)}, 1);
    CHECK(request.address == 0x8001_addr);
    CHECK_FALSE(wasStartOperationCalled);

    // Step 2: Read from indexed address
    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{HiByte(addressToRead)}, 2);
    CHECK(request.address == expectedIndexedAddress);
    CHECK(request.isRead());
    CHECK_FALSE(wasStartOperationCalled);

    // Step 3: Read from indexed address
    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{0x99}, 3);
    CHECK(request.address == Address{0xffff});
    CHECK(request.isRead());

    CHECK(wasStartOperationCalled);
  }

  SECTION("Page boundary crossing")
  {
    Address addressToRead = 0x20FF_addr;
    Address expectedIndexedAddress = addressToRead + 0x10;
    Address wrongAddress = 0x200F_addr;  // Due to page boundary bug
    Address startingPc = 0x8000_addr;

    cpu.set_pc(startingPc);
    cpu.set_x(0x10);

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

    executeAddressingMode(AddressMode::absoluteRead<Index::X>, cpu, memory);
  }
}

TEST_CASE("Absolute write addressing mode", "[addressing][absolute][write]")
{
  Mos6502 cpu;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestWriteInstruction);  // Set a test instruction

  Address startingPc = 0x8000_addr;
  Address addressToRead = 0x1234_addr;

  SECTION("Basic write operation")
  {
    cpu.set_pc(0x8000_addr);

    BusRequest request;

    // Steps 0-1: Read address bytes
    request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{}, 0);
    CHECK(request.address == startingPc);

    request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{LoByte(addressToRead)}, 1);
    CHECK(request.address == startingPc + 1);

    // Step 2: Should call StartOperation (no read from target)
    request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{HiByte(addressToRead)}, 2);

    // Verify target address was set correctly
    CHECK(cpu.target() == addressToRead);

    CHECK(wasStartOperationCalled);
  }
}

TEST_CASE("Absolute addressing edge cases", "[addressing][absolute][edge]")
{
  Mos6502 cpu;

  SECTION("Address wrapping at 16-bit boundary")
  {
    Address startingPc = 0xFFFF_addr;
    Address wrappedPc = 0x0000_addr;
    Address addressToRead = 0xFFFF_addr;  // Will wrap to $0000 when indexed

    cpu.set_pc(startingPc);
    cpu.set_x(0x01);

    BusRequest request;

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{}, 0);
    CHECK(request.address == startingPc);
    CHECK(cpu.pc() == wrappedPc);  // PC should wrap

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{LoByte(addressToRead)}, 1);
    CHECK(request.address == wrappedPc);
    CHECK(cpu.pc() == 0x0001_addr);
  }

  SECTION("Zero page crossing with index")
  {
    Address startingPc = 0x8000_addr;
    Address addressToRead = 0x00FF_addr;  // Will wrap to $0000 when indexed
    Address wrongAddress = 0x0000_addr;  // After wrapping, but before carry
    Address expectedIndexedAddress = 0x0100_addr;  // After wrapping

    cpu.set_pc(startingPc);
    cpu.set_x(0x01);
    cpu.setInstruction(TestReadInstruction);

    BusRequest request;

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{}, 0);
    CHECK(request.address == startingPc);

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{LoByte(addressToRead)}, 1);
    CHECK(request.address == startingPc + 1);

    // Should trigger page boundary logic even in zero page
    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{}, 2);
    CHECK(request.address == wrongAddress);  // Wrong address first

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{0x99}, 3);
    CHECK(request.address == expectedIndexedAddress);  // Correct address after wrapping

    request = AddressMode::absoluteRead<Index::X>(cpu, BusResponse{0xAB}, 4);
    CHECK(wasStartOperationCalled);
  }
}

TEST_CASE("Complete addressing mode execution", "[addressing][integration]")
{
  Address programCounter = 0x8000_addr;
  Address baseAddress = 0x4321_addr;
  Address expectedIndexedAddress = baseAddress + 0x05;

  Mos6502 cpu;

  // Setup a complete scenario
  cpu.set_pc(programCounter);
  cpu.set_x(0x05);

  cpu.setInstruction(TestReadInstruction);
  wasStartOperationCalled = false;

  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Request for lo byte (data is ignored)
      {{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},  // Request for hi byte (data is low byte)
      {{HiByte(baseAddress)}, BusRequest::Read(expectedIndexedAddress)},  // Request to read operand (data is high byte)
      {{0xAB}, BusRequest::Read(0xffff_addr)},  // Target value
  };

  executeAddressingMode(AddressMode::absoluteRead<Index::X>, cpu, memory);

  CHECK(wasStartOperationCalled);
  CHECK(cpu.operand() == 0xAB);
}

// Tests for AddressMode::absoluteWrite
// Add these to your AddressModeTest.cpp file

// Test absolute write mode (no indexing)
TEST_CASE("AddressMode::absoluteWrite<Index::None> - Basic functionality")
{
  Mos6502 cpu;
  cpu.setInstruction(TestReadInstruction);
  cpu.set_pc(0x1000_addr);
  wasStartOperationCalled = false;

  BusRequest request;

  // Step 0: Read low byte address
  request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{}, 0);
  CHECK(request.address == 0x1000_addr);
  CHECK(request.isRead());
  CHECK(cpu.pc() == 0x1001_addr);

  // Step 1: Read high byte address (simulate reading 0x34 as low byte)
  request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{0x34}, 1);
  CHECK(request.address == 0x1001_addr);
  CHECK(request.isRead());
  CHECK(cpu.pc() == 0x1002_addr);
  CHECK(LoByte(cpu.target()) == 0x34);

  // Step 2: Calculate target address and start operation (simulate reading 0x12 as high byte)
  request = AddressMode::absoluteWrite<Index::None>(cpu, BusResponse{0x12}, 2);
  CHECK(cpu.target() == 0x1234_addr);
  CHECK(wasStartOperationCalled == true);
}

TEST_CASE("AddressMode::absoluteWrite<Index::X> - No page boundary crossing")
{
  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x2010_addr;  // Base address for testing
  Address expectedIndexedAddress = baseAddress + 0x05;

  Mos6502 cpu;
  cpu.set_pc(programCounter);
  cpu.set_x(0x05);

  cpu.setInstruction(TestWriteInstruction);

  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Request for lo byte (data is ignored)
      {{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},  // Request for hi byte (data is low byte)
      {{HiByte(baseAddress)}, BusRequest::Write(expectedIndexedAddress, 0x00)},  // Request to write (data is high byte)
  };

  executeAddressingMode(AddressMode::absoluteWrite<Index::X>, cpu, memory);
}

TEST_CASE("AddressMode::absoluteWrite<Index::X> - Page boundary crossing")
{
  Mos6502 cpu;
  cpu.setInstruction(TestWriteInstruction);
  cpu.set_pc(0x1000_addr);
  cpu.set_x(0x10);
  wasStartOperationCalled = false;

  // Step 0: Read low byte address
  auto request = AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{}, 0);
  CHECK(request.address == 0x1000_addr);
  CHECK(request.isRead());

  // Step 1: Read high byte address (simulate reading 0xFF as low byte)
  request = AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{0xFF}, 1);
  CHECK(request.address == 0x1001_addr);
  CHECK(request.isRead());

  // Step 2: Page boundary crossing (0xFF + 0x10 = 0x10F, wraps to 0x0F)
  request = AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{0x10}, 2);
  CHECK(request.address == 0x100F_addr);  // Wrong address due to page boundary crossing
  CHECK(request.isRead());
  CHECK(wasStartOperationCalled == false);

  // Step 3: Extra cycle due to page boundary crossing
  request = AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{0x00}, 3);
  CHECK(wasStartOperationCalled == true);
  CHECK(request.address == 0x110F_addr);
}

TEST_CASE("AddressMode::absoluteWrite<Index::Y> - No page boundary crossing")
{
  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x3830_addr;
  Address expectedIndexedAddress = baseAddress + 0x08;

  Mos6502 cpu;
  cpu.set_pc(programCounter);
  cpu.set_y(0x08);

  cpu.setInstruction(TestWriteInstruction);
  wasStartOperationCalled = false;

  // Step 0: Read low byte address
  auto request = AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{}, 0);
  CHECK(request.address == programCounter);
  CHECK(request.isRead());

  // Step 1: Read high byte address (simulate reading 0x30 as low byte)
  request = AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{LoByte(baseAddress)}, 1);
  CHECK(request.address == programCounter + 1);
  CHECK(request.isRead());

  // Step 2: No page boundary crossing (0x30 + 0x08 = 0x38, no overflow)
  request = AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{HiByte(baseAddress)}, 2);
  CHECK(request.address == expectedIndexedAddress);
}

TEST_CASE("AddressMode::absoluteWrite<Index::Y> - Page boundary crossing")
{
  Address programCounter = 0x1000_addr;
  Address baseAddress = 0x00F0_addr;
  Address expectedWrongAddress = 0x0010_addr;
  Address expectedIndexedAddress = baseAddress + 0x20;

  Mos6502 cpu;
  cpu.set_pc(programCounter);
  cpu.set_y(0x20);  // Index value
  cpu.set_a(0x99);  // test value

  cpu.setInstruction(TestWriteInstruction);

  Cycle memory[] = {
      {{0x00}, BusRequest::Read(programCounter)},  // Request for lo byte (data is ignored)
      {{LoByte(baseAddress)}, BusRequest::Read(programCounter + 1)},  // Request for hi byte (data is low byte)
      {{HiByte(baseAddress)}, BusRequest::Read(expectedWrongAddress)},  // Request to read wrong address (data is high
                                                                        // byte)
      {{0x99}, BusRequest::Write(expectedIndexedAddress, 0x99)},  // Request to write operand (data is 0x99)
  };

  executeAddressingMode(AddressMode::absoluteWrite<Index::Y>, cpu, memory);
}

TEST_CASE("AddressMode::absoluteWrite<Index::X> - Edge case: X register is 0")
{
  Mos6502 cpu;
  cpu.setInstruction(TestWriteInstruction);
  cpu.set_pc(0x2000_addr);
  cpu.set_x(0x00);
  wasStartOperationCalled = false;

  // Should behave like non-indexed mode when X = 0
  AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{}, 0);
  AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{0x50}, 1);
  AddressMode::absoluteWrite<Index::X>(cpu, BusResponse{0x60}, 2);

  CHECK(cpu.target() == 0x6050_addr);  // No change since X = 0
  CHECK(wasStartOperationCalled == true);
}

TEST_CASE("AddressMode::absoluteWrite<Index::Y> - Edge case: Y register is 0xFF")
{
  Mos6502 cpu;
  cpu.setInstruction(TestReadInstruction);
  cpu.set_pc(0x3000_addr);
  cpu.set_y(0xFF);
  wasStartOperationCalled = false;

  BusRequest request;

  AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{}, 0);
  AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{0x01}, 1);  // Low byte = 0x01
  BusRequest result = AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{0x10}, 2);  // High byte = 0x10

  // 0x01 + 0xFF = 0x100_addr, so page boundary is crossed
  CHECK(cpu.target() == 0x1100_addr);  // 0x1001_addr + 0xFF
  CHECK(result.address == 0x1000_addr);  // Wrong address (0x1001_addr wrapped to 0x1000_addr)
  CHECK(result.isRead());
  CHECK(wasStartOperationCalled == false);

  // Step 3: Complete the addressing mode
  request.data = 0x00;
  AddressMode::absoluteWrite<Index::Y>(cpu, BusResponse{}, 3);
  CHECK(wasStartOperationCalled == true);
}

////////////////////////////////////////////////////////////////////////////////
// Zero Page Addressing Modes
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Zero Page Read addressing mode", "[addressing][zeropage]")
{
  Address programCounter = 0x8000_addr;

  Mos6502 cpu;
  cpu.set_pc(programCounter);

  cpu.setInstruction(TestReadInstruction);

  SECTION("Basic functionality")
  {
    Address zpAddress = 0x0042_addr;

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{0x42}, BusRequest::Read(zpAddress)},  // Fetch from zero page address
        {{0xAB}, BusRequest::Read(0xffff_addr)},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::None>, cpu, memory);
  }

  SECTION("X Read addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = zpBase + 5;

    cpu.set_x(0x05);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Read(expectedAddress)},  // Read base address (discarded)
        {{0xAB}, BusRequest::Read(0xffff_addr)},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::X>, cpu, memory);
  }

  SECTION("X Read addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.set_x(0x05);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Read(expectedAddress)},  // Read from wrapped address
        {{0xCD}, BusRequest::Read(0xffff_addr)},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,Y Read addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.set_y(0x03);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Read(expectedAddress)},  // Read expected address
        {{0xEF}, BusRequest::Read(0xffff_addr)},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::Y>, cpu, memory);
  }

  SECTION("Zero Page,Y Read addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.set_y(0x03);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Read(expectedAddress)},  // Read expected address
        {{0x12}, BusRequest::Read(0xffff_addr)},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::Y>, cpu, memory);
  }
}

TEST_CASE("Zero Page Write addressing mode", "[addressing][zeropage]")
{
  Address programCounter = 0x8000_addr;

  Mos6502 cpu;
  cpu.set_pc(programCounter);
  cpu.set_a(0xFE);

  cpu.setInstruction(TestWriteInstruction);

  SECTION("Basic functionality")
  {
    Address zpAddress = 0x0042_addr;
    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{0x42}, BusRequest::Write(zpAddress, 0xFE)},  // Write to zero page address
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::None>, cpu, memory);
  }

  SECTION("Zero Page,X Write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0047_addr;  // 0x42 + 0x05

    cpu.set_x(0x05);
    cpu.set_a(0x05);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x05)},  // Write to expected address with X offset
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,X Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.set_x(0x05);
    cpu.set_a(0x07);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x07)},  // Write to expected address with X offset
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,Y Write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.set_y(0x03);
    cpu.set_a(0x33);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x33)},  // Write to expected address with Y offset
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::Y>, cpu, memory);
  }

  SECTION("Zero Page,Y Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.set_y(0x03);
    cpu.set_a(0x13);

    Cycle memory[] = {
        {{0x00}, BusRequest::Read(programCounter)},  // Fetch opcode
        {{LoByte(zpBase)}, BusRequest::Write(expectedAddress, 0x13)},  // Write to wrapped address with Y offset
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::Y>, cpu, memory);
  }
}
