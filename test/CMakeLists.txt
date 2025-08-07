find_package(Catch2 REQUIRED)

add_executable(unit_tests
  test_main.cpp
  TestArithmeticLogicUnit.cpp
  TestCpu.cpp
  test_program_counter.cpp
  test_register.cpp
)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain cpu6502)

include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
