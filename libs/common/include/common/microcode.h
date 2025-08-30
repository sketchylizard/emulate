#pragma once

namespace Common
{

//! By inheriting from this template, you define the types needed to enable your CPU definition to
//! work with MicrocodePump. You must define all of your microcode functions to take a BusResponse and return a
//! Response.

template<typename StateType, typename BusResponseType, typename BusRequestType>
struct MicrocodeDefinition
{
  using State = StateType;
  using BusRequest = BusRequestType;
  using BusResponse = BusResponseType;

  struct Response;

  using Microcode = Response (*)(State&, BusResponse);

  struct Response
  {
    // The bus request to issue this cycle
    BusRequest request;
    // A pointer to a microcode function to inject into the stream, or nullptr if none.
    Microcode injection = nullptr;
  };

  // Your microcode functions can return any bus request and optionally inject another microcode function. For instance,
  // if you need to handle a page crossing penalty, you can return the bus request for the current cycle that reads the
  // wrong page, and inject a microcode function that adjust the page and issues the correct bus request on the next
  // cycle.

  // You must define two static methods to pump microcodes into the MicrocodePump:
  // 1. fetchNextOpcode(State& state) - returns a BusRequest that fetches the next. It will
  //    pass the current State so it can use the program counter to determine the address.
  // 2. decode(uint8_t opcode) - takes the fetched opcode and returns a pair of pointers to the
  //    beginning and end of the microcode sequence for that opcode
};

}  // namespace Common
