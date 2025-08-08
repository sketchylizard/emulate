add_library(util STATIC
  include/util/hex.h
  src/hex.cpp
)

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
