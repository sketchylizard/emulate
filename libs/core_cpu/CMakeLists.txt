add_library(core_cpu STATIC
  include/core_cpu/core_cpu.h
  src/core_cpu.cpp
)

target_link_libraries(core_cpu PRIVATE common)
target_include_directories(core_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(core_cpu PUBLIC MOS6502_TRACE=1)

if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)

  add_executable(core_cpu_test
    tests/core_cpu_test.cpp
  )

  target_link_libraries(core_cpu_test PRIVATE core_cpu cpu6502 Catch2::Catch2WithMain common)
  target_include_directories(core_cpu_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
