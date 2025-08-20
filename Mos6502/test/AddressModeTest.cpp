#include "src/AddressMode.h"

#include <catch2/catch_test_macros.hpp>
#include <format>
#include <fstream>
#include <span>

#include "Mos6502/Mos6502.h"
#include "common/Memory.h"

using namespace Common;

static bool wasStartOperationCalled = false;

static Bus TestReadOperation(Mos6502& cpu, Bus /*bus*/, Byte /*step*/)
{
  static_cast<void>(cpu);
  wasStartOperationCalled = true;
  return Bus::Read(0xffff_addr);  // Dummy read to simulate operation
}

static Bus TestWriteOperation(Mos6502& cpu, Bus /*bus*/, Byte /*step*/)
{
  wasStartOperationCalled = true;

  return Bus::Write(cpu.target(), cpu.a());
}

static Mos6502::Instruction TestReadInstruction{"RD ", nullptr, TestReadOperation};
static Mos6502::Instruction TestWriteInstruction{"WR ", nullptr, TestWriteOperation};

TEST_CASE("Absolute addressing mode - basic functionality", "[addressing][absolute]")
{
  Mos6502 cpu;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestReadInstruction);  // Set a test instruction

  // Setup: LDA $1234 (3 bytes: opcode, $34, $12)
  cpu.set_pc(0x8000_addr);

  Bus bus = Bus::Read(0x8000_addr);

  // Step 0: Read low byte
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 0);
  CHECK(bus.address == 0x8000_addr);
  CHECK(bus.isRead());
  CHECK(cpu.pc() == 0x8001_addr);
  CHECK_FALSE(wasStartOperationCalled);

  // Simulate memory response
  bus.data = 0x34;

  // Step 1: Read high byte
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 1);
  CHECK(bus.address == 0x8001_addr);
  CHECK(bus.isRead());
  CHECK(cpu.pc() == 0x8002_addr);
  CHECK_FALSE(wasStartOperationCalled);

  // Simulate memory response
  bus.data = 0x12;

  // Step 2: Read from target address
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 2);
  CHECK(bus.address == 0x1234_addr);
  CHECK(bus.isRead());
  CHECK_FALSE(wasStartOperationCalled);

  // Step 3: Read from target address
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 3);
  CHECK(bus.address == Address{0xffff});
  CHECK(bus.isRead());
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

    Bus bus;

    // Step 0: Read low byte
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    CHECK(bus.address == 0x8000_addr);
    CHECK_FALSE(wasStartOperationCalled);

    // Simulate memory response
    bus.data = LoByte(addressToRead);

    // Step 1: Read high byte
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    CHECK(bus.address == 0x8001_addr);
    CHECK_FALSE(wasStartOperationCalled);

    // Simulate memory response
    bus.data = HiByte(addressToRead);

    // Step 2: Read from indexed address
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    CHECK(bus.address == expectedIndexedAddress);
    CHECK(bus.isRead());
    CHECK_FALSE(wasStartOperationCalled);

    // Simulate memory response
    bus.data = 0x99;

    // Step 3: Read from indexed address
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 3);
    CHECK(bus.address == Address{0xffff});
    CHECK(bus.isRead());

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

    Bus bus = Bus::Read(startingPc);

    // Steps 0-1: Read address bytes
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    CHECK(bus.address == startingPc);
    bus.data = LoByte(addressToRead);

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    CHECK(bus.address == startingPc + 1);
    bus.data = HiByte(addressToRead);

    // Step 2: Should read from wrong address first (page boundary bug)
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    // Wrong address: $200F (high byte not incremented)
    CHECK(bus.address == wrongAddress);
    bus.data = 0xff;  // Dummy read

    // Step 3: Read from correct address
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 3);
    CHECK(bus.address == expectedIndexedAddress);  // Correct address
    bus.data = 0x42;

    // Step 4: Start operation
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 4);
    CHECK(wasStartOperationCalled);
  }
}

