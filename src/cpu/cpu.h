#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

#include "ArithmeticLogicUnit.h"
#include "program_counter.h"
#include "register.h"

class Cpu6502
{
public:
  using Byte = std::byte;

  // Register tags
  // clang-format off
  struct A_tag {};
  struct X_tag {};
  struct Y_tag {};
  struct StackPointer_tag {};
  // clang-format on

  // Define aliases for register types
  using ARegister = Register<A_tag>;
  using XRegister = Register<X_tag>;
  using YRegister = Register<Y_tag>;
  // Stack Pointer with default reset value per 6502 convention
  using StackPointer = Register<StackPointer_tag, std::byte{0xFD}>;

private:
  ProgramCounter m_pc;
  ARegister m_aRegister;
  XRegister m_xRegister;
  YRegister m_yRegister;
  StackPointer m_sp;
  ArithmeticFlags m_status;
  ArithmeticLogicUnit m_alu;

  // Memory is just a placeholder — inject later
  std::array<Byte, 65536> memory_{};

public:
  constexpr Cpu6502() = default;

  [[nodiscard]] constexpr std::byte a() const noexcept
  {
    return m_aRegister.read();
  }
  constexpr void set_a(std::byte v) noexcept
  {
    m_aRegister.write(v);
  }

  [[nodiscard]] constexpr std::byte x() const noexcept
  {
    return m_xRegister.read();
  }
  constexpr void set_x(std::byte v) noexcept
  {
    m_xRegister.write(v);
  }

  [[nodiscard]] constexpr std::byte y() const noexcept
  {
    return m_yRegister.read();
  }
  constexpr void set_y(std::byte v) noexcept
  {
    m_yRegister.write(v);
  }

  [[nodiscard]] constexpr std::byte sp() const noexcept
  {
    return m_sp.read();
  }
  constexpr void set_sp(std::byte v) noexcept
  {
    m_sp.write(v);
  }

  [[nodiscard]] constexpr uint16_t pc() const noexcept
  {
    return m_pc.read();
  }
  constexpr void set_pc(uint16_t addr) noexcept
  {
    m_pc.write(addr);
  }

  [[nodiscard]] constexpr ArithmeticFlags status() const noexcept
  {
    return m_status;
  }

  constexpr void set_status(ArithmeticFlags flags) noexcept
  {
    m_status = flags;
  }

  [[nodiscard]] constexpr Byte read_memory(uint16_t address) const noexcept
  {
    return memory_[address];
  }

  constexpr void write_memory(uint16_t address, Byte value) noexcept
  {
    memory_[address] = value;
  }

  constexpr void write_memory(uint16_t address, std::span<const std::byte> bytes) noexcept
  {
    assert(memory_.size() - bytes.size() > address);

    auto offset = memory_.begin() + address;
    std::ranges::copy(bytes, offset);
  }

  constexpr void step()
  {
    // Fetch
    Byte opcode = read_memory(m_pc.read());

    // Increment PC
    ++m_pc;

    // (Very simple execute example — no decode)
    if (opcode == Byte{0xEA})
    {  // NOP
       // Do nothing
    }
    else if (opcode == Byte{0x69})  // ADC #imm
    {
      Byte operand = read_memory(m_pc.read());
      ++m_pc;

      auto [result, flags] = m_alu.adc(m_aRegister.read(), operand, static_cast<bool>(m_status & CarryFlag));
      m_aRegister.write(result);
      m_status = flags;
    }
  }

  constexpr void reset(uint16_t reset_vector)
  {
    m_pc.write(reset_vector);
    m_aRegister = ARegister{};
    m_xRegister = XRegister{};
    m_yRegister = YRegister{};
    m_sp = StackPointer{};
  }
};
