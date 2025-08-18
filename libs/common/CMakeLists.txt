find_package(Catch2 REQUIRED)

add_library(common STATIC
  include/common/Bus.h
  include/common/Memory.h
  src/Bus.cpp
  src/Memory.cpp
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(commonTest
  test/MemoryTest.cpp
)

target_link_libraries(commonTest PRIVATE common Catch2::Catch2WithMain util)
target_include_directories(commonTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
