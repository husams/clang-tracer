# Native x86-64 compatibility profile; compile with SDK Clang, retain GCC 8's
# standard-library ABI and the SDK's glibc. Supply SDK-built dependencies through
# CMAKE_PREFIX_PATH. Never substitute the machine's modern shared dependencies.
set(TRACER_SDK_ROOT "" CACHE PATH "Native GCC 8.5 runtime SDK (required)")
if(NOT IS_ABSOLUTE "${TRACER_SDK_ROOT}" OR
   NOT EXISTS "${TRACER_SDK_ROOT}/bin/clang++")
  message(FATAL_ERROR "Pass -DTRACER_SDK_ROOT=/absolute/path/to/sdk containing Clang 21 and the GCC 8.5 SDK")
endif()
# Preserve the explicit SDK selection in CMake's compiler-check subprojects.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TRACER_SDK_ROOT)
set(CMAKE_C_COMPILER "${TRACER_SDK_ROOT}/bin/clang" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${TRACER_SDK_ROOT}/bin/clang++" CACHE FILEPATH "")
set(CMAKE_SYSROOT "${TRACER_SDK_ROOT}/x86_64-1a-linux-gnu/sysroot")
set(LLVM_DIR "${TRACER_SDK_ROOT}/lib.llvm/cmake/llvm" CACHE PATH "")
set(Clang_DIR "${TRACER_SDK_ROOT}/lib.llvm/cmake/clang" CACHE PATH "")
set(TRACER_CLANG_DRIVER "${TRACER_SDK_ROOT}/bin/clang" CACHE FILEPATH "")
set(_sdk_runtime "${TRACER_SDK_ROOT}/x86_64-1a-linux-gnu/lib64;${CMAKE_SYSROOT}/lib64;${TRACER_SDK_ROOT}/lib.llvm")
set(CMAKE_BUILD_RPATH "${_sdk_runtime}" CACHE STRING "SDK build runtime")
set(CMAKE_INSTALL_RPATH "${_sdk_runtime}" CACHE STRING "SDK installed runtime")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--enable-new-dtags,--dynamic-linker=${CMAKE_SYSROOT}/lib64/ld-linux-x86-64.so.2")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--enable-new-dtags")
set(TRACER_JIT_FLAGS "--sysroot=${CMAKE_SYSROOT};--gcc-toolchain=${TRACER_SDK_ROOT}" CACHE STRING "Matching SDK callback headers and ABI")
