"I'm building a cycle-accurate 6502 emulator called EmuLate in C++. Key architectural decisions from previous discussions:

## Core Architecture:

- Function-chain based microcode approach where each microcode function represents one bus cycle
- Template-based CoreCPU class that works with any 8-bit CPU state/instruction set.
- Templated on a CpuState that defines the signature for microcode operations and the opcode's Instruction struct.
- Instruction defines one or more microcode function pointers that define different bus states/operations
- CPU ticks advance through the microcode, but microcode can inject new operations midstream.
- Bus-driven timing model with BusRequest/BusResponse for cycle accuracy.
  CPU makes BusRequests, devices return BusResponses
- Tick-driven: Everything operates on clock cycles
- C++ Concept-driven design: Uses C++20 concepts for type safety and flexibility
- Address modes are defined as arrays of microcode that gets injected in front of the operation microcode stream, allowing reuse.

## Key Components:

```
using Byte = uint8_t;

//! 16-bit address type
enum class Address : uint16_t
{
};

struct BusRequest {
    uint16_t address; 
    uint8_t data;
    uint8_t control; // READ/WRITE flags
};

struct BusResponse {
    uint8_t data;
    uint8_t control; // IRQ lines, RDY, etc.
};

using Microcode = Common::BusRequest (*)(State&, Common::BusResponse response);

struct Instruction
{
  Common::Byte opcode = 0;
  const char* mnemonic = "???";
  Microcode ops[7] = {};  // sequence of microcode functions to execute
};
```

## Device System

### BusDevice concept

- Anything with `BusResponse tick(const BusRequest&)` can be a bus device
- Any object with `Byte operator[](Address)` meets the Readable concept
- Any object with `Byte& operator[](Address)` meets the Writeable concept
- Anything that meets the Readable or Writable concept can be automatically wrapped as a MemoryDevice providing readonly, or read/write capabilities. (i.e. C arrays, std::array, stdvector, std::span)
- AddressRemapper: Handles address translation AND bank switching so a bus device can be mapped to a specific address range. For instance, a `std::array<Byte, 0x1000>` can be mapped to appear to start at address 0xC000 instead of address 0.
- AddressRemapper also handles multiple banks (i.e. multiple BusDevice instances) so that they all appear at the same address offset, but only the current bank responds to bus ticks. 
- Memory maps should be composable at compile time

## Current Status:

- Implemented immediate, zero page, zero page indexed, absolute, and absolute indexed addressing modes for MOS 6502
- Built comprehensive test framework defining BusResponse for each tick and expected bus requests after each tick
- Decent tests for 6502 addressing modes
- Many opcodes defined but not well tested
- Working on CoreCPU class with tick() function and instruction sequencing
- Added null state to BusRequest for instruction completion signaling. MicrocodePump interprets a null bus request as a request to fetch the next opcode and will insert a fetch into the stream.

## Next Steps:

- Finish CoreCPU implementation and testing
- Connect to Klaus Dormann test suite
- Implement remaining opcodes
- Implement remaining addressing modes (indirect, etc.)"
