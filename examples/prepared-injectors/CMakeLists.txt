cmake_minimum_required(VERSION 3.20)
project(prepared_injectors_example LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_definitions(${target} PRIVATE PREPARED_DEMO_BIAS=${bias})
  target_compile_options(${target} PRIVATE
    -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
endfunction()

prepared_demo_target(prepared-injectors-target calculator.cpp "${PREPARED_DEMO_BIAS}")
if(BUILD_TESTING AND TRACER_BUILD_REPL)
  add_executable(prepared-concrete-target concrete/target.cpp concrete/virtual.cpp)
  target_compile_options(prepared-concrete-target PRIVATE
    -g0 -O0 -fno-omit-frame-pointer -fcf-protection=none)
  add_executable(prepared-history-target concrete/history.cpp)
  target_compile_options(prepared-history-target PRIVATE
    -g0 -O0 -fno-omit-frame-pointer -fcf-protection=none)
  # Exercise prepared binding both with and without a loaded ELF build ID.
  target_link_options(prepared-injectors-target PRIVATE -Wl,--build-id=none)
  # Give the ABI-compatible rebuild its own compile command. The original
  # calculator.cpp stays available as an unambiguous explicit flags donor.
  configure_file(calculator.cpp calculator-changed.cpp COPYONLY)
  prepared_demo_target(prepared-injectors-changed-target
    "${CMAKE_CURRENT_BINARY_DIR}/calculator-changed.cpp" 8)
  target_link_options(prepared-injectors-changed-target PRIVATE -Wl,--build-id)
endif()

add_executable(prepared-methods-target methods/target.cpp methods/account.cpp)
# The runnable demo has no debug information. Its package is bound by ELF
# linkage and its typed metadata comes from account.hpp.
target_compile_options(prepared-methods-target PRIVATE
  -g0 -O0 -fno-omit-frame-pointer -fcf-protection=none)

add_executable(prepared-pointee-target pointee/target.cpp pointee/item.cpp)
target_compile_options(prepared-pointee-target PRIVATE
  -g -O0 -fno-omit-frame-pointer -fcf-protection=none)

add_executable(prepared-polymorphic-pointee-target
  polymorphic-pointee/target.cpp polymorphic-pointee/item.cpp)
set_target_properties(prepared-polymorphic-pointee-target PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(prepared-polymorphic-pointee-target PRIVATE
  -g -O0 -fno-omit-frame-pointer -fcf-protection=none -fvisibility=hidden)

add_executable(prepared-pimpl-target pimpl/target.cpp pimpl/item.cpp)
set_target_properties(prepared-pimpl-target PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(prepared-pimpl-target PRIVATE
  -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
