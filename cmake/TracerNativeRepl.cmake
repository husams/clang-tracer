include(FetchContent)

function(tracer_add_native_repl)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  # No linkage to tracer_compat/context targets: they propagate C++20. This
  # portable client and its private generated RPC bindings must remain C++17.
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(BUILD_SHARED_LIBS OFF)
  # ANTLR otherwise FORCE-writes a default build type into the parent cache.
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
  endif()
  set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
  set(ANTLR_BUILD_CPP_TESTS OFF CACHE BOOL "" FORCE)
  set(ANTLR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ANTLR_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ANTLR4_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(tracer_replxx
    URL https://codeload.github.com/AmokHuginnsson/replxx/tar.gz/refs/tags/release-0.0.4
    URL_HASH SHA256=a22988b2184e1d256e2d111b5749e16ffb1accbf757c7b248226d73c426844c4)
  FetchContent_Declare(tracer_antlr
    URL https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip
    URL_HASH SHA256=0ed13668906e86dbc0dcddf30fdee68c10203dea4e83852b4edb810821bee3c4)
  FetchContent_MakeAvailable(tracer_replxx tracer_antlr)
  set(_repl "${CMAKE_CURRENT_SOURCE_DIR}/src/repl")
  add_library(tracer_repl_commands STATIC
    "${_repl}/commands.cpp"
    "${_repl}/generated/TracerCommandsLexer.cpp"
    "${_repl}/generated/TracerCommandsParser.cpp"
    "${_repl}/generated/TracerCommandsVisitor.cpp"
    "${_repl}/generated/TracerCommandsBaseVisitor.cpp")
  target_include_directories(tracer_repl_commands PUBLIC "${_repl}")
  target_include_directories(tracer_repl_commands SYSTEM PRIVATE "${_repl}/generated")
  target_link_libraries(tracer_repl_commands PRIVATE antlr4_static)
  find_package(Threads REQUIRED)
  find_package(Protobuf QUIET CONFIG)
  if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
  endif()
  find_package(gRPC REQUIRED CONFIG)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(_generated "${CMAKE_CURRENT_BINARY_DIR}/repl-generated")
  file(MAKE_DIRECTORY "${_generated}")
  add_custom_command(
    OUTPUT "${_generated}/tracer.pb.cc" "${_generated}/tracer.pb.h"
           "${_generated}/tracer.grpc.pb.cc" "${_generated}/tracer.grpc.pb.h"
           "${_generated}/context_transport.pb.cc" "${_generated}/context_transport.pb.h"
    COMMAND protobuf::protoc
    ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
         --cpp_out=${_generated} --grpc_out=${_generated}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         "${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto"
    COMMAND protobuf::protoc
    ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${_generated}
         "${CMAKE_CURRENT_SOURCE_DIR}/proto/context_transport.proto"
    DEPENDS proto/tracer.proto proto/context_transport.proto protobuf::protoc gRPC::grpc_cpp_plugin
    VERBATIM)
  add_library(tracer_repl_rpc STATIC "${_generated}/tracer.pb.cc"
    "${_generated}/tracer.grpc.pb.cc" "${_generated}/context_transport.pb.cc")
  target_include_directories(tracer_repl_rpc PUBLIC "${_generated}")
  target_link_libraries(tracer_repl_rpc PUBLIC gRPC::grpc++ protobuf::libprotobuf)
  add_library(tracer_repl_client STATIC "${_repl}/controller.cpp" "${_repl}/server_process.cpp" "${_repl}/events.cpp" "${_repl}/background_events.cpp")
  target_include_directories(tracer_repl_client PUBLIC "${_repl}")
  target_link_libraries(tracer_repl_client PUBLIC tracer_repl_rpc tracer_repl_commands Threads::Threads)
  target_compile_options(tracer_repl_client PRIVATE -Wall -Wextra -Wpedantic)
  include(CheckCXXSourceCompiles)
  set(_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
  set(_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++17")
  check_cxx_source_compiles("#include <filesystem>\nint main(){return std::filesystem::current_path().empty();}" TRACER_REPL_FILESYSTEM_LINKS)
  if(NOT TRACER_REPL_FILESYSTEM_LINKS)
    set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
    check_cxx_source_compiles("#include <filesystem>\nint main(){return std::filesystem::current_path().empty();}" TRACER_REPL_FILESYSTEM_ARCHIVE_LINKS)
    if(NOT TRACER_REPL_FILESYSTEM_ARCHIVE_LINKS)
      message(FATAL_ERROR "The native C++17 REPL requires a usable filesystem library")
    endif()
    target_link_libraries(tracer_repl_client PUBLIC stdc++fs)
  endif()
  set(CMAKE_REQUIRED_FLAGS "${_saved_required_flags}")
  set(CMAKE_REQUIRED_LIBRARIES "${_saved_required_libraries}")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TracerToolchain.cmake")
  add_executable(clang-tracer-prepare-injectors-native
    "${CMAKE_CURRENT_SOURCE_DIR}/src/prepare_injectors_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_resolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/header_context.cpp")
  target_include_directories(clang-tracer-prepare-injectors-native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_include_directories(clang-tracer-prepare-injectors-native SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_compile_definitions(clang-tracer-prepare-injectors-native PRIVATE
    TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
  target_link_libraries(clang-tracer-prepare-injectors-native PRIVATE
    tracer_repl_rpc clang-cpp LLVM)
  if(NOT TRACER_REPL_FILESYSTEM_LINKS)
    target_link_libraries(clang-tracer-prepare-injectors-native PRIVATE stdc++fs)
  endif()
  if(TRACER_SDK_ROOT AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(clang-tracer-prepare-injectors-native PRIVATE
      "-Wl,--push-state,--no-as-needed" rt pthread dl "-Wl,--pop-state")
  endif()
  set_target_properties(clang-tracer-prepare-injectors-native PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(clang-tracer-prepare-injectors-native PRIVATE -fno-rtti)
  endif()
  install(TARGETS clang-tracer-prepare-injectors-native RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
  set(_repl_api_destination "${CMAKE_INSTALL_FULL_DATADIR}/clang-tracer/repl/api.hpp")
  file(RELATIVE_PATH _repl_api_relative "${CMAKE_INSTALL_FULL_BINDIR}" "${_repl_api_destination}")
  file(RELATIVE_PATH _repl_resource_relative "${CMAKE_INSTALL_FULL_BINDIR}"
    "${CMAKE_INSTALL_FULL_DATADIR}/clang-tracer/clang-resource")
  set(_repl_compiler_arguments "  \"-nostdinc++\",\n")
  foreach(_directory IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    string(REPLACE "\\" "\\\\" _directory "${_directory}")
    string(REPLACE "\"" "\\\"" _directory "${_directory}")
    string(APPEND _repl_compiler_arguments "  \"-isystem\", \"${_directory}\",\n")
  endforeach()
  if(APPLE)
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE _sdk OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _sdk_status)
    if(_sdk_status EQUAL 0)
      string(APPEND _repl_compiler_arguments "  \"-isysroot\", \"${_sdk}\",\n")
    endif()
  endif()
  configure_file("${_repl}/repl_config.hpp.in" "${_generated}/repl_config.hpp" @ONLY)
  add_executable(clang-tracer-repl-check "${_repl}/check.cpp" "${_repl}/completion.cpp" "${_repl}/record_mappings.cpp" "${_repl}/mapping_cache.cpp")
  target_include_directories(clang-tracer-repl-check SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_include_directories(clang-tracer-repl-check PRIVATE "${_repl}")
  target_link_libraries(clang-tracer-repl-check PRIVATE clang-cpp LLVM)
  if(NOT TRACER_REPL_FILESYSTEM_LINKS)
    target_link_libraries(clang-tracer-repl-check PRIVATE stdc++fs)
  endif()
  if(TRACER_SDK_ROOT AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # LLVM's SDK DSOs have no RUNPATH; retain matching split libc companions.
    target_link_libraries(clang-tracer-repl-check PRIVATE
      "-Wl,--push-state,--no-as-needed" rt pthread dl "-Wl,--pop-state")
  endif()
  set_target_properties(clang-tracer-repl-check PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
  add_executable(clang-tracer-repl "${_repl}/main.cpp" "${_repl}/interpreter.cpp" "${_repl}/completion.cpp" "${_repl}/record_mappings.cpp" "${_repl}/check_process.cpp" "${_repl}/check_session.cpp" "${_repl}/shell.cpp" "${_repl}/compiler_profile.cpp" "${_repl}/mapping_cache.cpp")
  add_dependencies(clang-tracer-repl clang-tracer-repl-check clang-tracer-prepare-injectors-native)
  target_include_directories(clang-tracer-repl SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_include_directories(clang-tracer-repl PRIVATE "${_generated}")
  target_link_libraries(clang-tracer-repl PRIVATE tracer_repl_client replxx::replxx clang-cpp LLVM)
  target_compile_options(clang-tracer-repl PRIVATE -Wall -Wextra -Wpedantic)
  set_target_properties(clang-tracer-repl PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF ENABLE_EXPORTS ON INSTALL_RPATH_USE_LINK_PATH TRUE)
  if(NOT LLVM_ENABLE_RTTI)
    set_source_files_properties("${_repl}/interpreter.cpp" "${_repl}/completion.cpp" PROPERTIES COMPILE_OPTIONS "-fno-rtti")
  endif()
  install(TARGETS clang-tracer-repl clang-tracer-repl-check RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
  install(FILES "${_repl}/api.hpp" "${_repl}/event_value.hpp" "${_repl}/event_ranges.hpp" DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/repl" COMPONENT Runtime)
  option(TRACER_BUNDLE_CLANG_HEADERS "Install selected Clang builtin headers with the runtime" ON)
  if(TRACER_REPL_ONLY AND TRACER_BUNDLE_CLANG_HEADERS)
    install(DIRECTORY "${_tracer_resource_dir}/include"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/clang-resource" COMPONENT Runtime)
  endif()
  install(FILES "${tracer_replxx_SOURCE_DIR}/LICENSE.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses" RENAME replxx.txt COMPONENT Runtime)
  install(FILES "${tracer_antlr_SOURCE_DIR}/LICENSE.txt" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses" RENAME antlr4.txt COMPONENT Runtime)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/LLVM.txt" DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses" COMPONENT Runtime)
  if(BUILD_TESTING)
    add_executable(repl-range-tests tests/repl_ranges.cpp)
    target_include_directories(repl-range-tests PRIVATE "${_repl}")
    add_test(NAME native_repl_ranges COMMAND repl-range-tests)
    add_executable(repl-command-tests tests/repl_commands.cpp)
    target_link_libraries(repl-command-tests PRIVATE tracer_repl_commands)
    add_test(NAME native_repl_commands COMMAND repl-command-tests)
    add_executable(repl-profile-tests tests/repl_profile.cpp "${_repl}/shell.cpp")
    target_link_libraries(repl-profile-tests PRIVATE tracer_repl_client)
    add_test(NAME native_repl_profile COMMAND repl-profile-tests)
    set_tests_properties(native_repl_profile PROPERTIES TIMEOUT 20)
    add_executable(repl-client-tests tests/repl_client.cpp)
    target_link_libraries(repl-client-tests PRIVATE tracer_repl_client)
    add_test(NAME native_repl_client COMMAND repl-client-tests)
    set_tests_properties(native_repl_client PROPERTIES TIMEOUT 30)
    add_executable(repl-value-tests tests/repl_values.cpp)
    target_link_libraries(repl-value-tests PRIVATE tracer_repl_client)
    add_test(NAME native_repl_values COMMAND repl-value-tests)
    add_executable(repl-snapshot-storage-tests tests/value_snapshot_storage.cpp "${CMAKE_CURRENT_SOURCE_DIR}/src/value_snapshot_storage.cpp")
    target_include_directories(repl-snapshot-storage-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(repl-snapshot-storage-tests PRIVATE Threads::Threads)
    add_test(NAME native_repl_snapshot_storage COMMAND repl-snapshot-storage-tests)
    set_tests_properties(native_repl_snapshot_storage PROPERTIES TIMEOUT 30)
    add_executable(repl-cpp-tests tests/repl_cpp.cpp "${_repl}/interpreter.cpp" "${_repl}/completion.cpp" "${_repl}/record_mappings.cpp" "${_repl}/check_process.cpp" "${_repl}/check_session.cpp" "${_repl}/shell.cpp" "${_repl}/compiler_profile.cpp" "${_repl}/mapping_cache.cpp")
    add_dependencies(repl-cpp-tests clang-tracer-repl-check)
    target_include_directories(repl-cpp-tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
    target_include_directories(repl-cpp-tests PRIVATE "${_generated}")
    target_link_libraries(repl-cpp-tests PRIVATE tracer_repl_client clang-cpp LLVM)
    set_target_properties(repl-cpp-tests PROPERTIES ENABLE_EXPORTS ON)
    add_test(NAME native_repl_cpp COMMAND repl-cpp-tests)
    set_tests_properties(native_repl_cpp PROPERTIES TIMEOUT 60)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME native_repl_checker_session
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_checker_session.py"
        $<TARGET_FILE:clang-tracer-repl-check>)
    set_tests_properties(native_repl_checker_session PROPERTIES TIMEOUT 90)
    add_test(NAME native_repl_console
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_console.py"
        $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:repl-client-tests>)
    set_tests_properties(native_repl_console PROPERTIES TIMEOUT 90)
    add_test(NAME native_repl_terminal
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_terminal.py"
        $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:repl-client-tests>)
    set_tests_properties(native_repl_terminal PROPERTIES TIMEOUT 120)
    add_test(NAME native_repl_auto_records
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_auto_records.py"
        $<TARGET_FILE:clang-tracer-repl>)
    set_tests_properties(native_repl_auto_records PROPERTIES TIMEOUT 240)
    add_test(NAME native_repl_header_reuse
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_header_reuse.py"
        $<TARGET_FILE:clang-tracer-repl>)
    set_tests_properties(native_repl_header_reuse PROPERTIES TIMEOUT 240)
    if(NOT TRACER_REPL_ONLY)
      add_test(NAME native_repl_prepared_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_prepared_integration.py"
          --repl $<TARGET_FILE:clang-tracer-repl> --server $<TARGET_FILE:clang-tracer-server>
          --agent $<TARGET_FILE:clang_tracer_agent>
          --target $<TARGET_FILE:prepared-injectors-target>
          --changed-target $<TARGET_FILE:prepared-injectors-changed-target>
          --example "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors"
          --compdb "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json"
          --compiler "${CMAKE_CXX_COMPILER}")
      set_tests_properties(native_repl_prepared_integration PROPERTIES TIMEOUT 240)
      add_test(NAME native_repl_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_native_integration.py"
          --repl $<TARGET_FILE:clang-tracer-repl> --server $<TARGET_FILE:clang-tracer-server>
          --target $<TARGET_FILE:cpp-interceptors-target>
          --bool-target $<TARGET_FILE:signature-interceptor-fixture>
          --callback "${CMAKE_CURRENT_SOURCE_DIR}/examples/cpp-interceptors/method.cpp")
      set_tests_properties(native_repl_integration PROPERTIES TIMEOUT 240)
      add_test(NAME native_repl_complex_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_complex_integration.py"
          --repl $<TARGET_FILE:clang-tracer-repl> --server $<TARGET_FILE:clang-tracer-server>
          --target $<TARGET_FILE:repl-complex-fixture>
          --header "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repl_complex_types.hpp")
      set_tests_properties(native_repl_complex_integration PROPERTIES TIMEOUT 600)
      add_test(NAME native_repl_value_examples
        COMMAND "${CMAKE_COMMAND}" -E env
          "CLANG_TRACER_REPL=$<TARGET_FILE:clang-tracer-repl>"
          "CLANG_TRACER_SERVER=$<TARGET_FILE:clang-tracer-server>"
          "CLANG_TRACER_VALUES_TARGET=$<TARGET_FILE:repl-values-example-target>"
          bash "${CMAKE_CURRENT_SOURCE_DIR}/examples/native-repl-values/run.sh" all)
      set_tests_properties(native_repl_value_examples PROPERTIES TIMEOUT 600)
    endif()
  endif()
endfunction()
