# Compiler-specific warning flags and settings

set(CMAKE_COMPILE_WARNING_AS_ERROR ${CMAKE_COMPILE_WARNING_AS_ERROR})

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wold-style-cast -Wcast-align -Wundef -Wdouble-promotion -Werror

    -Wnon-virtual-dtor -Woverloaded-virtual -Winconsistent-missing-override
    -Wswitch-enum -Wcovered-switch-default
    -Wrange-loop-analysis -Wnull-dereference
    -Wshadow-field -Wshadow-uncaptured-local
    -Wextra-semi -Wcomma -Wreserved-id-macro -Wmismatched-tags
    -Wmissing-variable-declarations
    -Wimplicit-int-float-conversion -Wshorten-64-to-32 -Wvla -Wunreachable-code
    -Wformat-security
  )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(STATUS "Using GCC warning flags")
  add_compile_options(
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wold-style-cast -Wcast-align=strict -Wundef -Wdouble-promotion
    -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
    -Wswitch-enum -Wrange-loop-construct -Wnull-dereference
    -Wuseless-cast -Wduplicated-cond -Wduplicated-branches -Wlogical-op
    -Wrestrict -Wformat=2 -Wformat-security -Wextra-semi
    -Wvla -Wplacement-new=2 -Wredundant-decls -Wmissing-declarations
    -Wmissing-declarations -Wctor-dtor-privacy
    -Wdeprecated-enum-float-conversion -Wunreachable-code
  )

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(STATUS "Using MSVC warning flags")
  add_compile_options(/W4 /WX)
endif()
