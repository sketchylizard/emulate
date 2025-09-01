#include "cpu6502/address_mode.h"

namespace cpu6502
{

MicrocodeResponse AddressMode::requestOperandLow(State& cpu, BusResponse /*response*/)
{
  // We don't care about the response; just fetch the low byte of the address
  cpu.lo = cpu.hi = 0;  // clear hi/lo to ensure they are set
  return {BusRequest::Read(cpu.pc++)};
}

MicrocodeResponse AddressMode::requestOperandHigh(State& cpu, BusResponse response)
{
  // The incoming response data is the low byte of the address
  cpu.lo = response.data;
  return {BusRequest::Read(cpu.pc++)};
}

MicrocodeResponse AddressMode::requestEffectiveAddress(State& cpu, BusResponse response)
{
  // The incoming response data is the high byte of the address
  cpu.hi = response.data;
  return {BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi))};
}

}  // namespace cpu6502