TEST_CASE("Absolute write addressing mode", "[addressing][absolute][write]")
{
  Mos6502 cpu;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestReadInstruction);  // Set a test instruction

  Address startingPc = 0x8000_addr;
  Address addressToRead = 0x1234_addr;

  SECTION("Basic write operation")
  {
    cpu.set_pc(0x8000_addr);

    Bus bus;

    // Steps 0-1: Read address bytes
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 0);
    CHECK(bus.address == startingPc);

    bus.data = LoByte(addressToRead);
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 1);
    CHECK(bus.address == startingPc + 1);
    bus.data = HiByte(addressToRead);

    // Step 2: Should call StartOperation (no read from target)
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 2);

    // Verify target address was set correctly
    CHECK(cpu.target() == 0x1234_addr);

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

    Bus bus;

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    CHECK(bus.address == startingPc);
    CHECK(cpu.pc() == wrappedPc);  // PC should wrap

    bus.data = LoByte(addressToRead);
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    CHECK(bus.address == wrappedPc);
    CHECK(cpu.pc() == 0x0001_addr);
  }

  SECTION("Zero page crossing with index")
  {
    Address startingPc = 0x8000_addr;
    Address addressToRead = 0x00FF_addr;  // Will wrap to $0000 when indexed
    Address expectedIndexedAddress = 0x0100_addr;  // After wrapping

    cpu.set_pc(startingPc);
    cpu.set_x(0x01);
    cpu.setInstruction(TestReadInstruction);

    Bus bus;

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    CHECK(bus.address == startingPc);

    bus.data = LoByte(addressToRead);
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    CHECK(bus.address == startingPc + 1);

    bus.data = HiByte(addressToRead);

    // Should trigger page boundary logic even in zero page
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    CHECK(bus.address == 0x0000_addr);  // Wrong address first

    bus.data = 0x99;
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 3);
    CHECK(bus.address == expectedIndexedAddress);  // Correct address after wrapping

    bus.data = 0xAB;
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 4);
    CHECK(wasStartOperationCalled);
  }
}

struct TestStep
{
  Byte input_data;  // Data provided TO the addressing mode
  Address expected_address;  // Address the addressing mode should REQUEST
};

