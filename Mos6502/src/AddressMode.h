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
  static Common::Bus accumulator(Mos6502& cpu, Common::Bus bus, Common::Byte step);
  static Common::Bus implied(Mos6502& cpu, Common::Bus bus, Common::Byte step);
  static Common::Bus immediate(Mos6502& cpu, Common::Bus bus, Common::Byte step);
  static Common::Bus relative(Mos6502& cpu, Common::Bus bus, Common::Byte step);

  template<Index index>
  static Common::Bus zeroPageRead(Mos6502& cpu, Common::Bus bus, Common::Byte step);

  template<Index index>
  static Common::Bus zeroPageWrite(Mos6502& cpu, Common::Bus bus, Common::Byte step);

  template<Index index>
  static Common::Bus absoluteRead(Mos6502& cpu, Common::Bus bus, Common::Byte step);

  template<Index index>
  static Common::Bus absoluteWrite(Mos6502& cpu, Common::Bus bus, Common::Byte step);

  template<Index index>
    requires(index != Index::None)
  static Common::Bus indirect(Mos6502& cpu, Common::Bus bus, Common::Byte step);

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
Common::Bus AddressMode::zeroPageRead(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    return Common::Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    Common::Byte loByte = bus.data;
    cpu.m_log.addByte(loByte, 0);

    char buffer[] = "$XX  ";
    std::format_to(buffer + 1, "{:02X}{}", loByte, index == Index::None ? "  " : index == Index::X ? ",X" : ",Y");
    cpu.m_log.setOperand(buffer);

    auto [address1, address2] = AddressMode::indexed<index>(cpu, loByte, c_ZeroPage);
    // In the case of zero page addressing, we wrap around to the start of the page,
    // so the second value returned is the correct value.
    return Common::Bus::Read(address2);
  }

  cpu.m_operand = bus.data;
  return cpu.StartOperation(bus);
}

template<Index index>
Common::Bus AddressMode::zeroPageWrite(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    return Common::Bus::Read(cpu.m_pc++);
  }

  Common::Byte loByte = bus.data;
  cpu.m_log.addByte(loByte, 0);

  char buffer[] = "$XX  ";
  std::format_to(buffer + 1, "{:02X}{}", loByte, index == Index::None ? "  " : index == Index::X ? ",X" : ",Y");
  cpu.m_log.setOperand(buffer);

  auto [address1, address2] = AddressMode::indexed<index>(cpu, loByte, c_ZeroPage);
  // In the case of zero page addressing, we wrap around to the start of the page,
  // so the second value returned is the correct value.
  cpu.m_target = address2;
  return cpu.StartOperation(bus);
}

template<Index index>
Common::Bus AddressMode::absoluteRead(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    // Put the address of the lo byte on the bus
    return Common::Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    // Read the lo byte from the bus
    cpu.m_target = Common::MakeAddress(bus.data, c_ZeroPage);
    cpu.m_log.addByte(bus.data, 0);

    // Put the address of the hi byte on the bus.
    return Common::Bus::Read(cpu.m_pc++);
  }
  if (step == 2)
  {
    // Read the hi byte from the bus.
    Common::Byte hiByte = bus.data;
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
        cpu.SetFlag(Mos6502::ExtraStepRequired, true);
        return Common::Bus::Read(wrongAddress);
      }
    }

    // We either are not in indexed mode, or we did not cross a page boundary.
    // Perform the normal read.
    return Common::Bus::Read(cpu.m_target);
  }
  if (step == 3 && index != Index::None && cpu.HasFlag(Mos6502::ExtraStepRequired))
  {
    // If we get here we were in index mode and we crossed a page boundary.
    // Perform the correct read now.
    return Common::Bus::Read(cpu.m_target);
  }

  cpu.m_operand = bus.data;
  return cpu.StartOperation(bus);
}

template<Index index>
Common::Bus AddressMode::absoluteWrite(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  if (step == 0)
  {
    // Put the address of the lo byte on the bus
    return Common::Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    // Read the lo byte from the bus
    cpu.m_target = Common::MakeAddress(bus.data, c_ZeroPage);
    cpu.m_log.addByte(bus.data, 0);

    // Put the address of the hi byte on the bus.
    return Common::Bus::Read(cpu.m_pc++);
  }
  if (step == 2)
  {
    // Read the hi byte from the bus.
    Common::Byte hiByte = bus.data;
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
        cpu.SetFlag(Mos6502::ExtraStepRequired, true);
        return Common::Bus::Read(wrongAddress);
      }
    }

    // We either are not in indexed mode, or we did not cross a page boundary.
    return cpu.StartOperation(bus);
  }

  assert(step == 3);
  assert(cpu.HasFlag(Mos6502::ExtraStepRequired) == true);

  return cpu.StartOperation(bus);
}

template<Index index>
  requires(index != Index::None)
Common::Bus AddressMode::indirect(Mos6502& cpu, Common::Bus bus, Common::Byte step)
{
  static_cast<void>(step);
  return cpu.StartOperation(bus);
}

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
