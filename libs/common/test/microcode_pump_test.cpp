#include <array>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "common/address.h"
#include "common/bus.h"
#include "common/microcode.h"
#include "common/microcode_pump.h"

using namespace Common;

#if 0

// In common/test_helpers.h or wherever you put the test utilities
// Helper function for readable output
std::ostream& operator<<(std::ostream& os, const BusRequest& value)
{
  if (value.isSync())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isRead())
  {
    os << std::format("Bus read(${:04x})", value.address);
  }
  else if (value.isWrite())
  {
    os << std::format("Bus write(${:04x}, ${:02x})", value.address, value.data);
  }
  os << "NONE";
  return os;
}


// CPU state
struct TestState
{
  Address pc{0};
  Byte a = 0;
  Byte x = 0;

  // Test tracking variables
  std::vector<std::string> executionTrace;
  int step1Called = 0;
  int step2Called = 0;
  int step3Called = 0;
  bool scheduledCalled = false;
};

struct TestCpuDefinition : ProcessorDefinition<TestState, BusResponse, BusRequest>
{
  static constexpr int c_maxOps = 10;

  // We only have one opcode for testing
  static Byte opcode;
  static std::pair<Microcode*, Microcode*> microcodes;

  static Response fetchNextOpcode(State& cpu, BusResponse) noexcept
  {
    return {BusRequest::Fetch(cpu.pc++)};
  }

  static std::pair<Microcode*, Microcode*> decodeOpcode(Common::Byte incomingOpcode) noexcept
  {
    return incomingOpcode == opcode ? microcodes : std::pair{nullptr, nullptr};
  }
};

Common::Byte TestCpuDefinition::opcode;
std::pair<TestCpuDefinition::Microcode*, TestCpuDefinition::Microcode*> TestCpuDefinition::microcodes;


using State = TestCpuDefinition::State;
using Microcode = TestCpuDefinition::Microcode;
using BusRequest = TestCpuDefinition::BusRequest;
using BusResponse = TestCpuDefinition::BusResponse;
using MicrocodeResponse = TestCpuDefinition::Response;

using namespace Common;

// Test microcode functions
auto step1 = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
{
  cpu.step1Called++;
  cpu.executionTrace.push_back("step1");
  return {BusRequest::Read(cpu.pc), nullptr};
};

auto step2 = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
{
  cpu.step2Called++;
  cpu.executionTrace.push_back("step2");
  return {BusRequest::Read(cpu.pc), nullptr};
};

auto step3 = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
{
  cpu.step3Called++;
  cpu.executionTrace.push_back("step3");
  return {BusRequest::Read(cpu.pc), nullptr};
};

auto scheduledStep = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
{
  cpu.scheduledCalled = true;
  cpu.executionTrace.push_back("scheduled");
  return {BusRequest::Read(cpu.pc), nullptr};
};

// Test fixture to set up microcode arrays
class MicrocodePumpTestFixture
{
public:
  std::array<TestCpuDefinition::Microcode, TestCpuDefinition::c_maxOps> microcodeArray;
  TestState state;
  MicrocodePump<TestCpuDefinition> pump;

  void setupMicrocodes(std::initializer_list<TestCpuDefinition::Microcode> ops)
  {
    microcodeArray.fill(nullptr);
    size_t i = 0;
    for (auto op : ops)
    {
      microcodeArray[i++] = op;
    }
    TestCpuDefinition::microcodes = {microcodeArray.data(), microcodeArray.data() + ops.size()};
  }

  void resetState()
  {
    state = TestState{};
  }
};

