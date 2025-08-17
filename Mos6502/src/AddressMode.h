#pragma once

#include "Mos6502/Bus.h"
#include "Mos6502/Mos6502.h"

enum class Index
{
  None,
  X,
  Y
};

inline constexpr Byte c_ZeroPage{0x00};
inline constexpr Byte c_StackPage{0x01};

struct AddressMode
{
  static Bus accumulator(Mos6502& cpu, Bus bus, Byte step);
  static Bus implied(Mos6502& cpu, Bus bus, Byte step);
  static Bus immediate(Mos6502& cpu, Bus bus, Byte step);
  static Bus relative(Mos6502& cpu, Bus bus, Byte step);

  template<Index index>
  static Bus zero_page(Mos6502& cpu, Bus bus, Byte step);

  template<Index index>
  static Bus absoluteRead(Mos6502& cpu, Bus bus, Byte step);

  template<Index index>
  static Bus absoluteWrite(Mos6502& cpu, Bus bus, Byte step);

  template<Index index>
    requires(index != Index::None)
  static Bus indirect(Mos6502& cpu, Bus bus, Byte step);
};

// Template definitions must remain in the header:
template<Index index>
Bus AddressMode::zero_page(Mos6502& cpu, Bus bus, Byte step)
{
  if (step == 0)
  {
    return Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    Byte loByte = bus.data;
    cpu.m_log.addByte(loByte, 0);

    char buffer[] = "$XX  ";
    std::format_to(buffer + 1, "{:02X}{}", loByte, index == Index::None ? "  " : index == Index::X ? ",X" : ",Y");
    cpu.m_log.setOperand(buffer);

    if constexpr (index == Index::X)
    {
      loByte += cpu.m_x;
    }
    else if constexpr (index == Index::Y)
    {
      loByte += cpu.m_y;
    }

    return Bus::Read(MakeAddress(loByte, c_ZeroPage));
  }

  cpu.m_operand = bus.data;
  return cpu.StartOperation(bus);
}

template<Index index>
Bus AddressMode::absoluteRead(Mos6502& cpu, Bus bus, Byte step)
{
  if (step == 0)
  {
    // Put the address of the lo byte on the bus
    return Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    // Read the lo byte from the bus
    cpu.m_target = MakeAddress(bus.data, c_ZeroPage);
    cpu.m_log.addByte(bus.data, 0);

    // Put the address of the hi byte on the bus.
    return Bus::Read(cpu.m_pc++);
  }
  if (step == 2)
  {
    // Read the hi byte from the bus.
    Byte hiByte = bus.data;
    cpu.m_log.addByte(hiByte, 1);

    // Combine loByte and hiByte to form the target address
    Byte loByte = LoByte(cpu.m_target);
    cpu.m_target = MakeAddress(loByte, hiByte);

    // Log the target address
    char buffer[] = "$XXXX";
    std::format_to(buffer + 2, "{:02X}", cpu.m_target);
    cpu.m_log.setOperand(buffer);

    Byte originalLoByte = loByte;

    // If this is an indexed addressing mode, add the index register to the lo byte.
    // Note: This can cause the address to wrap around if it exceeds 0xFF which will
    // give us the wrong address. On the 6502, this is known as "page boundary crossing"
    // and it causes an extra read cycle. We emulate it here for accuracy.
    if constexpr (index == Index::X)
    {
      // Increment loByte to calculate the wrong address.
      loByte += cpu.m_x;
      // Calculate the correct address.
      cpu.m_target += cpu.m_x;
    }
    else if constexpr (index == Index::Y)
    {
      // Increment loByte to calculate the wrong address.
      loByte += cpu.m_y;
      // Calculate the correct address.
      cpu.m_target += cpu.m_y;
    }

    // The behavior described above only occurs with index absolute mode.

    if constexpr (index != Index::None)
    {
      if (originalLoByte > loByte)
      {
        Address wrongAddr = MakeAddress(loByte, hiByte);
        return Bus::Read(wrongAddr);
      }
    }

    // We either are not in indexed mode, or we did not cross a page boundary.
    // Perform the normal read.
    return Bus::Read(cpu.m_target);
  }
  if (step == 3)
  {
    assert(index != Index::None);

    // If we get here we were in index mode and we crossed a page boundary.
    // Perform the correct read now.
    return Bus::Read(cpu.m_target);
  }

  cpu.m_operand = bus.data;
  return cpu.StartOperation(bus);
}

template<Index index>
Bus AddressMode::absoluteWrite(Mos6502& cpu, Bus bus, Byte step)
{
  if (step == 0)
  {
    // Put the address of the lo byte on the bus
    return Bus::Read(cpu.m_pc++);
  }
  if (step == 1)
  {
    // Read the lo byte from the bus
    cpu.m_target = MakeAddress(bus.data, c_ZeroPage);
    cpu.m_log.addByte(bus.data, 0);

    // Put the address of the hi byte on the bus.
    return Bus::Read(cpu.m_pc++);
  }
  if (step == 2)
  {
    // Read the hi byte from the bus.
    Byte hiByte = bus.data;
    cpu.m_log.addByte(hiByte, 1);

    // Combine loByte and hiByte to form the target address
    Byte loByte = LoByte(cpu.m_target);
    cpu.m_target = MakeAddress(loByte, hiByte);

    // Log the target address
    char buffer[] = "$XXXX";
    std::format_to(buffer + 2, "{:02X}", cpu.m_target);
    cpu.m_log.setOperand(buffer);

    if constexpr (index == Index::X)
    {
      cpu.m_target += cpu.m_x;
    }
    else if constexpr (index == Index::Y)
    {
      cpu.m_target += cpu.m_y;
    }

    Byte originalLoByte = loByte;

    // If this is an indexed addressing mode, add the index register to the lo byte.
    // Note: This can cause the address to wrap around if it exceeds 0xFF which will
    // give us the wrong address. On the 6502, this is known as "page boundary crossing"
    // and it causes an extra read cycle. We emulate it here for accuracy.
    if constexpr (index == Index::X)
    {
      // Increment loByte to calculate the wrong address.
      loByte += cpu.m_x;
      // Calculate the correct address.
      cpu.m_target += cpu.m_x;
    }
    else if constexpr (index == Index::Y)
    {
      // Increment loByte to calculate the wrong address.
      loByte += cpu.m_y;
      // Calculate the correct address.
      cpu.m_target += cpu.m_y;
    }

    // The behavior described above only occurs with index absolute mode.

    if constexpr (index != Index::None)
    {
      if (originalLoByte > loByte)
      {
        Address wrongAddr = MakeAddress(loByte, hiByte);
        return Bus::Read(wrongAddr);
      }
    }

    // This is taken if this is not an indexed addressing mode or if we didn't cross a page boundary.
    return cpu.StartOperation(bus);
  }

  assert(step == 3);
  // This is taken if we crossed a page boundary in an indexed mode.
  return cpu.StartOperation(bus);
}

template<Index index>
  requires(index != Index::None)
Bus AddressMode::indirect(Mos6502& cpu, Bus bus, Byte step)
{
  static_cast<void>(step);
  return cpu.StartOperation(bus);
}
