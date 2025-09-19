if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)
  find_package(incbin REQUIRED)

  message(STATUS "Klaus6502 found: ${Klaus6502_ADDED}")
  message(STATUS "incbin found: ${incbin_FOUND} @ ${incbin_DIR}")

  add_executable(integrationTest
    Mos6502Test.cpp
    KlausFunctional.cpp
    KlausFunctional.h
    single_step_tests.cpp
  )

  target_link_libraries(integrationTest PRIVATE
    cpu6502
    common
    Catch2::Catch2WithMain
    incbin::incbin
  )

  target_include_directories(integrationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
