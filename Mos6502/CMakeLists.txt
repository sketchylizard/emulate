find_package(Catch2 REQUIRED)

add_library(Mos6502 STATIC
  include/Mos6502/Mos6502.h
  src/AddressMode.cpp
  src/AddressMode.h
  src/Mos6502.cpp
)

target_link_libraries(Mos6502 PUBLIC common)
target_include_directories(Mos6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Mos6502Test
  test/AddressModeTest.cpp
  test/Mos6502Test.cpp
)

target_link_libraries(Mos6502Test PRIVATE Mos6502 Catch2::Catch2WithMain util)
target_include_directories(Mos6502Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(Klaus6502_ADDED)
    message(STATUS "Downloaded 6502 functional tests: ${Klaus6502_SOURCE_DIR}")
    target_compile_definitions(Mos6502Test PRIVATE KLAUS6502_TESTS_DIR="${Klaus6502_SOURCE_DIR}")
endif()
