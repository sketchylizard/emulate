#pragma once

#include "Mos6502/Mos6502.h"
#include "common/Bus.h"

enum class Index
{
  None,
  X,
  Y
};

inline constexpr Common::Byte c_ZeroPage{0x00};
inline constexpr Common::Byte c_StackPage{0x01};

struct AddressMode
{
  static Common::BusRequest acc(Mos6502& cpu, Common::BusResponse response);
  static Common::BusRequest imp(Mos6502& cpu, Common::BusResponse response);
  static Common::BusRequest imm(Mos6502& cpu, Common::BusResponse response);
  static Common::BusRequest imm1(Mos6502& cpu, Common::BusResponse response);
  static Common::BusRequest rel(Mos6502& cpu, Common::BusResponse response);
  static Common::BusRequest rel1(Mos6502& cpu, Common::BusResponse response);

  static void logRelOperand(Mos6502& cpu, Common::Byte displacement);

  template<Index index>
  static Common::BusRequest zp0(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    cpu.m_action = &AddressMode::zp1<index>;
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  [[nodiscard]] static bool incrementByte(Common::Byte& byte, int8_t inc) noexcept
  {
    const uint16_t sum = static_cast<uint16_t>(static_cast<uint16_t>(byte) + inc);
    byte = static_cast<Common::Byte>(sum & 0xFF);
    return (sum & 0x100) != 0;
  }

  [[nodiscard]] static bool incrementByte(Common::Byte& byte, Common::Byte inc) noexcept
  {
    const uint16_t sum = static_cast<uint16_t>(static_cast<uint16_t>(byte) + inc);
    byte = static_cast<Common::Byte>(sum & 0xFF);
    return (sum & 0x100) != 0;
  }

  template<Index index>
  static Common::BusRequest zp1(Mos6502& cpu, Common::BusResponse response)
  {
    // Zero Page Write addressing mode
    // Fetch the low byte of the address
    cpu.m_targetLo = response.data;
    cpu.m_targetHi = c_ZeroPage;
    cpu.m_log.addByte(cpu.m_targetLo, 0);

    char buffer[] = "$XX  ";
    std::format_to(buffer + 1, "{:02X}{}", cpu.m_targetLo,
        index == Index::None ? "  " :
        index == Index::X    ? ",X" :
                               ",Y");
    cpu.m_log.setOperand(buffer);

    if constexpr (index != Index::None)
    {
      // Add the index register to the low byte, wrapping around within the zero page.
      bool carry = incrementByte(cpu.m_targetLo, getIndexValue<index>(cpu));
      static_cast<void>(carry);  // carry is ignored in zero page addressing
    }
    return Mos6502::nextOp(cpu, response);
  }

  template<Index index>
  static Common::BusRequest abs0(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    // Put the address of the lo byte on the BusRequest
    cpu.m_action = &AddressMode::abs1<index>;
    return Common::BusRequest::Read(cpu.regs.pc++);
  }

  template<Index index>
  static Common::BusRequest abs1(Mos6502& cpu, Common::BusResponse response)
  {
    // Read the lo byte from the BusRequest
    cpu.m_targetLo = response.data;
    cpu.m_log.addByte(response.data, 0);

    cpu.m_action = &AddressMode::abs2<index>;
    // Put the address of the hi byte on the bus.
    return Common::BusRequest::Read(cpu.regs.pc++);
  }
  template<Index index>
  static Common::BusRequest abs2(Mos6502& cpu, Common::BusResponse response)
  {
    // Read the hi byte from the response.
    cpu.m_targetHi = response.data;
    cpu.m_log.addByte(cpu.m_targetHi, 1);

    // Log the target address
    char buffer[] = "$XXXX";
    std::format_to(buffer + 1, "{:02X}{:02X}", cpu.m_targetHi, cpu.m_targetLo);
    cpu.m_log.setOperand(buffer);

    // If this is an indexed addressing mode, add the index register to the lo byte.
    // Note: This can cause the address to wrap around if it exceeds 0xFF which will
    // give us the wrong address. On the 6502, this is known as "page boundary crossing"
    // and it causes an extra read cycle. We emulate it here for accuracy.

    bool carry = false;
    if constexpr (index != Index::None)  // overflow occurred
    {
      carry = incrementByte(cpu.m_targetLo, getIndexValue<index>(cpu));
    }

    // If carry is set, then target is the wrong address, but we need to read from it to emulate the 6502's,
    // then we need to increment the hi byte to and read it on the next cycle. If no carry, then target is
    // correct and we're done.
    if (carry)
    {
      cpu.m_action = &AddressMode::abs3<index>;
      return Common::BusRequest::Read(cpu.getEffectiveAddress());
    }
    return Mos6502::nextOp(cpu, response);
  }

  template<Index index>
  static Common::BusRequest abs3(Mos6502& cpu, Common::BusResponse response)
  {
    // We read from the wrong address due to page boundary crossing.
    ++cpu.m_targetHi;  // increment hi byte to get the correct address
    return Mos6502::nextOp(cpu, response);
  }

  template<Index index>
    requires(index != Index::None)
  static Common::BusRequest indirect(Mos6502& cpu, Common::BusResponse response)
  {
    return Mos6502::nextOp(cpu, response);
  }

  static Common::BusRequest Fetch(Mos6502& cpu, Common::BusResponse response);

  static constexpr auto abs = &AddressMode::abs0<Index::None>;
  static constexpr auto absX = &AddressMode::abs0<Index::X>;
  static constexpr auto absY = &AddressMode::abs0<Index::Y>;

  static constexpr auto zp = &AddressMode::zp0<Index::None>;
  static constexpr auto zpX = &AddressMode::zp0<Index::X>;
  static constexpr auto zpY = &AddressMode::zp0<Index::Y>;

  template<Index index>
    requires(index != Index::None)
  static Common::Byte getIndexValue(const Mos6502& cpu) noexcept
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
