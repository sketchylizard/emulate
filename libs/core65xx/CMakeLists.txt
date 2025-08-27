add_library(core65xx STATIC
  include/core65xx/address_mode.h
  include/core65xx/core65xx.h
  src/address_mode.cpp
  src/core65xx.cpp
)

target_link_libraries(core65xx PUBLIC common)
target_include_directories(core65xx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(core65xx PUBLIC MOS6502_TRACE=1)

if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)

  add_executable(core65xxTest
    tests/address_mode_test.cpp
  )

  target_link_libraries(core65xxTest PRIVATE core65xx Catch2::Catch2WithMain)
  target_include_directories(core65xxTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
