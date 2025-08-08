find_package(Catch2 REQUIRED)

add_library(Mos6502 STATIC
  src/Mos6502.cpp
  include/Mos6502/Mos6502.h
)

target_include_directories(Mos6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Mos6502Test
  test/Mos6502Test.cpp
)

target_link_libraries(Mos6502Test PRIVATE Mos6502 Catch2::Catch2WithMain util)