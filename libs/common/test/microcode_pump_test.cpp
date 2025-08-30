#include <catch2/catch_test_macros.hpp>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "common/address.h"
#include "common/address_string_maker.h"
#include "common/microcode_pump.h"

using namespace Common;

struct TestState
{
  using Microcode = Common::BusRequest (*)(TestState&, Common::BusResponse response);

  struct Instruction
  {
    Common::Byte opcode = 0;
    const char* mnemonic = "???";
    Microcode ops[7] = {};  // sequence of microcode functions to execute
  };

  Common::Address pc{0};
  Byte a = 0;
  Byte x = 0;

  // Next microcode action to perform, or null if none
  Microcode next = nullptr;

  // Test tracking variables
  std::vector<std::string> executionTrace;
  int step1Called = 0;
  int step2Called = 0;
  int step3Called = 0;
  bool scheduledCalled = false;

  void reset()
  {
    executionTrace.clear();
    step1Called = step2Called = step3Called = 0;
    scheduledCalled = false;
  }
};

// Test microcode functions
namespace TestMicrocode
{

Common::BusRequest requestOperand(TestState& state, Common::BusResponse /*response*/)
{
  state.executionTrace.push_back("requestOperand");
  return Common::BusRequest::Read(state.pc++);
}

Common::BusRequest step1(TestState& state, Common::BusResponse /*response*/)
{
  state.step1Called++;
  state.executionTrace.push_back("step1");
  return Common::BusRequest::Read(0x1000_addr);
}

Common::BusRequest step2(TestState& state, Common::BusResponse /*response*/)
{
  state.step2Called++;
  state.executionTrace.push_back("step2");
  return Common::BusRequest::Read(0x2000_addr);
}

Common::BusRequest step3(TestState& state, Common::BusResponse /*response*/)
{
  state.step3Called++;
  state.executionTrace.push_back("step3");
  return Common::BusRequest::Read(0x3000_addr);
}

Common::BusRequest scheduledStep(TestState& state, Common::BusResponse /*response*/)
{
  state.scheduledCalled = true;
  state.executionTrace.push_back("scheduledStep");
  return Common::BusRequest::Read(0x5000_addr);
}

Common::BusRequest scheduleNext(TestState& state, Common::BusResponse /*response*/)
{
  state.executionTrace.push_back("scheduleNext");
  state.next = &scheduledStep;
  return Common::BusRequest::Read(0x4000_addr);
}

Common::BusRequest loadA(TestState& state, Common::BusResponse response)
{
  state.a = response.data;
  state.executionTrace.push_back("loadA");
  return Common::BusRequest{};  // No more bus activity
}
}  // namespace TestMicrocode

// In common/test_helpers.h or wherever you put the test utilities
// Helper function for readable output
std::string formatBusRequest(const Common::BusRequest& request)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');

  if (request.isRead())
  {
    oss << "READ($" << request.address << ")";
    if (request.isSync())
      oss << " SYNC";
  }
  else if (request.isWrite())
  {
    oss << "WRITE($" << request.address << ", $" << std::setw(2) << static_cast<int>(request.data) << ")";
  }
  else
  {
    oss << "NONE";
  }

  return oss.str();
}

// In common/test_helpers.h
template<typename CpuType>
bool execute(CpuType& cpu, Common::Byte input, Common::BusRequest expected)
{
  Common::BusRequest actual = cpu.tick(Common::BusResponse{input});

  if (actual != expected)
  {
    UNSCOPED_INFO("Cycle mismatch:");
    UNSCOPED_INFO("  Expected: " << formatBusRequest(expected));
    UNSCOPED_INFO("  Actual:   " << formatBusRequest(actual));
    UNSCOPED_INFO("  Input:    $" << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(input));
    return false;
  }

  return true;
}

TEST_CASE("MicrocodePump basic execution", "[corecpu]")
{
  // Create instruction table
  std::array<TestState::Instruction, 256> instructions{};

  // Define a test instruction with 3 steps
  instructions[0x42] = {
      .opcode = 0x42, .mnemonic = "TEST", .ops = {&TestMicrocode::step1, &TestMicrocode::step2, &TestMicrocode::step3}};

  MicrocodePump<TestState> cpu{instructions};
  TestState& state = cpu.getState();
  state.pc = 0x8000_addr;
  state.reset();

  SECTION("Execute complete instruction")
  {
    // Cycle 1: Fetch opcode
    CHECK(execute(cpu, 0x00, Common::BusRequest::Fetch(0x8000_addr)));
    CHECK(state.pc == 0x8001_addr);  // PC should increment

    // Cycle 2: Decode opcode 0x42 and execute step1
    CHECK(execute(cpu, 0x42, Common::BusRequest::Read(0x1000_addr)));  // step1's address
    CHECK(state.step1Called == 1);

    // Cycle 3: Execute step2
    CHECK(execute(cpu, 0x00, Common::BusRequest::Read(0x2000_addr)));  // step2's address
    CHECK(state.step2Called == 1);

    // Cycle 4: Execute step3
    CHECK(execute(cpu, 0x00, Common::BusRequest::Read(0x3000_addr)));  // step3's address
    CHECK(state.step3Called == 1);

    // Cycle 5: Instruction complete, fetch next opcode
    CHECK(execute(cpu, 0x00, Common::BusRequest::Fetch(0x8001_addr)));

    // Verify execution order
    std::vector<std::string> expected = {"step1", "step2", "step3"};
    CHECK(state.executionTrace == expected);
  }
}

TEST_CASE("MicrocodePump scheduled microcode", "[corecpu]")
{
  std::array<TestState::Instruction, 256> instructions{};

  instructions[0x43] = {.opcode = 0x43, .mnemonic = "SCHED", .ops = {&TestMicrocode::scheduleNext, &TestMicrocode::step2}};

  MicrocodePump<TestState> cpu{instructions};
  TestState& state = cpu.getState();
  state.reset();

  // Fetch and decode instruction
  static_cast<void>(cpu.tick(Common::BusResponse{0x00}));
  static_cast<void>(cpu.tick(Common::BusResponse{0x43}));  // Execute scheduleNext

  // Next tick should execute scheduled microcode, not step2
  [[maybe_unused]] auto request = cpu.tick(Common::BusResponse{0x00});
  CHECK(state.scheduledCalled);
  CHECK(state.step2Called == 0);  // step2 should be skipped

  // After scheduled completes, should continue with step2
  static_cast<void>(cpu.tick(Common::BusResponse{0x00}));
  CHECK(state.step2Called == 1);
}

TEST_CASE("MicrocodePump instruction with data processing", "[corecpu]")
{
  std::array<TestState::Instruction, 256> instructions{};

  instructions[0x44] = {.opcode = 0x44, .mnemonic = "LOAD", .ops = {&TestMicrocode::requestOperand, &TestMicrocode::loadA}};

  MicrocodePump<TestState> cpu{instructions};
  TestState& state = cpu.getState();
  state.reset();

  CHECK(execute(cpu, 0x00, Common::BusRequest::Fetch(0x0000_addr)));  // Fetch opcode
  CHECK(execute(cpu, 0x44, Common::BusRequest::Read(0x0001_addr)));  // Return opcode / request operand
  CHECK(execute(cpu, 0x99, Common::BusRequest::Fetch(0x0002_addr)));  // Return data to load / next opcode fetch

  CHECK(state.a == 0x99);
  CHECK(state.executionTrace == std::vector<std::string>{"requestOperand", "loadA"});
}
