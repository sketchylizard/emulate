#include "cpu6502/address_mode.h"

namespace cpu6502
{

Common::BusRequest AddressMode::requestOpcode(State& cpu, Common::BusResponse /*response*/)
{
  // Fetch the next opcode
  return Common::BusRequest::Fetch(cpu.pc++);
}

Common::BusRequest AddressMode::requestOperandLow(State& cpu, Common::BusResponse /*response*/)
{
  // We don't care about the response; just fetch the low byte of the address
  cpu.lo = cpu.hi = 0;  // clear hi/lo to ensure they are set
  return Common::BusRequest::Read(cpu.pc++);
}

Common::BusRequest AddressMode::requestOperandHigh(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the low byte of the address
  cpu.lo = response.data;
  return Common::BusRequest::Read(cpu.pc++);
}

Common::BusRequest AddressMode::requestEffectiveAddress(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the high byte of the address
  cpu.hi = response.data;
  return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, cpu.hi));
}

Common::BusRequest AddressMode::requestZeroPageAddress(State& cpu, Common::BusResponse response)
{
  // The incoming response data is the low byte of the address (the high byte is always 0)
  cpu.lo = response.data;
  return Common::BusRequest::Read(Common::MakeAddress(cpu.lo, 0x00));
}

}  // namespace cpu6502
