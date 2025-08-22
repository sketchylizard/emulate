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
    return Common::BusRequest::Read(cpu.m_pc++);
  }

  template<Index index>
  static Common::BusRequest zp1(Mos6502& cpu, Common::BusResponse response)
  {
    // Zero Page Write addressing mode
    // Fetch the low byte of the address
    Common::Byte loByte = response.data;
    cpu.m_log.addByte(loByte, 0);

    char buffer[] = "$XX  ";
    std::format_to(buffer + 1, "{:02X}{}", loByte, index == Index::None ? "  " : index == Index::X ? ",X" : ",Y");
    cpu.m_log.setOperand(buffer);

    auto [address1, address2] = AddressMode::indexed<index>(cpu, loByte, c_ZeroPage);
    // In the case of zero page addressing, we wrap around to the start of the page,
    // so the second value returned is the correct value.
    cpu.m_target = address2;
    return cpu.StartOperation(response);
  }

  template<Index index>
  static Common::BusRequest abs0(Mos6502& cpu, Common::BusResponse /*response*/)
  {
    // Put the address of the lo byte on the BusRequest
    cpu.m_action = &AddressMode::abs1<index>;
    return Common::BusRequest::Read(cpu.m_pc++);
  }
  template<Index index>
  static Common::BusRequest abs1(Mos6502& cpu, Common::BusResponse response)
  {
    // Read the lo byte from the BusRequest
    cpu.m_target = Common::MakeAddress(response.data, c_ZeroPage);
    cpu.m_log.addByte(response.data, 0);

    cpu.m_action = &AddressMode::abs2<index>;
    // Put the address of the hi byte on the bus.
    return Common::BusRequest::Read(cpu.m_pc++);
  }
  template<Index index>
  static Common::BusRequest abs2(Mos6502& cpu, Common::BusResponse response)
  {
    // Read the hi byte from the response.
    Common::Byte hiByte = response.data;
    cpu.m_log.addByte(hiByte, 1);

    // Combine loByte and hiByte to form the target address
    Common::Byte loByte = Common::LoByte(cpu.m_target);

    // Log the target address
    char buffer[] = "$XXXX";
    std::format_to(buffer + 1, "{:02X}{:02X}", hiByte, loByte);
    cpu.m_log.setOperand(buffer);

    // If this is an indexed addressing mode, add the index register to the lo byte.
    // Note: This can cause the address to wrap around if it exceeds 0xFF which will
    // give us the wrong address. On the 6502, this is known as "page boundary crossing"
    // and it causes an extra read cycle. We emulate it here for accuracy.
    Common::Address wrongAddress;
    std::tie(cpu.m_target, wrongAddress) = indexed<index>(cpu, loByte, hiByte);

    if constexpr (index != Index::None)  // overflow occurred
    {
      if (cpu.m_target != wrongAddress)
      {
        cpu.m_action = &AddressMode::abs3<index>;
        return Common::BusRequest::Read(wrongAddress);
      }
    }
    return cpu.StartOperation(response);
  }
  template<Index index>
  static Common::BusRequest abs3(Mos6502& cpu, Common::BusResponse response)
  {
    // If we get here we were in index mode and we crossed a page boundary.
    return cpu.StartOperation(response);
  }

  template<Index index>
    requires(index != Index::None)
  static Common::BusRequest indirect(Mos6502& cpu, Common::BusResponse response)
  {
    return cpu.StartOperation(response);
  }

  static Common::BusRequest Fetch(Mos6502& cpu, Common::BusResponse response);

  static constexpr auto abs = &AddressMode::abs0<Index::None>;
  static constexpr auto absX = &AddressMode::abs0<Index::X>;
  static constexpr auto absY = &AddressMode::abs0<Index::Y>;

  static constexpr auto zp = &AddressMode::zp0<Index::None>;
  static constexpr auto zpX = &AddressMode::zp0<Index::X>;
  static constexpr auto zpY = &AddressMode::zp0<Index::Y>;

  // Add the given index to the address and returns a pair of addresses,
  // first = correct address, second = address with just the low byte incremented.
  // In the case of a page boundary crossing, the second address will be incorrect,
  // but because of the way the 6502 handles memory, it will read from the wrong address
  // first. In the case of zero page indexing, the second value is the correct value
  // because it wraps around and continues to index the zero page.
  template<Index index>
  static std::pair<Common::Address, Common::Address> indexed(const Mos6502& cpu, Common::Byte loByte, Common::Byte hiByte);
};

// Template definitions must remain in the header:

template<Index index>
std::pair<Common::Address, Common::Address> AddressMode::indexed(const Mos6502& cpu, Common::Byte loByte, Common::Byte hiByte)
{
  if constexpr (index != Index::None)
  {
    // Cast it to a 16-bit value so we can observe any overflow
    auto low = Common::MakeAddress(loByte, c_ZeroPage);

    if constexpr (index == Index::X)
    {
      low += cpu.m_x;
    }
    else
    {
      assert((index == Index::Y));
      low += cpu.m_y;
    }
    if (low > Common::Address{0x00FF})  // overflowed
    {
      loByte = Common::LoByte(low);
      auto wrong = Common::MakeAddress(loByte, hiByte);
      auto correct = Common::MakeAddress(loByte, hiByte + 1);
      return {correct, wrong};
    }
    else
    {
      loByte = Common::LoByte(low);
    }
  }
  auto address = Common::MakeAddress(loByte, hiByte);
  return {address, address};
}
