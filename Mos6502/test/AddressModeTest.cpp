#include "src/AddressMode.h"

#include <catch2/catch_test_macros.hpp>

#include "Mos6502/Memory.h"
#include "Mos6502/Mos6502.h"

static bool wasStartOperationCalled = false;

static Bus TestStartOperation(Mos6502& cpu, Bus bus, Byte /*step*/)
{
  static_cast<void>(cpu);
  wasStartOperationCalled = true;
  return bus;
}

static Bus TestOperation(Mos6502& cpu, Bus bus, Byte /*step*/)
{
  static_cast<void>(cpu);
  return bus;
}

static Mos6502::Instruction TestInstruction{"Test", TestStartOperation, TestOperation};

TEST_CASE("Absolute addressing mode - basic functionality", "[addressing][absolute]")
{
  Mos6502 cpu;
  Memory<Byte> memory;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestInstruction);  // Set a test instruction

  // Setup: LDA $1234 (3 bytes: opcode, $34, $12)
  cpu.set_pc(Address{0x8000});
  memory[Address{0x8000}] = 0x34;  // Low byte of address
  memory[Address{0x8001}] = 0x12;  // High byte of address
  memory[Address{0x1234}] = 0x42;  // Value at target address

  Bus bus = Bus::Read(Address{0x8000});

  // Step 0: Read low byte
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 0);
  REQUIRE(bus.address == Address{0x8000});
  REQUIRE(bus.isRead());

  // Simulate memory response
  bus = memory.Tick(bus);
  REQUIRE(bus.data == 0x34);
  REQUIRE(cpu.pc() == Address{0x8001});
  CHECK_FALSE(wasStartOperationCalled);

  // Step 1: Read high byte
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 1);
  REQUIRE(bus.address == Address{0x8001});
  REQUIRE(bus.isRead());

  // Simulate memory response
  bus = memory.Tick(bus);
  REQUIRE(bus.data == 0x12);
  REQUIRE(cpu.pc() == Address{0x8002});
  CHECK_FALSE(wasStartOperationCalled);

  // Step 2: Read from target address
  bus = AddressMode::absoluteRead<Index::None>(cpu, bus, 2);
  REQUIRE(bus.address == Address{0x1234});
  REQUIRE(bus.isRead());
  REQUIRE(cpu.target() == Address{0x1234});

  // Simulate memory response
  bus = memory.Tick(bus);
  REQUIRE(bus.data == 0x42);

  CHECK(wasStartOperationCalled);
}

TEST_CASE("Absolute,X addressing mode", "[addressing][absolute][indexed]")
{
  Mos6502 cpu;
  Memory<Byte> memory;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestInstruction);  // Set a test instruction

  SECTION("No page boundary crossing")
  {
    cpu.set_pc(Address{0x8000});
    cpu.set_x(0x05);
    memory[Address{0x8000}] = 0x10;  // Low byte
    memory[Address{0x8001}] = 0x20;  // High byte ($2010)
    memory[Address{0x2015}] = 0x99;  // Value at $2010 + $05

    Bus bus = Bus::Read(Address{0x8000});

    // Step 0: Read low byte
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    bus = memory.Tick(bus);
    CHECK_FALSE(wasStartOperationCalled);

    // Step 1: Read high byte
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    bus = memory.Tick(bus);

    // Step 2: Read from indexed address
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    REQUIRE(bus.address == Address{0x2015});  // $2010 + $05
    REQUIRE(bus.isRead());
    CHECK_FALSE(wasStartOperationCalled);

    bus = memory.Tick(bus);
    REQUIRE(bus.data == 0x99);
    CHECK_FALSE(wasStartOperationCalled);
  }

  SECTION("Page boundary crossing")
  {
    cpu.set_pc(Address{0x8000});
    cpu.set_x(0x10);
    memory[Address{0x8000}] = 0xFF;  // Low byte
    memory[Address{0x8001}] = 0x20;  // High byte ($20FF)
    memory[Address{0x210F}] = 0x88;  // Value at $20FF + $10 = $210F

    Bus bus = Bus::Read(Address{0x8000});

    // Steps 0-1: Read address bytes
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    bus = memory.Tick(bus);
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    bus = memory.Tick(bus);

    // Step 2: Should read from wrong address first (page boundary bug)
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    // Wrong address: $200F (high byte not incremented)
    REQUIRE(bus.address == Address{0x200F});
    bus = memory.Tick(bus);  // Dummy read

    // Step 3: Read from correct address
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 3);
    REQUIRE(bus.address == Address{0x210F});  // Correct address
    bus = memory.Tick(bus);
    REQUIRE(bus.data == 0x88);
  }
}