// Helper function for multi-step execution
// Memory is a span of TestStep structures, each containing the data to push on the bus as input to the
// CPU for that step, and the expected address the addressing mode should request.
void executeAddressingMode(Mos6502::StateFunc addressingMode, Mos6502& cpu, std::span<TestStep> memory)
{
  Bus bus;

  wasStartOperationCalled = false;

  Byte step = 0;
  for (auto stepData : memory)
  {
    REQUIRE_FALSE(wasStartOperationCalled);

    bus.data = stepData.input_data;
    bus = addressingMode(cpu, bus, step++);
    REQUIRE(bus.address == stepData.expected_address);
  }

  CHECK(wasStartOperationCalled);
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

  TestStep memory[] = {
      {0x00, programCounter},  // Request for lo byte (data is ignored)
      {LoByte(baseAddress), programCounter + 1},  // Request for hi byte (data is low byte)
      {HiByte(baseAddress), expectedIndexedAddress},  // Request to read operand (data is high byte)
      {0xAB, 0xffff_addr},  // Target value
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

  Bus bus;

  // Step 0: Read low byte address
  bus = AddressMode::absoluteWrite<Index::None>(cpu, Bus{}, 0);
  CHECK(bus.address == 0x1000_addr);
  CHECK(bus.isRead());
  CHECK(cpu.pc() == 0x1001_addr);

  // Step 1: Read high byte address (simulate reading 0x34 as low byte)
  bus.data = 0x34;
  bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 1);
  CHECK(bus.address == 0x1001_addr);
  CHECK(bus.isRead());
  CHECK(cpu.pc() == 0x1002_addr);
  CHECK(LoByte(cpu.target()) == 0x34);

  // Step 2: Calculate target address and start operation (simulate reading 0x12 as high byte)
  bus.data = 0x12;
  bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 2);
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

  TestStep memory[] = {
      {0x00, programCounter},  // Request for lo byte (data is ignored)
      {LoByte(baseAddress), programCounter + 1},  // Request for hi byte (data is low byte)
      {HiByte(baseAddress), expectedIndexedAddress},  // Request to write (data is high byte)
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
  Bus bus = AddressMode::absoluteWrite<Index::X>(cpu, Bus{}, 0);
  CHECK(bus.address == 0x1000_addr);
  CHECK(bus.isRead());

  // Step 1: Read high byte address (simulate reading 0xFF as low byte)
  bus.data = 0xFF;
  bus = AddressMode::absoluteWrite<Index::X>(cpu, bus, 1);
  CHECK(bus.address == 0x1001_addr);
  CHECK(bus.isRead());

  // Step 2: Page boundary crossing (0xFF + 0x10 = 0x10F, wraps to 0x0F)
  bus.data = 0x10;
  bus = AddressMode::absoluteWrite<Index::X>(cpu, bus, 2);
  CHECK(bus.address == 0x100F_addr);  // Wrong address due to page boundary crossing
  CHECK(bus.isRead());
  CHECK(wasStartOperationCalled == false);

  // Step 3: Extra cycle due to page boundary crossing
  bus.data = 0x00;  // Simulate reading from the incorrect address
  bus = AddressMode::absoluteWrite<Index::X>(cpu, bus, 3);
  CHECK(wasStartOperationCalled == true);
  CHECK(bus.address == 0x110F_addr);
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
  Bus bus = AddressMode::absoluteWrite<Index::Y>(cpu, Bus{}, 0);
  CHECK(bus.address == programCounter);
  CHECK(bus.isRead());

  // Step 1: Read high byte address (simulate reading 0x30 as low byte)
  bus.data = LoByte(baseAddress);
  bus = AddressMode::absoluteWrite<Index::Y>(cpu, bus, 1);
  CHECK(bus.address == programCounter + 1);
  CHECK(bus.isRead());

  // Step 2: No page boundary crossing (0x30 + 0x08 = 0x38, no overflow)
  bus.data = HiByte(baseAddress);
  bus = AddressMode::absoluteWrite<Index::Y>(cpu, bus, 2);
  CHECK(bus.address == expectedIndexedAddress);
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

  TestStep memory[] = {
      {0x00, programCounter},  // Request for lo byte (data is ignored)
      {LoByte(baseAddress), programCounter + 1},  // Request for hi byte (data is low byte)
      {HiByte(baseAddress), expectedWrongAddress},  // Request to read wrong address (data is high byte)
      {0x99, expectedIndexedAddress},  // Request to read operand (data is random)
  };

  executeAddressingMode(AddressMode::absoluteWrite<Index::Y>, cpu, memory);
}

TEST_CASE("AddressMode::absoluteWrite<Index::None> - Logging verification")
{
  Mos6502 cpu;
  cpu.setInstruction(TestReadInstruction);
  cpu.set_pc(0x8000_addr);

  Bus bus;

  // Step 0: Read low byte address
  AddressMode::absoluteWrite<Index::None>(cpu, bus, 0);

  // Step 1: Verify low byte is logged
  bus.data = 0xCD;
  AddressMode::absoluteWrite<Index::None>(cpu, bus, 1);
  // Note: Can't directly test logging without access to m_log, but the code path is exercised

  // Step 2: Verify high byte is logged and operand is set
  bus.data = 0xAB;
  AddressMode::absoluteWrite<Index::None>(cpu, bus, 2);
  CHECK(cpu.target() == 0xABCD_addr);
}

