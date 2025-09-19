#pragma once

#include "common/microcode_pump.h"

namespace Common
{

//! By inheriting from this template, you define the types needed to enable your CPU definition to
//! work with MicrocodePump. You must define all of your microcode functions should take a State
//! object as a reference and a Bus object by value as parameters. The State object represents the
//! CPU state, and the Bus object represents the bus interface to read and write memory and I/O. The
//! microcode can return a Response object that contains a Microcode function to inject into the
//! stream, or nullptr if none.

template<typename AddressType, typename DataType, typename StateType>
struct ProcessorDefinition
{
  using Address = AddressType;
  using Data = DataType;
  using State = StateType;

  //! Exception thrown when the CPU encounters a trap condition, such as a self-jump
  //! or self branch.
  class TrapException : public std::exception
  {
  public:
    explicit TrapException(Address trapAddress)
      : m_address(trapAddress)
    {
    }

    Address address() const noexcept
    {
      return m_address;
    }

    const char* what() const noexcept override
    {
      return "CPU trap detected";
    }

  private:
    Address m_address;
  };

  //! Trap exception handler. If set, this function will be called when a trap is triggered.
  //! If not set, a TrapException will be thrown.
  using TrapHandler = void (*)(Address pc);

  // Default trap handler that throws
  static void defaultTrapHandler(Address trapAddress)
  {
    throw TrapException(trapAddress);
  }

  // Static function pointer with default value
  static inline TrapHandler trapHandler = defaultTrapHandler;

  // Convenience function for microcode to call
  static void trap(Address address)
  {
    trapHandler(address);
  }

  // Function to change the trap handler (for testing)
  static void setTrapHandler(TrapHandler handler)
  {
    trapHandler = handler ? handler : defaultTrapHandler;
  }

  // You must define two static methods to pump microcodes into the MicrocodePump:
  // 1. fetchNextOpcode(State& cpu) - returns a BusRequest that fetches the next. It will
  //    pass the current State so it can use the program counter to determine the address.
  // 2. decode(uint8_t opcode) - takes the fetched opcode and returns a pair of pointers to the
  //    beginning and end of the microcode sequence for that opcode

  // Generic bus interface - most CPUs have address and data concepts
  class BusInterface
  {
  public:
    virtual ~BusInterface() = default;
    virtual DataType read(Address addr) = 0;
    virtual void write(Address addr, DataType data) = 0;
  };

  // Generic bus wrapper with single-use enforcement
  struct BusToken
  {
    BusInterface* m_impl = nullptr;

    DataType read(Address addr) noexcept
    {
      assert(m_impl != nullptr);
      auto* impl = m_impl;

      // Consume the token to prevent re-use
      m_impl = nullptr;
      return impl->read(addr);
    }

    void write(Address addr, DataType data) noexcept
    {
      assert(m_impl != nullptr);
      auto* impl = m_impl;
      // Consume the token to prevent re-use
      m_impl = nullptr;
      impl->write(addr, data);
    }
  };

  // Microcode function type Each microcode function takes a State reference and a Bus object by
  // value, and returns a Response object that can contain an injected microcode function or be
  // empty. The Bus Token object is a lightweight wrapper around the actual bus interface that enforces
  // single-use semantics for each microcode function. The State object represents the CPU state and
  // can be modified by the microcode function.o
  struct Response;

  using Microcode = Response (*)(State&, BusToken bus);

  struct Response
  {
    // A pointer to a microcode function to inject into the stream, or nullptr if none.
    Microcode injection = nullptr;
  };
};

}  // namespace Common