TEST_CASE_METHOD(MicrocodePumpTestFixture, "MicrocodePump startup behavior")
{
  SECTION("First tick issues opcode fetch")
  {
    TestCpuDefinition::opcode = 0x42;
    setupMicrocodes({step1});

    auto request = pump.tick(state, BusResponse{0x00});

    REQUIRE(request.isSync());  // Should be a fetch (SYNC) request
    REQUIRE(request.address == 0_addr);
    REQUIRE(cpu.pc == 1_addr);  // PC should be incremented by fetchNextOpcode
    REQUIRE(pump.cycles() == 1);
  }

  SECTION("Second tick decodes and executes first microcode")
  {
    TestCpuDefinition::opcode = 0x42;
    setupMicrocodes({step1});

    // First tick - fetch
    [[maybe_unused]] auto response = pump.tick(state, BusResponse{0x00});

    // Second tick - decode and execute
    [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x42});  // Send the opcode

    REQUIRE(cpu.step1Called == 1);
    REQUIRE(cpu.executionTrace == std::vector<std::string>{"step1"});
    REQUIRE(pump.cycles() == 2);
  }
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "Multi-step instruction execution")
{
  SECTION("Three-step instruction executes in order")
  {
    TestCpuDefinition::opcode = 0x55;
    setupMicrocodes({step1, step2, step3});

    // Fetch cycle
    [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});

    // Decode and execute step1
    request = pump.tick(state, BusResponse{0x55});

    // Execute step2
    request = pump.tick(state, BusResponse{0x00});

    // Execute step3
    request = pump.tick(state, BusResponse{0x00});

    REQUIRE(cpu.step1Called == 1);
    REQUIRE(cpu.step2Called == 1);
    REQUIRE(cpu.step3Called == 1);
    REQUIRE(cpu.executionTrace == std::vector<std::string>{"step1", "step2", "step3"});
    REQUIRE(pump.cycles() == 4);
  }

  SECTION("After instruction completes, fetches next opcode")
  {
    TestCpuDefinition::opcode = 0x55;
    setupMicrocodes({step1});

    Address initialPc = cpu.pc;

    // Complete first instruction
    [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});  // Fetch
    request = pump.tick(state, BusResponse{0x55});  // Execute step1

    // Next tick should fetch again
    request = pump.tick(state, BusResponse{0x00});

    REQUIRE(request.isSync());
    REQUIRE(cpu.pc == initialPc + 2);  // Should have incremented twice
  }
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "Microcode injection")
{
  auto injectingStep = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
  {
    cpu.executionTrace.push_back("injecting");
    return {BusRequest::Read(cpu.pc), scheduledStep};  // Inject scheduledStep
  };

  SECTION("Injected microcode executes before next instruction step")
  {
    TestCpuDefinition::opcode = 0x77;
    setupMicrocodes({injectingStep, step2});

    // Fetch and decode
    [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});
    request = pump.tick(state, BusResponse{0x77});  // Execute injectingStep (injects scheduledStep)

    // Next tick should execute injected step, not step2
    request = pump.tick(state, BusResponse{0x00});

    // Then step2 should execute
    request = pump.tick(state, BusResponse{0x00});

    REQUIRE(cpu.scheduledCalled == true);
    REQUIRE(cpu.step2Called == 1);
    REQUIRE(cpu.executionTrace == std::vector<std::string>{"injecting", "scheduled", "step2"});
  }

  SECTION("Multiple injections queue properly")
  {
    auto doubleInjectingStep = [](TestState& state, BusResponse /*response*/) -> TestCpuDefinition::Response
    {
      cpu.executionTrace.push_back("double_injecting");
      // Return injection that will inject again
      return {BusRequest::Read(cpu.pc), +[](TestState& state1, BusResponse) -> TestCpuDefinition::Response
          {
            state1.executionTrace.push_back("first_injected");
            return {BusRequest::Read(state1.pc), scheduledStep};  // Chain another injection
          }};
    };

    TestCpuDefinition::opcode = 0x88;
    setupMicrocodes({doubleInjectingStep});

    [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});  // Fetch
    request = pump.tick(state, BusResponse{0x88});  // Execute doubleInjectingStep
    request = pump.tick(state, BusResponse{0x00});  // Execute first injection
    request = pump.tick(state, BusResponse{0x00});  // Execute second injection

    REQUIRE(cpu.executionTrace == std::vector<std::string>{"double_injecting", "first_injected", "scheduled"});
  }
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "Unknown opcode handling")
{
  TestCpuDefinition::opcode = 0x42;  // Only recognize this opcode
  setupMicrocodes({step1});

  // Fetch
  auto request1 = pump.tick(state, BusResponse{0x00});
  REQUIRE(request1.isSync());

  // Decode unknown opcode - should be no-op, immediately fetch next
  auto request2 = pump.tick(state, BusResponse{0x99});  // Unknown opcode

  REQUIRE(request2.isSync());  // Should immediately fetch next opcode
  REQUIRE(cpu.executionTrace.empty());  // No steps executed
  REQUIRE(cpu.step1Called == 0);  // step1 should not have been called
  REQUIRE(pump.cycles() == 2);  // Two microcode operations: fetch + fetch again
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "Empty instruction handling")
{
  TestCpuDefinition::opcode = 0x42;
  setupMicrocodes({});  // No microcodes

  // Fetch
  [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});

  // Decode empty instruction - should immediately fetch next
  request = pump.tick(state, BusResponse{0x42});

  REQUIRE(request.isSync());  // Should fetch next opcode immediately
  REQUIRE(cpu.executionTrace.empty());  // No steps executed
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "Microcode counter accuracy")
{
  TestCpuDefinition::opcode = 0x42;
  setupMicrocodes({step1, step2});

  REQUIRE(pump.cycles() == 0);

  [[maybe_unused]] auto request = pump.tick(state, BusResponse{0x00});  // Fetch
  REQUIRE(pump.cycles() == 1);

  request = pump.tick(state, BusResponse{0x42});  // Decode + step1
  REQUIRE(pump.cycles() == 2);

  request = pump.tick(state, BusResponse{0x00});  // step2
  REQUIRE(pump.cycles() == 3);

  request = pump.tick(state, BusResponse{0x00});  // Next fetch
  REQUIRE(pump.cycles() == 4);
}

TEST_CASE_METHOD(MicrocodePumpTestFixture, "State isolation")
{
  TestCpuDefinition::opcode = 0x42;
  setupMicrocodes({step1});

  TestState state1, state2;
  state1.pc = 0x1000_addr;
  state1.a = 0x11;

  state2.pc = 0x2000_addr;
  state2.a = 0x22;

  // Same pump, different states
  [[maybe_unused]] auto request = pump.tick(state1, BusResponse{0x00});
  request = pump.tick(state2, BusResponse{0x00});

  REQUIRE(state1.pc == 0x1001_addr);
  REQUIRE(state1.a == 0x11);

  REQUIRE(state2.pc == 0x2001_addr);
  REQUIRE(state2.a == 0x22);
}
#endif
