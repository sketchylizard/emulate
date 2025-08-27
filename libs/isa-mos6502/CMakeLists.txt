
add_library(isa-mos6502 STATIC
  include/mos6502/mos6502.h
  src/mos6502.cpp
)

target_include_directories(isa-mos6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(isa-mos6502 PUBLIC core65xx)
