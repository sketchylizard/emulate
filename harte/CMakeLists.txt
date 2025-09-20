if(BUILD_TESTING)
  find_package(simdjson CONFIG REQUIRED)

  add_executable(harte
    single_step_tests.cpp
    test_reporter.cpp
    test_reporter.h
    test_run.cpp
    test_run.h
  )

  target_link_libraries(harte PRIVATE
    cpu6502
    common
    simdjson::simdjson
  )

  target_include_directories(harte PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  if(EMULATE_ENABLE_LOGGING)
    target_compile_definitions(harte PRIVATE EMULATE_ENABLE_LOGGING)
  endif()
endif()
