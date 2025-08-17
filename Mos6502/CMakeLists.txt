find_package(Catch2 REQUIRED)

add_library(Mos6502 STATIC
  include/Mos6502/Bus.h
  include/Mos6502/Log.h
  include/Mos6502/Memory.h
  include/Mos6502/Mos6502.h
  src/AddressMode.cpp
  src/AddressMode.h
  src/Bus.cpp
  src/Log.cpp
  src/Memory.cpp
  src/Mos6502.cpp
)

target_include_directories(Mos6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Mos6502Test
  test/AddressModeTest.cpp
  test/Mos6502Test.cpp
)

target_link_libraries(Mos6502Test PRIVATE Mos6502 Catch2::Catch2WithMain util)
target_include_directories(Mos6502Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