TEST_CASE("AddressMode::absoluteWrite<Index::X> - Edge case: X register is 0")
{
  Mos6502 cpu;
  cpu.setInstruction(TestReadInstruction);
  cpu.set_pc(0x2000_addr);
  cpu.set_x(0x00);
  wasStartOperationCalled = false;

  Bus bus;

  // Should behave like non-indexed mode when X = 0
  AddressMode::absoluteWrite<Index::X>(cpu, bus, 0);
  bus.data = 0x50;
  AddressMode::absoluteWrite<Index::X>(cpu, bus, 1);
  bus.data = 0x60;  // High byte
  AddressMode::absoluteWrite<Index::X>(cpu, bus, 2);

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

  Bus bus;

  AddressMode::absoluteWrite<Index::Y>(cpu, bus, 0);
  bus.data = 0x01;
  AddressMode::absoluteWrite<Index::Y>(cpu, bus, 1);  // Low byte = 0x01
  bus.data = 0x10;
  Bus result = AddressMode::absoluteWrite<Index::Y>(cpu, bus, 2);  // High byte = 0x10

  // 0x01 + 0xFF = 0x100_addr, so page boundary is crossed
  CHECK(cpu.target() == 0x1100_addr);  // 0x1001_addr + 0xFF
  CHECK(result.address == 0x1000_addr);  // Wrong address (0x1001_addr wrapped to 0x1000_addr)
  CHECK(result.isRead());
  CHECK(wasStartOperationCalled == false);

  // Step 3: Complete the addressing mode
  bus.data = 0x00;
  AddressMode::absoluteWrite<Index::Y>(cpu, bus, 3);
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

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {0x42, zpAddress},  // Fetch from zero page address
        {0xAB, 0xffff_addr},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::None>, cpu, memory);
  }

  SECTION("X Read addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = zpBase + 5;

    cpu.set_x(0x05);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Read base address (discarded)
        {0xAB, 0xffff_addr},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::X>, cpu, memory);
  }

  SECTION("X Read addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.set_x(0x05);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Read from wrapped address
        {0xCD, 0xffff_addr},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,Y Read addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.set_y(0x03);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Read expected address
        {0xEF, 0xffff_addr},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::Y>, cpu, memory);
  }

  SECTION("Zero Page,Y Read addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.set_y(0x03);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Read expected address
        {0x12, 0xffff_addr},  // Target operand value
    };

    executeAddressingMode(AddressMode::zeroPageRead<Index::Y>, cpu, memory);
  }
}

TEST_CASE("Zero Page Write addressing mode", "[addressing][zeropage]")
{
  Address programCounter = 0x8000_addr;

  Mos6502 cpu;
  cpu.set_pc(programCounter);

  cpu.setInstruction(TestWriteInstruction);

  SECTION("Basic functionality")
  {
    Address zpAddress = 0x0042_addr;
    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {0x42, zpAddress},  // Write to zero page address
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::None>, cpu, memory);
  }

  SECTION("Zero Page,X Write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0047_addr;  // 0x42 + 0x05

    cpu.set_x(0x05);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Read expected address
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,X Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FE_addr;
    Address expectedAddress = 0x0003_addr;  // (0xFE + 0x05) & 0xFF = 0x03

    cpu.set_x(0x05);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  //
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::X>, cpu, memory);
  }

  SECTION("Zero Page,Y Write addressing mode - no wrap")
  {
    Address zpBase = 0x0042_addr;
    Address expectedAddress = 0x0045_addr;  // 0x42 + 0x03

    cpu.set_y(0x03);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  //
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::Y>, cpu, memory);
  }

  SECTION("Zero Page,Y Write addressing mode - with wrap")
  {
    Address zpBase = 0x00FF_addr;
    Address expectedAddress = 0x0002_addr;  // (0xFF + 0x03) & 0xFF = 0x02

    cpu.set_y(0x03);

    TestStep memory[] = {
        {0x00, programCounter},  // Fetch opcode
        {LoByte(zpBase), expectedAddress},  // Write to wrapped address
    };

    executeAddressingMode(AddressMode::zeroPageWrite<Index::Y>, cpu, memory);
  }
}
