# Keep the compiler language mode independent of the target's libstdc++ age.
add_library(tracer_compat INTERFACE)
target_compile_features(tracer_compat INTERFACE cxx_std_20)
target_include_directories(tracer_compat INTERFACE ${PROJECT_SOURCE_DIR}/include)
if(TRACER_SDK_ROOT AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The SDK's prebuilt LLVM/glibc DSOs have no runtime search paths of their
  # own. Retain their split libc companions directly so this target's RUNPATH
  # selects matching SDK libraries instead of incompatible host stubs.
  target_link_libraries(tracer_compat INTERFACE
    "-Wl,--push-state,--no-as-needed" rt pthread dl "-Wl,--pop-state")
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <filesystem>
int main() { return std::filesystem::current_path().empty(); }"
  TRACER_FILESYSTEM_IN_STDCXX)
if(NOT TRACER_FILESYSTEM_IN_STDCXX)
  set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
  check_cxx_source_compiles("#include <filesystem>
int main() { return std::filesystem::current_path().empty(); }"
    TRACER_FILESYSTEM_IN_SEPARATE_ARCHIVE)
  unset(CMAKE_REQUIRED_LIBRARIES)
  if(NOT TRACER_FILESYSTEM_IN_SEPARATE_ARCHIVE)
    message(FATAL_ERROR "The selected C++ library has no usable filesystem implementation")
  endif()
  target_link_libraries(tracer_compat INTERFACE stdc++fs)
endif()
