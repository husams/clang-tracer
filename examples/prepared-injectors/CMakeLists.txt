cmake_minimum_required(VERSION 3.20)
project(prepared_injectors_example LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  message(FATAL_ERROR "Run this target on Linux x86_64")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PREPARED_DEMO_BIAS "7" CACHE STRING "Change to produce a different linked image")
if(NOT PREPARED_DEMO_BIAS MATCHES "^[0-9]+$")
  message(FATAL_ERROR "PREPARED_DEMO_BIAS must be a nonnegative integer")
endif()

function(prepared_demo_target target calculator bias)
  add_executable(${target} target.cpp "${calculator}")
  set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(${target} PRIVATE PREPARED_DEMO_BIAS=${bias})
  target_compile_options(${target} PRIVATE
    -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
endfunction()

prepared_demo_target(prepared-injectors-target calculator.cpp "${PREPARED_DEMO_BIAS}")
if(BUILD_TESTING AND TRACER_BUILD_REPL)
  # Give the ABI-compatible rebuild its own compile command. The original
  # calculator.cpp stays available as an unambiguous explicit flags donor.
  configure_file(calculator.cpp calculator-changed.cpp COPYONLY)
  prepared_demo_target(prepared-injectors-changed-target
    "${CMAKE_CURRENT_BINARY_DIR}/calculator-changed.cpp" 8)
endif()
