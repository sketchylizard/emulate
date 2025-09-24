find_package(simdjson CONFIG REQUIRED)

add_library(cpu6502 STATIC
  include/cpu6502/address_mode.h
  include/cpu6502/cpu6502_types.h
  include/cpu6502/mos6502.h
  include/cpu6502/registers.h
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
    tests/single_step_tests.cpp
    tests/test_reporter.h
    tests/test_run.h
    tests/test_reporter.cpp
    tests/test_run.cpp
  )

  target_link_libraries(
    cpu6502Test PRIVATE
    cpu6502
    common
    simdjson::simdjson)

  target_include_directories(cpu6502Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
