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

MicrocodeResponse AddressMode::requestIndirectHigh(State& state, BusResponse response)
{
  // The incoming response data is the low byte of the pointer.
  Common::Byte save = response.data;

  // Increment the pointer address (wrapping within zero page) to get the high byte
  ++state.lo;

  auto effectiveAddr = Common::MakeAddress(state.lo, 0x00);

  // Now we can start forming the effective address for the next operation.
  state.lo = save;
  // The high byte will be filled in when we read from the incremented pointer address.

  return {BusRequest::Read(effectiveAddr)};  // Read High byte from pointer
}

MicrocodeResponse IndirectZeroPageX::requestIndirect16(State& state, BusResponse /*response*/)
{
  // The incoming response data is the low byte of a zero page address
  // which we have already stored in state.lo
  auto effectiveAddr = Common::MakeAddress(state.lo, 0x00);
  return {BusRequest::Read(effectiveAddr), requestIndirectHigh};  // Read LOW byte from pointer
}

MicrocodeResponse IndirectZeroPageY::requestIndirect16(State& state, BusResponse response)
{
  // The incoming response data is the low byte of a zero page address
  state.lo = response.data;

  auto effectiveAddr = Common::MakeAddress(state.lo, 0x00);
  return {BusRequest::Read(effectiveAddr), &AddressMode::requestIndirectHigh};  // Read LOW byte from pointer
}


}  // namespace cpu6502
