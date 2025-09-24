find_package(Catch2 REQUIRED)

add_library(common STATIC
  include/common/address.h
  include/common/bank_switcher.h
  include/common/bus.h
  include/common/fixed_formatter.h
  include/common/hex.h
  include/common/logger.h
  include/common/memory.h
  include/common/microcode_pump.h
  src/address.cpp
  src/bank_switcher.cpp
  src/bus.cpp
  src/fixed_formatter.cpp
  src/hex.cpp
  src/logger.cpp
  src/memory.cpp
  src/microcode_pump.cpp
)

if (EMULATE_ENABLE_LOGGING)
  target_compile_definitions(common PUBLIC EMULATE_ENABLE_LOGGING=1)
endif()

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)
  add_executable(commonTest
    test/address_test.cpp
    test/bank_switcher_test.cpp
    test/memory_test.cpp
  )

  target_link_libraries(commonTest PRIVATE common Catch2::Catch2WithMain util)
  target_include_directories(commonTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
