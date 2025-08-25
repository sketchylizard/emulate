//==============================================================================
// trace.hpp - 6502 CPU Tracing System
//==============================================================================

#pragma once
#include <cstdint>
#include <string_view>

#include "common/Memory.h"

#ifndef MOS6502_TRACE
#define MOS6502_TRACE 0
#endif

class Tracer
{
public:
  using Byte = Common::Byte;
  using Address = Common::Address;

  enum class OperandType
  {
    None,  // Implied addressing
    Immediate,  // #$nn
    ZeroPage,  // $nn
    ZeroPageX,  // $nn,X
    ZeroPageY,  // $nn,Y
    Absolute,  // $nnnn
    AbsoluteX,  // $nnnn,X
    AbsoluteY,  // $nnnn,Y
    Indirect,  // ($nnnn)
    IndirectX,  // ($nn,X)
    IndirectY,  // ($nn),Y
    Relative,  // Branch instructions
    Accumulator  // A
  };

  // Public interface - compiles to empty when MOS6502_TRACE=0
  void setEnabled(bool enable) noexcept
  {
#if MOS6502_TRACE
    runtime_enabled = enable;
#else
    (void) enable;  // Suppress unused parameter warning
#endif
  }

  bool isEnabled() const noexcept
  {
#if MOS6502_TRACE
    return runtime_enabled;
#else
    return false;
#endif
  }

  void addRegisters(Address pc, Byte a, Byte x, Byte y, Byte p, Byte sp) noexcept
  {
#if MOS6502_TRACE
    if (!runtime_enabled)
      return;
    addRegistersImpl(pc, a, x, y, p, sp);
#else
    (void) pc;
    (void) a;
    (void) x;
    (void) y;
    (void) p;
    (void) sp;
#endif
  }

  void addInstruction(Byte opcode, std::string_view mnemonic) noexcept
  {
#if MOS6502_TRACE
    if (!runtime_enabled)
      return;
    addInstructionImpl(opcode, mnemonic);
#else
    (void) opcode;
    (void) mnemonic;
#endif
  }

  void addAddressMode(OperandType operandType, Byte lo, Byte hi = 0) noexcept
  {
#if MOS6502_TRACE
    if (!runtime_enabled)
      return;
    addAddressModeImpl(operandType, lo, hi);
#else
    (void) operandType;
    (void) lo;
    (void) hi;
#endif
  }

  void flush() noexcept
  {
#if MOS6502_TRACE
    if (runtime_enabled)
      flushImpl();
#endif
  }

#if MOS6502_TRACE
private:
  struct TraceFrame
  {
    Address pc{};
    Byte a{}, x{}, y{}, p{}, sp{};
    Byte opcode{};
    std::string_view mnemonic{};
    OperandType operand_type = OperandType::None;
    Byte operand_lo{}, operand_hi{};
    bool has_registers = false;
    bool has_instruction = false;
    bool has_addressing = false;
  };

  TraceFrame current_frame{};
  bool runtime_enabled = true;

  // Implementation functions - only exist when tracing enabled
  void addRegistersImpl(Address pc, Byte a, Byte x, Byte y, Byte p, Byte sp) noexcept;
  void addInstructionImpl(Byte opcode, std::string_view mnemonic) noexcept;
  void addAddressModeImpl(OperandType operandType, Byte lo, Byte hi) noexcept;
  void flushImpl() noexcept;
  void outputTrace() noexcept;
  void formatOperand() noexcept;
#endif
};