TEST_CASE("Absolute write addressing mode", "[addressing][absolute][write]")
{
  Mos6502 cpu;
  Memory<Byte> memory;

  wasStartOperationCalled = false;

  cpu.setInstruction(TestInstruction);  // Set a test instruction

  SECTION("Basic write operation")
  {
    cpu.set_pc(Address{0x8000});
    memory[Address{0x8000}] = 0x34;
    memory[Address{0x8001}] = 0x12;

    Bus bus = Bus::Read(Address{0x8000});

    // Steps 0-1: Read address bytes
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 0);
    bus = memory.Tick(bus);
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 1);
    bus = memory.Tick(bus);

    // Step 2: Should call StartOperation (no read from target)
    bus = AddressMode::absoluteWrite<Index::None>(cpu, bus, 2);

    // Verify target address was set correctly
    REQUIRE(cpu.target() == Address{0x1234});

    CHECK(wasStartOperationCalled);
  }
}

TEST_CASE("Absolute addressing edge cases", "[addressing][absolute][edge]")
{
  Mos6502 cpu;
  Memory<Byte> memory;

  SECTION("Address wrapping at 16-bit boundary")
  {
    cpu.set_pc(Address{0xFFFF});
    cpu.set_x(0x01);
    memory[Address{0xFFFF}] = 0xFF;
    memory[Address{0x0000}] = 0xFF;  // Wraps to $0000

    Bus bus = Bus::Read(Address{0xFFFF});

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    bus = memory.Tick(bus);
    REQUIRE(cpu.pc() == Address{0x0000});  // PC should wrap

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    bus = memory.Tick(bus);
    REQUIRE(cpu.pc() == Address{0x0001});  // PC continues wrapping
  }

  SECTION("Zero page crossing with index")
  {
    cpu.set_pc(Address{0x8000});
    cpu.set_x(0x01);
    memory[Address{0x8000}] = 0xFF;  // $00FF
    memory[Address{0x8001}] = 0x00;

    Bus bus = Bus::Read(Address{0x8000});

    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 0);
    bus = memory.Tick(bus);
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 1);
    bus = memory.Tick(bus);

    // Should trigger page boundary logic even in zero page
    bus = AddressMode::absoluteRead<Index::X>(cpu, bus, 2);
    REQUIRE(bus.address == Address{0x00FF});  // Wrong address first
  }
}

// Helper function for multi-step execution
std::pair<Bus, bool> executeAddressingMode(
    std::function<Bus(Mos6502&, Bus, size_t)> addressingMode, Mos6502& cpu, Memory<Byte>& memory, size_t maxSteps = 10)
{
  Bus bus = Bus::Read(cpu.pc());

  for (Byte step = 0; step < maxSteps; ++step)
  {
    bus = memory.Tick(bus);  // Get memory response first
    bus = addressingMode(cpu, bus, step);

    // Check if we called StartOperation (would need cpu state inspection)
    // This is implementation-specific
    if (wasStartOperationCalled)
    {
      return {bus, true};
    }
  }

  return {bus, false};  // Didn't complete in maxSteps
}

TEST_CASE("Complete addressing mode execution", "[addressing][integration]")
{
  Mos6502 cpu;
  Memory<Byte> memory;

  // Setup a complete scenario
  cpu.set_pc(Address{0x8000});
  cpu.set_x(0x05);
  memory[Address{0x8000}] = 0x00;
  memory[Address{0x8001}] = 0x30;  // $3000
  memory[Address{0x3005}] = 0xAB;  // Target value

  auto [finalBus, completed] = executeAddressingMode(AddressMode::absoluteRead<Index::X>, cpu, memory);

  REQUIRE(completed);
  REQUIRE(cpu.operand() == 0xAB);
  REQUIRE(cpu.target() == Address{0x3005});
}
