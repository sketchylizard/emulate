#pragma once

#include "common/Bus.h"
#include "core65xx/core65xx.h"

enum class Index
{
  None,
  X,
  Y
};

struct AddressMode
{
  static Common::BusRequest acc(Core65xx& cpu, Common::BusResponse response);
  static Common::BusRequest imp(Core65xx& cpu, Common::BusResponse response);
  static Common::BusRequest imm(Core65xx& cpu, Common::BusResponse response);
  static Common::BusRequest imm1(Core65xx& cpu, Common::BusResponse response);
  static Common::BusRequest rel(Core65xx& cpu, Common::BusResponse response);
  static Common::BusRequest rel1(Core65xx& cpu, Common::BusResponse response);

  template<Index index>
  static Common::BusRequest zp0(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_operand.type = Core65xx::Operand::Type::Zp;
    cpu.m_action = &AddressMode::zp1<index>;
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  [[nodiscard]] static bool incrementByte(Common::Byte& byte, Common::Byte inc) noexcept
  {
    const uint16_t sum = static_cast<uint16_t>(static_cast<uint16_t>(byte) + inc);
    byte = static_cast<Common::Byte>(sum & 0xFF);
    return (sum & 0x100) != 0;
  }

  template<Index index>
  static Common::BusRequest zp1(Core65xx& cpu, Common::BusResponse response)
  {
    // Zero Page Write addressing mode
    // Fetch the low byte of the address
    cpu.m_operand.lo = response.data;
    cpu.m_operand.hi = c_ZeroPage;

    if constexpr (index != Index::None)
    {
      // Add the index register to the low byte, wrapping around within the zero page.
      bool carry = incrementByte(cpu.m_operand.lo, getIndexValue<index>(cpu));
      static_cast<void>(carry);  // carry is ignored in zero page addressing
    }
    return Core65xx::nextOp(cpu, response);
  }

  template<Index index>
  static Common::BusRequest abs0(Core65xx& cpu, Common::BusResponse /*response*/)
  {
    if constexpr (index == Index::None)
      cpu.m_operand.type = Core65xx::Operand::Type::Abs;
    else if constexpr (index == Index::X)
      cpu.m_operand.type = Core65xx::Operand::Type::AbsX;
    else
      cpu.m_operand.type = Core65xx::Operand::Type::AbsY;

    // Put the address of the lo byte on the BusRequest
    cpu.m_action = &AddressMode::abs1<index>;
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  template<Index index>
  static Common::BusRequest abs1(Core65xx& cpu, Common::BusResponse response)
  {
    // Read the lo byte from the BusRequest
    cpu.m_operand.lo = response.data;

    cpu.m_action = &AddressMode::abs2<index>;
    // Put the address of the hi byte on the bus.
    return Common::BusRequest::Read(cpu.regs.pc++);
  }
  template<Index index>
  static Common::BusRequest abs2(Core65xx& cpu, Common::BusResponse response)
  {
    // Read the hi byte from the response.
    cpu.m_operand.hi = response.data;

    // If this is an indexed addressing mode, add the index register to the lo byte.
    // Note: This can cause the address to wrap around if it exceeds 0xFF which will
    // give us the wrong address. On the 6502, this is known as "page boundary crossing"
    // and it causes an extra read cycle. We emulate it here for accuracy.

    bool carry = false;
    if constexpr (index != Index::None)  // overflow occurred
    {
      carry = incrementByte(cpu.m_operand.lo, getIndexValue<index>(cpu));
    }

    // If carry is set, then target is the wrong address, but we need to read from it to emulate the 6502's,
    // then we need to increment the hi byte to and read it on the next cycle. If no carry, then target is
    // correct and we're done.
    if (carry)
    {
      cpu.m_action = &AddressMode::abs3<index>;
      return Common::BusRequest::Read(cpu.getEffectiveAddress());
    }
    return Core65xx::nextOp(cpu, response);
  }

  template<Index index>
  static Common::BusRequest abs3(Core65xx& cpu, Common::BusResponse response)
  {
    // We read from the wrong address due to page boundary crossing.
    ++cpu.m_operand.hi;  // increment hi byte to get the correct address
    return Core65xx::nextOp(cpu, response);
  }

  template<Index index>
    requires(index != Index::None)
  static Common::BusRequest indirect(Core65xx& cpu, Common::BusResponse response)
  {
    if constexpr (index == Index::None)
      cpu.m_operand.type = Core65xx::Operand::Type::Ind;
    else if constexpr (index == Index::X)
      cpu.m_operand.type = Core65xx::Operand::Type::IndZpX;
    else
      cpu.m_operand.type = Core65xx::Operand::Type::IndZpY;
    return Core65xx::nextOp(cpu, response);
  }

  static Common::BusRequest Fetch(Core65xx& cpu, Common::BusResponse response);

  static constexpr auto abs = &AddressMode::abs0<Index::None>;
  static constexpr auto absX = &AddressMode::abs0<Index::X>;
  static constexpr auto absY = &AddressMode::abs0<Index::Y>;

  static constexpr auto zp = &AddressMode::zp0<Index::None>;
  static constexpr auto zpX = &AddressMode::zp0<Index::X>;
  static constexpr auto zpY = &AddressMode::zp0<Index::Y>;

  template<Index index>
    requires(index != Index::None)
  static Common::Byte getIndexValue(const Core65xx& cpu) noexcept
  {
    if constexpr (index == Index::X)
    {
      return cpu.regs.x;
    }
    else
    {
      assert((index == Index::Y));
      return cpu.regs.y;
    }
  }
};
