# Standard CMake compiler/toolchain-file selection is intentionally preserved.
set(TRACER_LLVM_ROOT "" CACHE PATH "LLVM/Clang installation prefix")
if(DEFINED _TRACER_CONFIGURED_LLVM_ROOT AND
   NOT "${_TRACER_CONFIGURED_LLVM_ROOT}" STREQUAL "${TRACER_LLVM_ROOT}")
  message(FATAL_ERROR "Changing TRACER_LLVM_ROOT requires a fresh build directory")
endif()
set(_TRACER_CONFIGURED_LLVM_ROOT "${TRACER_LLVM_ROOT}" CACHE INTERNAL "Selected LLVM root")
if(TRACER_LLVM_ROOT)
  find_package(LLVM REQUIRED CONFIG PATHS
    "${TRACER_LLVM_ROOT}/lib/cmake/llvm" "${TRACER_LLVM_ROOT}/lib64/cmake/llvm"
    NO_DEFAULT_PATH)
  find_package(Clang REQUIRED CONFIG PATHS
    "${TRACER_LLVM_ROOT}/lib/cmake/clang" "${TRACER_LLVM_ROOT}/lib64/cmake/clang"
    NO_DEFAULT_PATH)
else()
  find_package(LLVM REQUIRED CONFIG)
  find_package(Clang REQUIRED CONFIG)
endif()
if(LLVM_VERSION_MAJOR LESS 21 OR LLVM_VERSION_MAJOR GREATER 22)
  message(FATAL_ERROR "Use LLVM/Clang 21 or 22")
endif()

set(TRACER_CLANG_RESOURCE_DIR "" CACHE PATH "Clang resource directory; empty: discover from selected LLVM")
set(TRACER_CLANG_DRIVER "" CACHE FILEPATH "Matching Clang driver used to discover builtin headers")
if(TRACER_CLANG_RESOURCE_DIR)
  set(_tracer_resource_dir "${TRACER_CLANG_RESOURCE_DIR}")
else()
  if(NOT TRACER_CLANG_DRIVER)
    unset(TRACER_CLANG_DRIVER CACHE)
    unset(TRACER_CLANG_DRIVER)
    find_program(TRACER_CLANG_DRIVER NAMES clang-${LLVM_VERSION_MAJOR} clang
      HINTS "${LLVM_TOOLS_BINARY_DIR}" NO_DEFAULT_PATH REQUIRED)
  endif()
  execute_process(COMMAND "${TRACER_CLANG_DRIVER}" --version
    RESULT_VARIABLE _driver_status OUTPUT_VARIABLE _driver_version)
  if(NOT _driver_status EQUAL 0 OR NOT _driver_version MATCHES "clang version ${LLVM_VERSION_MAJOR}\\.")
    message(FATAL_ERROR "TRACER_CLANG_DRIVER must match LLVM ${LLVM_VERSION_MAJOR}")
  endif()
  execute_process(COMMAND "${TRACER_CLANG_DRIVER}" --print-resource-dir
    RESULT_VARIABLE _resource_status OUTPUT_VARIABLE _tracer_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _resource_status EQUAL 0)
    message(FATAL_ERROR "Cannot locate builtin headers; set TRACER_CLANG_RESOURCE_DIR")
  endif()
endif()
get_filename_component(_tracer_resource_dir "${_tracer_resource_dir}" ABSOLUTE)
if(NOT EXISTS "${_tracer_resource_dir}/include/stddef.h")
  message(FATAL_ERROR "Clang resource headers are missing from ${_tracer_resource_dir}")
endif()
message(STATUS "Tracing LLVM: ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
message(STATUS "Tracing Clang resources: ${_tracer_resource_dir}")
