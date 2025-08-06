add_library(cpu6502 STATIC)

target_sources(cpu6502
  PRIVATE
    cpu/cpu.cpp
  PUBLIC
    cpu/cpu.h
    cpu/alu.h
    cpu/register.h
    cpu/flags.h
    util/bit_ops.h
    util/trace.h
)

target_include_directories(cpu6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
