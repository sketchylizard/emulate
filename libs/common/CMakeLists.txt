find_package(Catch2 REQUIRED)

add_library(common STATIC
  include/common/address_string_maker.h
  include/common/address.h
  include/common/Bus.h
  include/common/hex.h
  include/common/memory.h
  src/address.cpp
  src/Bus.cpp
  src/hex.cpp
  src/memory.cpp
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)
  add_executable(commonTest
    test/address_test.cpp
  )

  target_link_libraries(commonTest PRIVATE common Catch2::Catch2WithMain util)
  target_include_directories(commonTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
