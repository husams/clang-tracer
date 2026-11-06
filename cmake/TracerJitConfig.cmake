include_guard(GLOBAL)

set(TRACER_JIT_FLAGS "" CACHE STRING "Semicolon-separated default compiler flags for embedded callback compilation")
set(_tracer_jit_entries "")
set(_tracer_jit_count 0)
set(_tracer_runtime_jit_flags ${TRACER_JIT_FLAGS})
if(TRACER_SDK_ROOT)
  # Embedded Clang does not read the SDK driver's configuration files. Select
  # the native compiler's include profile explicitly, without host C++ fallback.
  list(APPEND _tracer_runtime_jit_flags -nostdinc++)
  foreach(_include IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    list(APPEND _tracer_runtime_jit_flags -isystem "${_include}")
  endforeach()
endif()
foreach(_flag IN LISTS _tracer_runtime_jit_flags)
  string(REPLACE "\\" "\\\\" _escaped "${_flag}")
  string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
  string(REPLACE "\n" "\\n" _escaped "${_escaped}")
  string(REPLACE "\r" "\\r" _escaped "${_escaped}")
  string(APPEND _tracer_jit_entries "  \"${_escaped}\",\n")
  math(EXPR _tracer_jit_count "${_tracer_jit_count} + 1")
endforeach()
configure_file("${PROJECT_SOURCE_DIR}/src/runtime_config.hpp.in"
  "${PROJECT_BINARY_DIR}/generated/tracer_runtime_config.hpp" @ONLY)
