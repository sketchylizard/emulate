if(BUILD_TESTING)
  find_package(simdjson CONFIG REQUIRED)

  add_executable(harte
    single_step_tests.cpp
  )

  target_link_libraries(harte PRIVATE
    cpu6502
    common
    simdjson::simdjson
  )

  target_include_directories(harte PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
