
add_library(apple2 STATIC
  include/apple2/apple2system.h
  src/apple2system.cpp
)

target_include_directories(apple2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(apple2 PRIVATE common cpu6502)

if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)

  add_executable(apple2Test
    tests/apple2test.cpp
    )

  target_link_libraries(
    apple2Test PRIVATE
    apple2
    common
    cpu6502
  )
  
endif()
