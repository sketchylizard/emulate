#include "cpu6502/address_mode.h"

namespace cpu6502
{

MicrocodeResponse AddressMode::requestAddress8(State& cpu, BusResponse /*response*/)
{
  // We don't care about the response; just fetch the low byte of the address
  cpu.lo = cpu.hi = 0;  // clear hi/lo to ensure they are set
  return {BusRequest::Read(cpu.pc++)};
}

MicrocodeResponse AddressMode::requestAddress16(State& cpu, BusResponse /*response*/)
{
  // We don't care about the response; just fetch the low byte of the address
  cpu.lo = cpu.hi = 0;  // clear hi/lo to ensure they are set
  return {BusRequest::Read(cpu.pc++), &requestAddressHigh};
}

MicrocodeResponse AddressMode::requestAddressHigh(State& cpu, BusResponse response)
{
  // The incoming response data is the low byte of the address
  cpu.lo = response.data;
  return {BusRequest::Read(cpu.pc++)};
}

MicrocodeResponse AddressMode::fixupPageCrossing(State& state, BusResponse /*response*/)
{
  // We have already read from the wrong address; now read from the correct one
  return {BusRequest::Read(Common::MakeAddress(state.lo, state.hi))};
}

}  // namespace cpu6502
