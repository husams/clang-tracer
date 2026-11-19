# Native x86-64 compatibility profile; compile with SDK Clang, retain GCC 8's
# standard-library ABI and the SDK's glibc. Supply SDK-built dependencies through
# CMAKE_PREFIX_PATH. Never substitute the machine's modern shared dependencies.
set(TRACER_SDK_ROOT "" CACHE PATH "Native GCC 8.5 runtime SDK (required)")
if(NOT IS_ABSOLUTE "${TRACER_SDK_ROOT}" OR
   NOT EXISTS "${TRACER_SDK_ROOT}/bin/clang++")
  message(FATAL_ERROR "Pass -DTRACER_SDK_ROOT=/absolute/path/to/sdk containing Clang 21 and the GCC 8.5 SDK")
endif()
set(CMAKE_SYSROOT "${TRACER_SDK_ROOT}/x86_64-1a-linux-gnu/sysroot"
  CACHE PATH "SDK compilation sysroot")
set(TRACER_DYNAMIC_LOADER "${CMAKE_SYSROOT}/lib64/ld-linux-x86-64.so.2"
  CACHE FILEPATH "Target executable dynamic loader")
if(NOT IS_ABSOLUTE "${CMAKE_SYSROOT}" OR NOT IS_DIRECTORY "${CMAKE_SYSROOT}")
  message(FATAL_ERROR "CMAKE_SYSROOT must name an existing absolute directory")
endif()
if(NOT IS_ABSOLUTE "${TRACER_DYNAMIC_LOADER}" OR
   NOT EXISTS "${TRACER_DYNAMIC_LOADER}" OR IS_DIRECTORY "${TRACER_DYNAMIC_LOADER}")
  message(FATAL_ERROR "TRACER_DYNAMIC_LOADER must name an existing absolute loader file")
endif()
# Preserve the profile in compiler checks and reject stale cached linker flags.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
  TRACER_SDK_ROOT CMAKE_SYSROOT TRACER_DYNAMIC_LOADER)
foreach(_profile_setting TRACER_SDK_ROOT CMAKE_SYSROOT TRACER_DYNAMIC_LOADER)
  if(DEFINED _TRACER_CONFIGURED_${_profile_setting} AND
     NOT "${_TRACER_CONFIGURED_${_profile_setting}}" STREQUAL "${${_profile_setting}}")
    message(FATAL_ERROR "Changing ${_profile_setting} requires a fresh build directory")
  endif()
  set(_TRACER_CONFIGURED_${_profile_setting} "${${_profile_setting}}"
    CACHE INTERNAL "Configured SDK profile value")
endforeach()
set(CMAKE_C_COMPILER "${TRACER_SDK_ROOT}/bin/clang" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${TRACER_SDK_ROOT}/bin/clang++" CACHE FILEPATH "")
set(LLVM_DIR "${TRACER_SDK_ROOT}/lib.llvm/cmake/llvm" CACHE PATH "")
set(Clang_DIR "${TRACER_SDK_ROOT}/lib.llvm/cmake/clang" CACHE PATH "")
set(TRACER_CLANG_DRIVER "${TRACER_SDK_ROOT}/bin/clang" CACHE FILEPATH "")
set(_sdk_runtime "${TRACER_SDK_ROOT}/x86_64-1a-linux-gnu/lib64;${CMAKE_SYSROOT}/lib64;${TRACER_SDK_ROOT}/lib.llvm")
set(CMAKE_BUILD_RPATH "${_sdk_runtime}" CACHE STRING "SDK build runtime")
set(CMAKE_INSTALL_RPATH "${_sdk_runtime}" CACHE STRING "SDK installed runtime")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--enable-new-dtags,--dynamic-linker=${TRACER_DYNAMIC_LOADER}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--enable-new-dtags")
set(TRACER_JIT_FLAGS "--sysroot=${CMAKE_SYSROOT};--gcc-toolchain=${TRACER_SDK_ROOT}" CACHE STRING "Matching SDK callback headers and ABI")
