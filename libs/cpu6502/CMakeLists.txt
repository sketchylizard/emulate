
add_library(cpu6502 STATIC
  include/cpu6502/address_mode.h
  include/cpu6502/cpu6502_types.h
  include/cpu6502/mos6502.h
  include/cpu6502/state.h
  src/address_mode.cpp
  src/cpu6502_types.cpp
  src/mos6502.cpp
  src/state.cpp
)

target_include_directories(cpu6502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cpu6502 PRIVATE common)

if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)

  add_executable(cpu6502Test
    tests/address_mode_test.cpp
    tests/helpers.cpp
    tests/helpers.h
  )

  target_link_libraries(cpu6502Test PRIVATE cpu6502 common Catch2::Catch2WithMain)
  target_include_directories(cpu6502Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
