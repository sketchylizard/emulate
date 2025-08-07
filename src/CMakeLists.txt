add_library(cpu6502 STATIC)

target_sources(cpu6502
  PRIVATE
  cpu/cpu.cpp
  PUBLIC
  cpu/ArithmeticLogicUnit.h
  cpu/cpu.h
  cpu/flags.h
  cpu/program_counter.h
  cpu/register.h
  util/bit_ops.h
  util/hex.h
  util/trace.h
)

target_include_directories(cpu6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
