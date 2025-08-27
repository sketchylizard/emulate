add_library(Core65xx STATIC
  include/Core65xx/Core65xx.h
  include/Core65xx/Tracer.h
  src/AddressMode.cpp
  src/AddressMode.h
  src/Core65xx.cpp
  src/Tracer.cpp
)

target_link_libraries(Core65xx PUBLIC common)
target_include_directories(Core65xx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(Core65xx PUBLIC MOS6502_TRACE=1)

find_package(Catch2 REQUIRED)
find_package(incbin REQUIRED)

message(STATUS "Klaus6502 found: ${Klaus6502_ADDED}")
message(STATUS "incbin found: ${incbin_FOUND} @ ${incbin_DIR}")

add_executable(Mos6502Test
  test/AddressModeTest.cpp
  test/Mos6502Test.cpp
  test/Mos6502Test.cpp
  test/KlausFunctional.cpp
  test/KlausFunctional.h
)

target_link_libraries(Mos6502Test PRIVATE Core65xx Catch2::Catch2WithMain util incbin::incbin)
target_include_directories(Mos6502Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
