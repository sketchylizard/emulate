find_package(Catch2 REQUIRED)

add_library(common STATIC
  include/common/address_string_maker.h
  include/common/address.h
  include/common/bus.h
  include/common/hex.h
  include/common/memory.h
  include/common/microcode_pump.h
  src/microcode_pump.cpp
  src/address.cpp
  src/bus.cpp
  src/hex.cpp
  src/memory.cpp
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)
  add_executable(commonTest
    test/address_test.cpp
    test/memory_test.cpp
    test/microcode_pump_test.cpp
  )

  target_link_libraries(commonTest PRIVATE common Catch2::Catch2WithMain util)
  target_include_directories(commonTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
