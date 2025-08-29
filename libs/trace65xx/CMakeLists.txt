find_package(Catch2 REQUIRED)

add_library(trace65xx STATIC
  include/trace65xx/trace65xx.h
  src/trace65xx.cpp
)

target_include_directories(trace65xx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(trace65xx PUBLIC core_cpu)
