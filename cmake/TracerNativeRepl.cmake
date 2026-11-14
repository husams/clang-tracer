include(FetchContent)

function(tracer_add_native_repl)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
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
  find_package(SQLite3 REQUIRED)
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
  add_library(tracer_repl_client STATIC "${_repl}/controller.cpp" "${_repl}/server_process.cpp"
    "${_repl}/events.cpp" "${_repl}/../object_snapshot.cpp" "${_repl}/background_events.cpp")
  target_include_directories(tracer_repl_client PUBLIC "${_repl}"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(tracer_repl_client PUBLIC tracer_repl_rpc tracer_repl_commands Threads::Threads
    PRIVATE SQLite::SQLite3)
  target_compile_options(tracer_repl_client PRIVATE -Wall -Wextra -Wpedantic)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TracerToolchain.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TracerJitConfig.cmake")
  set(_src "${CMAKE_CURRENT_SOURCE_DIR}/src")
  add_library(tracer_module_index SHARED
    "${_src}/module_index.cpp" "${_src}/module_index_dwarf.cpp" "${_src}/module_index_headers.cpp")
  target_include_directories(tracer_module_index PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
  target_include_directories(tracer_module_index SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  target_link_libraries(tracer_module_index PRIVATE LLVM SQLite::SQLite3)
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(tracer_module_index PRIVATE -fno-rtti)
  endif()
  target_compile_options(tracer_module_index PRIVATE -Wall -Wextra -Wpedantic)
  add_executable(clang-tracer-module "${_src}/module_index_cli.cpp")
  target_include_directories(clang-tracer-module SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
  target_link_libraries(clang-tracer-module PRIVATE tracer_module_index LLVM)
  target_link_libraries(tracer_repl_client PUBLIC tracer_module_index)
  install(TARGETS tracer_module_index clang-tracer-module
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Runtime)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/module_index.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tracer" COMPONENT Development)
  add_executable(clang-tracer-prepare-injectors-native
    "${CMAKE_CURRENT_SOURCE_DIR}/src/prepare_injectors_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/prepare_elf_signature.cpp"
    "${_src}/prepare_package.cpp" "${_src}/module_index_prepare.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_resolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/header_context.cpp")
  target_include_directories(clang-tracer-prepare-injectors-native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_BINARY_DIR}/generated")
  target_include_directories(clang-tracer-prepare-injectors-native SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_compile_definitions(clang-tracer-prepare-injectors-native PRIVATE
    TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}"
    TRACER_PREPARE_BUILD_CXX="${CMAKE_CXX_COMPILER}")
  target_link_libraries(clang-tracer-prepare-injectors-native PRIVATE
    tracer_repl_rpc tracer_module_index clang-cpp LLVM)
  file(RELATIVE_PATH _module_lib_relative "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
  if(APPLE)
    set(_module_origin "@loader_path")
  else()
    set(_module_origin "$ORIGIN")
  endif()
  foreach(_target clang-tracer-prepare-injectors-native clang-tracer-module)
    set_target_properties(${_target} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE
      INSTALL_RPATH "${_module_origin}/${_module_lib_relative}")
  endforeach()
  set_target_properties(tracer_module_index PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(clang-tracer-prepare-injectors-native PRIVATE -fno-rtti)
  endif()
  install(TARGETS clang-tracer-prepare-injectors-native RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
  # Each executable uses the facade it was built against, even across upgrades.
  set(_repl_facade_headers "${_repl}/api.hpp" "${_repl}/event_value.hpp"
    "${_repl}/event_ranges.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/module_index.hpp")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_repl_facade_headers}")
  set(_repl_facade_digests "")
  foreach(_header IN LISTS _repl_facade_headers)
    file(SHA256 "${_header}" _digest)
    string(APPEND _repl_facade_digests "${_digest}")
  endforeach()
  string(SHA256 _repl_facade_id "${_repl_facade_digests}")
  set(_repl_api_directory "${CMAKE_INSTALL_DATADIR}/clang-tracer/repl")
  set(_repl_facade_directory "${_repl_api_directory}/facade-${_repl_facade_id}")
  file(RELATIVE_PATH _repl_api_relative "${CMAKE_INSTALL_FULL_BINDIR}"
    "${CMAKE_INSTALL_FULL_DATADIR}/clang-tracer/repl/facade-${_repl_facade_id}/api.hpp")
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
  set_target_properties(clang-tracer-repl-check PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
  add_executable(clang-tracer-repl "${_repl}/main.cpp" "${_repl}/interpreter.cpp" "${_repl}/completion.cpp" "${_repl}/record_mappings.cpp" "${_repl}/check_process.cpp" "${_repl}/check_session.cpp" "${_repl}/shell.cpp" "${_repl}/compiler_profile.cpp" "${_repl}/mapping_cache.cpp")
  target_sources(clang-tracer-repl PRIVATE "${_repl}/help.cpp" "${_repl}/syntax.cpp"
    "${_repl}/editor.cpp" "${_repl}/editor_completion.cpp")
  add_dependencies(clang-tracer-repl clang-tracer-repl-check clang-tracer-prepare-injectors-native)
  target_include_directories(clang-tracer-repl SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_include_directories(clang-tracer-repl PRIVATE "${_generated}")
  target_link_libraries(clang-tracer-repl PRIVATE tracer_repl_client replxx::replxx clang-cpp LLVM)
  target_compile_options(clang-tracer-repl PRIVATE -Wall -Wextra -Wpedantic)
  set_target_properties(clang-tracer-repl PROPERTIES ENABLE_EXPORTS ON INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "${_module_origin}/${_module_lib_relative}")
  if(NOT LLVM_ENABLE_RTTI)
    set_source_files_properties("${_repl}/interpreter.cpp" "${_repl}/completion.cpp" PROPERTIES COMPILE_OPTIONS "-fno-rtti")
  endif()
  install(FILES ${_repl_facade_headers} DESTINATION "${_repl_facade_directory}" COMPONENT Runtime)
  install(TARGETS clang-tracer-repl clang-tracer-repl-check RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)
  install(FILES ${_repl_facade_headers} DESTINATION "${_repl_api_directory}" COMPONENT Runtime)
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
    target_link_libraries(repl-client-tests PRIVATE tracer_repl_client SQLite::SQLite3)
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
    if(NOT TRACER_REPL_ONLY AND (TARGET clang_tracer_agent OR TARGET clang_tracer_object_cache_arm))
      if(TARGET clang_tracer_object_cache_arm)
        set(_object_cache_agent clang_tracer_object_cache_arm)
        set(_object_test_rendezvous --rendezvous arm_rendezvous)
        set(_object_test_arch_flags)
      else()
        set(_object_cache_agent clang_tracer_agent)
        set(_object_test_rendezvous)
        set(_object_test_arch_flags -fcf-protection=none)
      endif()
      foreach(_snapshot_case object_snapshots object_snapshot_unwind)
        set(_snapshot_target repl-${_snapshot_case}-target)
        set(_snapshot_source "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${_snapshot_case}")
        set(_snapshot_dir "${CMAKE_CURRENT_BINARY_DIR}/${_snapshot_case}/$<CONFIG>")
        set(_snapshot_options)
        add_executable(${_snapshot_target} "${_snapshot_source}/state.cpp")
        set_target_properties(${_snapshot_target} PROPERTIES CXX_STANDARD 17 OUTPUT_NAME fixture
          RUNTIME_OUTPUT_DIRECTORY "${_snapshot_dir}" INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_snapshot_target} PRIVATE -g -O0 -pthread -fno-omit-frame-pointer
          ${_object_test_arch_flags})
        target_link_libraries(${_snapshot_target} PRIVATE Threads::Threads)
        foreach(_snapshot_file state.cpp state.hpp)
          file(GENERATE OUTPUT "${_snapshot_dir}/${_snapshot_file}"
            INPUT "${_snapshot_source}/${_snapshot_file}")
        endforeach()
        if(_snapshot_case STREQUAL "object_snapshots")
          file(GENERATE OUTPUT "${_snapshot_dir}/policy.hpp" INPUT "${_snapshot_source}/policy.hpp")
          set(_snapshot_options --registered-any)
        elseif(TARGET clang_tracer_object_cache_arm)
          target_link_options(${_snapshot_target} PRIVATE "LINKER:--build-id=none")
          set(_snapshot_options --reexec --check-failed-start)
        else()
          set(_snapshot_options --check-failed-start)
        endif()
        file(GENERATE OUTPUT "${_snapshot_dir}/compile_commands.json" CONTENT
          "[{\"directory\":\"${_snapshot_dir}\",\"file\":\"state.cpp\",\"arguments\":[\"${CMAKE_CXX_COMPILER}\",\"-std=c++17\",\"-pthread\",\"-g\",\"-O0\",\"-c\",\"state.cpp\"]}]\n")
        add_test(NAME native_repl_${_snapshot_case}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_${_snapshot_case}.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> "${_snapshot_dir}" ${_snapshot_options})
        set_tests_properties(native_repl_${_snapshot_case} PROPERTIES TIMEOUT 150)
      endforeach()
      if(TARGET clang_tracer_object_cache_arm)
        set(_runtime_call_arch_flags -mbranch-protection=standard)
      else()
        set(_runtime_call_arch_flags -fcf-protection=none)
      endif()
      add_executable(repl-object-writer-entry-target
        tests/fixtures/object_writer_entry/writer.cpp
        tests/fixtures/object_writer_entry/dispatch.cpp
        tests/fixtures/object_writer_entry/main.cpp)
      set_target_properties(repl-object-writer-entry-target PROPERTIES
        CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-writer-entry-target PRIVATE
        -g -O2 -falign-functions=16 -fPIE ${_runtime_call_arch_flags})
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(repl-object-writer-entry-target PRIVATE -fstandalone-debug)
      endif()
      target_link_options(repl-object-writer-entry-target PRIVATE -pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_object_writer_entry
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_writer_entry.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-writer-entry-target>)
      set_tests_properties(native_repl_object_writer_entry PROPERTIES TIMEOUT 120)
      foreach(_plugin_value 19 23)
        set(_runtime_plugin repl-object-runtime-plugin-${_plugin_value})
        add_library(${_runtime_plugin} SHARED tests/fixtures/object_plan/runtime_plugin.cpp)
        target_compile_options(${_runtime_plugin} PRIVATE -g -O2 ${_runtime_call_arch_flags})
        target_compile_definitions(${_runtime_plugin} PRIVATE TRACE_TEST_PLUGIN_VALUE=${_plugin_value})
      endforeach()
      foreach(_dispatch direct indirect shared tail nested namespace selected_dso discovered_nonleaf discovered_dso discovered_namespace)
        set(_runtime_fixture repl-object-runtime-${_dispatch}-target)
        add_executable(${_runtime_fixture}
          tests/fixtures/object_plan/record.cpp tests/fixtures/object_plan/indirect.cpp)
        set_target_properties(${_runtime_fixture} PROPERTIES
          CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_runtime_fixture} PRIVATE -g -O2 -fPIE -fomit-frame-pointer
          -fno-optimize-sibling-calls ${_runtime_call_arch_flags})
        target_link_options(${_runtime_fixture} PRIVATE -pie "LINKER:--build-id=sha1")
        set(_runtime_plugin_args)
        set(_runtime_mode ${_dispatch})
        if(_dispatch STREQUAL "direct")
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_DIRECT)
        elseif(_dispatch STREQUAL "indirect")
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_MULTI_INDIRECT)
        elseif(_dispatch STREQUAL "tail")
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_TAIL)
        elseif(_dispatch STREQUAL "nested")
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_NESTED)
        elseif(_dispatch STREQUAL "discovered_nonleaf")
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_DISCOVERED_NONLEAF)
        else()
          target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_DSO)
          if(_dispatch STREQUAL "namespace")
            target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_NAMESPACE)
            set(_runtime_mode shared)
          endif()
          target_link_libraries(${_runtime_fixture} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
          set(_runtime_plugin_args --plugin $<TARGET_FILE:repl-object-runtime-plugin-23>)
          if(_dispatch STREQUAL "selected_dso")
            target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_SELECTED_DSO)
          elseif(_dispatch MATCHES "^discovered_(dso|namespace)$")
            target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_DISCOVERED_DSO)
            set(_runtime_mode discovered_dso)
            if(_dispatch STREQUAL "discovered_namespace")
              target_compile_definitions(${_runtime_fixture} PRIVATE TRACE_TEST_RUNTIME_NAMESPACE)
            endif()
          else()
            list(APPEND _runtime_plugin_args --plugin $<TARGET_FILE:repl-object-runtime-plugin-19>)
          endif()
        endif()
        add_test(NAME native_repl_object_runtime_${_dispatch}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_indirect.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:${_runtime_fixture}>
            --runtime ${_runtime_mode} --cfi ${_runtime_plugin_args})
        set_tests_properties(native_repl_object_runtime_${_dispatch} PROPERTIES TIMEOUT 120)
      endforeach()
      add_test(NAME native_repl_object_runtime_limit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_indirect.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-runtime-indirect-target>
          --runtime indirect --bounded --cfi)
      set_tests_properties(native_repl_object_runtime_limit PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_object_runtime_tail_limit
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_indirect.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-runtime-tail-target>
          --runtime tail --bounded --cfi)
      set_tests_properties(native_repl_object_runtime_tail_limit PROPERTIES TIMEOUT 120)
      foreach(_inline_region collection disjoint)
        set(_inline_target repl-object-inline-${_inline_region}-target)
        set(_inline_args)
        add_executable(${_inline_target} tests/fixtures/object_aarch64/collection.cpp)
        set_target_properties(${_inline_target} PROPERTIES CXX_STANDARD 17
          INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_inline_target} PRIVATE -g -O2 -fPIE
          -fno-optimize-sibling-calls ${_object_test_arch_flags})
        target_compile_definitions(${_inline_target} PRIVATE TRACE_TEST_INLINE_MUTATION)
        target_link_options(${_inline_target} PRIVATE -pie "LINKER:--build-id=sha1")
        if(_inline_region STREQUAL "disjoint")
          target_compile_definitions(${_inline_target} PRIVATE TRACE_TEST_DISJOINT_MUTATION)
          set(_inline_args --disjoint)
        endif()
        add_test(NAME native_repl_inline_${_inline_region}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_inline_collection.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:${_inline_target}> ${_inline_args})
        set_tests_properties(native_repl_inline_${_inline_region} PROPERTIES TIMEOUT 180)
      endforeach()
      foreach(_pointee raw unique shared)
        set(_pointee_target repl-object-pointee-target)
        set(_pointee_test native_repl_object_pointee)
        if(NOT _pointee STREQUAL "raw")
          set(_pointee_target repl-object-pointee-${_pointee}-target)
          string(APPEND _pointee_test "_${_pointee}")
        endif()
        add_executable(${_pointee_target} tests/fixtures/object_aarch64/pointee.cpp)
        set_target_properties(${_pointee_target} PROPERTIES CXX_STANDARD 17
          INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_pointee_target} PRIVATE -g -O2 -fPIE
          -fno-optimize-sibling-calls ${_object_test_arch_flags})
        target_link_options(${_pointee_target} PRIVATE -pie "LINKER:--build-id=sha1")
        if(NOT _pointee STREQUAL "raw")
          string(TOUPPER "${_pointee}" _pointee_kind)
          target_compile_definitions(${_pointee_target} PRIVATE TRACE_TEST_${_pointee_kind}_POINTEE)
        endif()
        add_test(NAME ${_pointee_test}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_pointee.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:${_pointee_target}>)
        set_tests_properties(${_pointee_test} PROPERTIES TIMEOUT 180)
      endforeach()
      add_executable(repl-object-signal-window-target tests/fixtures/object_aarch64/signal_window.cpp)
      set_target_properties(repl-object-signal-window-target PROPERTIES CXX_STANDARD 17
        INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-signal-window-target PRIVATE -g -O2 -fPIE
        -fno-optimize-sibling-calls ${_object_test_arch_flags})
      target_link_options(repl-object-signal-window-target PRIVATE -pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_object_signal_window
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_signal_window.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-signal-window-target>)
      set_tests_properties(native_repl_object_signal_window PROPERTIES TIMEOUT 180)
      foreach(_shared_kind storage scalar)
        set(_shared_fixture repl-object-shared-${_shared_kind}-target)
        set(_shared_args)
        add_executable(${_shared_fixture} tests/fixtures/object_aarch64/shared_storage.cpp)
        set_target_properties(${_shared_fixture} PROPERTIES CXX_STANDARD 17
          INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_shared_fixture} PRIVATE -g -O2 -fPIE
          -fno-optimize-sibling-calls ${_object_test_arch_flags})
        if(_shared_kind STREQUAL "scalar")
          target_compile_definitions(${_shared_fixture} PRIVATE TRACE_TEST_SHARED_SCALAR)
          set(_shared_args --caller-lock)
        elseif(TARGET clang_tracer_object_cache_arm)
          target_compile_options(${_shared_fixture} PRIVATE -march=armv8.1-a+lse)
        endif()
        target_link_options(${_shared_fixture} PRIVATE -pie "LINKER:--build-id=sha1")
        add_test(NAME native_repl_object_shared_${_shared_kind}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_shared_storage.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:${_shared_fixture}> ${_shared_args})
        set_tests_properties(native_repl_object_shared_${_shared_kind} PROPERTIES TIMEOUT 180)
      endforeach()
      if(TARGET clang_tracer_object_cache_arm)
        add_executable(repl-object-arm-parked-fault-target tests/fixtures/object_aarch64/fault.cpp)
        set_target_properties(repl-object-arm-parked-fault-target PROPERTIES CXX_STANDARD 17
          INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_definitions(repl-object-arm-parked-fault-target PRIVATE TRACE_ARM_TEST_RENDEZVOUS)
        target_compile_options(repl-object-arm-parked-fault-target PRIVATE -g -O2 -fPIE
          -fno-optimize-sibling-calls)
        target_link_options(repl-object-arm-parked-fault-target PRIVATE -pie "LINKER:--build-id=sha1")
        add_test(NAME native_repl_object_static_active_reclaim
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_runtime_prefix_detach.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-arm-parked-fault-target> --arm-static)
        set_tests_properties(native_repl_object_static_active_reclaim PROPERTIES TIMEOUT 120)
      endif()
      if(TARGET clang_tracer_agent)
        foreach(_call_fixture repl-object-runtime-prefix-target repl-object-runtime-memory-target)
          add_executable(${_call_fixture}
            tests/fixtures/object_plan/record.cpp tests/fixtures/object_plan/runtime_prefix.cpp)
          target_compile_options(${_call_fixture} PRIVATE -g -O2 -fPIE -fcf-protection=none)
          target_link_options(${_call_fixture} PRIVATE -pie "LINKER:--build-id=sha1")
          target_link_libraries(${_call_fixture} PRIVATE Threads::Threads)
        endforeach()
        target_compile_definitions(repl-object-runtime-memory-target PRIVATE TRACE_TEST_RUNTIME_MEMORY)
        add_test(NAME native_repl_object_runtime_prefix_detach
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_runtime_prefix_detach.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-runtime-prefix-target>)
        set_tests_properties(native_repl_object_runtime_prefix_detach PROPERTIES TIMEOUT 120)
        add_test(NAME native_repl_object_static_active_reclaim
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_runtime_prefix_detach.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-runtime-prefix-target> --static)
        set_tests_properties(native_repl_object_static_active_reclaim PROPERTIES TIMEOUT 120)
        add_test(NAME native_repl_object_runtime_active_reclaim
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_runtime_prefix_detach.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-runtime-prefix-target> --callee)
        set_tests_properties(native_repl_object_runtime_active_reclaim PROPERTIES TIMEOUT 120)
        add_test(NAME native_repl_object_runtime_memory_detach
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_runtime_prefix_detach.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-runtime-memory-target>)
        set_tests_properties(native_repl_object_runtime_memory_detach PROPERTIES TIMEOUT 120)
      endif()
      add_executable(repl-object-reexec-target
        tests/fixtures/object_plan/reexec.cpp tests/fixtures/object_plan/record.cpp)
      set_target_properties(repl-object-reexec-target PROPERTIES CXX_STANDARD 17
        INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-reexec-target PRIVATE -g -O2 -fPIE
        -fno-optimize-sibling-calls ${_object_test_arch_flags})
      target_link_options(repl-object-reexec-target PRIVATE -pie)
      set(_object_reexec_args)
      if(TARGET clang_tracer_object_cache_arm)
        set(_object_reexec_args --object-watch $<TARGET_FILE:clang-tracer-object-watch>)
      endif()
      add_test(NAME native_repl_code_cache_reexec
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_reexec.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE:repl-object-reexec-target> ${_object_reexec_args})
      set_tests_properties(native_repl_code_cache_reexec PROPERTIES TIMEOUT 100)
      set(_object_overlap_cases ordered external)
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        list(APPEND _object_overlap_cases idle_attach worker_attach sve_state sme_state)
      endif()
      foreach(_object_case IN LISTS _object_overlap_cases)
        set(_object_overlap_args --${_object_case} ${_object_test_rendezvous})
        if(_object_case STREQUAL "external")
          set(_object_source tests/fixtures/object_overlap/external.cpp)
        else()
          set(_object_source tests/fixtures/object_overlap/fixture.cpp)
        endif()
        set(_object_target repl-object-${_object_case}-target)
        set(_object_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/object-${_object_case}-fixture/$<CONFIG>")
        file(GENERATE OUTPUT "${_object_fixture_dir}/fixture.cpp"
          INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${_object_source}")
        add_executable(${_object_target} "${_object_source}")
        set_target_properties(${_object_target} PROPERTIES CXX_STANDARD 17 OUTPUT_NAME fixture
          RUNTIME_OUTPUT_DIRECTORY "${_object_fixture_dir}")
        target_compile_options(${_object_target} PRIVATE -g -O2 -fno-inline
          ${_object_test_arch_flags})
        if(_object_case MATCHES "^(idle_attach|worker_attach|sve_state|sme_state)$")
          target_compile_definitions(${_object_target} PRIVATE TRACE_TEST_IDLE_ATTACH)
          set(_object_overlap_args --ordered)
          if(_object_case STREQUAL "worker_attach")
            target_compile_definitions(${_object_target} PRIVATE TRACE_TEST_WORKER_ATTACH)
            list(APPEND _object_overlap_args --worker-attach)
          elseif(_object_case STREQUAL "sve_state")
            target_compile_definitions(${_object_target} PRIVATE TRACE_TEST_SVE_STATE)
            target_compile_options(${_object_target} PRIVATE -march=armv8.2-a+sve)
            list(APPEND _object_overlap_args --sve-state)
          elseif(_object_case STREQUAL "sme_state")
            target_compile_definitions(${_object_target} PRIVATE TRACE_TEST_SME_STATE)
            target_compile_options(${_object_target} PRIVATE -march=armv9-a+sme2)
            list(APPEND _object_overlap_args --sme-state)
          endif()
        endif()
        target_link_libraries(${_object_target} PRIVATE Threads::Threads)
        add_test(NAME native_repl_code_cache_${_object_case}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_overlap.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:${_object_cache_agent}> $<TARGET_FILE_DIR:${_object_target}>
            ${_object_overlap_args})
        set_tests_properties(native_repl_code_cache_${_object_case} PROPERTIES TIMEOUT 100)
        if(_object_case MATCHES "^(sve_state|sme_state)$")
          set_tests_properties(native_repl_code_cache_${_object_case} PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
        endif()
      endforeach()
    endif()
    if(NOT TRACER_REPL_ONLY AND TARGET clang_tracer_agent)
      add_test(NAME native_repl_module_index
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_module_index.py"
          --repl $<TARGET_FILE:clang-tracer-repl> --server $<TARGET_FILE:clang-tracer-server>
          --agent $<TARGET_FILE:clang_tracer_agent> --module-tool $<TARGET_FILE:clang-tracer-module>
          --compiler "${CMAKE_CXX_COMPILER}")
      set_tests_properties(native_repl_module_index PROPERTIES TIMEOUT 240)
      add_executable(repl-browser-target tests/fixtures/repl_browser.cpp)
      target_link_libraries(repl-browser-target PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
      target_compile_options(repl-browser-target PRIVATE -g -O0 -fno-omit-frame-pointer)
      add_test(NAME native_repl_browser_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_browser_integration.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:repl-browser-target> $<TARGET_FILE:call-stacks-calculator>)
      set_tests_properties(native_repl_browser_integration PROPERTIES TIMEOUT 100)
      add_executable(repl-memory-target tests/fixtures/repl_memory.cpp)
      add_test(NAME native_repl_memory_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_memory_integration.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:repl-memory-target>)
      set_tests_properties(native_repl_memory_integration PROPERTIES TIMEOUT 100)
      foreach(_entry_target repl-entry-target repl-entry-stripped-target)
        add_executable(${_entry_target} tests/fixtures/repl_entry.cpp)
        target_compile_options(${_entry_target} PRIVATE -g -O0 -fno-omit-frame-pointer
          -fcf-protection=none -fPIE)
        target_link_options(${_entry_target} PRIVATE -pie "LINKER:--build-id=sha1")
      endforeach()
      target_link_options(repl-entry-stripped-target PRIVATE "LINKER:--strip-all")
      add_test(NAME native_repl_entry_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_entry_integration.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:repl-entry-target> $<TARGET_FILE:repl-entry-stripped-target> "${CMAKE_NM}")
      set_tests_properties(native_repl_entry_integration PROPERTIES TIMEOUT 100)
      add_executable(repl-object-watch-target tests/fixtures/repl_object_watch.cpp)
      set_target_properties(repl-object-watch-target PROPERTIES CXX_STANDARD 17)
      target_compile_options(repl-object-watch-target PRIVATE -g -O2 -fno-omit-frame-pointer -fcf-protection=none -fPIE)
      target_link_options(repl-object-watch-target PRIVATE -pie "LINKER:--build-id=sha1")
      target_link_libraries(repl-object-watch-target PRIVATE Threads::Threads)
      add_test(NAME native_repl_object_watch
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_watch.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-watch-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repl_object_watch.cpp")
      set_tests_properties(native_repl_object_watch PROPERTIES TIMEOUT 100)
      add_executable(repl-small-getter-target tests/fixtures/repl_small_getter.cpp)
      set_target_properties(repl-small-getter-target PROPERTIES CXX_STANDARD 17)
      target_compile_options(repl-small-getter-target PRIVATE -g -O2 -fno-omit-frame-pointer
        -momit-leaf-frame-pointer -fno-optimize-sibling-calls -fcf-protection=none -fno-pie)
      target_link_options(repl-small-getter-target PRIVATE -no-pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_small_getter
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_small_getter.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-small-getter-target>)
      set_tests_properties(native_repl_small_getter PROPERTIES TIMEOUT 120)
      add_executable(repl-person-target tests/fixtures/person/person.cpp tests/fixtures/person/driver.cpp)
      set_target_properties(repl-person-target PROPERTIES CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-person-target PRIVATE -g -O2 -fno-omit-frame-pointer -fcf-protection=none -fno-pie)
      target_link_options(repl-person-target PRIVATE -no-pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_person_store
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_person_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-person-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/person/person.cpp")
      set_tests_properties(native_repl_person_store PROPERTIES TIMEOUT 100)
      foreach(_record_variant repl-object-plan-target repl-object-plan-rebuilt-target repl-object-union-target)
        add_executable(${_record_variant}
          tests/fixtures/object_plan/record.cpp tests/fixtures/object_plan/driver.cpp)
        set_target_properties(${_record_variant} PROPERTIES
          CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_record_variant} PRIVATE
          -g -O2 -fPIE -fno-optimize-sibling-calls -fcf-protection=none)
        target_link_options(${_record_variant} PRIVATE -pie "LINKER:--build-id=sha1")
        target_link_libraries(${_record_variant} PRIVATE Threads::Threads)
      endforeach()
      target_compile_definitions(repl-object-plan-rebuilt-target PRIVATE OBJECT_PLAN_PADDED)
      target_compile_definitions(repl-object-union-target PRIVATE OBJECT_PLAN_UNION)
      add_test(NAME native_repl_object_plan
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_plan.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-plan-target>
          $<TARGET_FILE:repl-object-plan-rebuilt-target>)
      set_tests_properties(native_repl_object_plan PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_object_union
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_union.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-union-target>)
      set_tests_properties(native_repl_object_union PROPERTIES TIMEOUT 120)
      add_executable(repl-object-lifetime-target tests/fixtures/object_lifetime/fixture.cpp)
      set_target_properties(repl-object-lifetime-target PROPERTIES
        CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-lifetime-target PRIVATE
        -g -O0 -fno-omit-frame-pointer -fPIE -fcf-protection=none)
      target_link_options(repl-object-lifetime-target PRIVATE -pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_object_lifetime
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_lifetime.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-lifetime-target> --check-reuse)
      set_tests_properties(native_repl_object_lifetime PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_object_lifetime_native
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_lifetime.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-lifetime-target> --check-reuse --backend code-cache)
      set_tests_properties(native_repl_object_lifetime_native PROPERTIES TIMEOUT 120)
      foreach(_optimization O0 O2)
        set(_unpadded_target repl-object-unpadded-${_optimization}-target)
        add_executable(${_unpadded_target} tests/fixtures/object_lifetime/unpadded.cpp
          tests/fixtures/object_lifetime/unpadded_main.cpp)
        set_target_properties(${_unpadded_target} PROPERTIES
          CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_unpadded_target} PRIVATE -g -${_optimization} -fno-inline
          -falign-functions=16 -fno-omit-frame-pointer -fPIE -fcf-protection=none)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
          target_compile_options(${_unpadded_target} PRIVATE -fstandalone-debug)
        endif()
        target_link_options(${_unpadded_target} PRIVATE -pie "LINKER:--build-id=sha1")
        if(_optimization STREQUAL "O0")
          set(_unpadded_cases vi)
        else()
          set(_unpadded_cases vd)
        endif()
        foreach(_backend ptrace code-cache)
          set(_cases ${_unpadded_cases})
          if(_optimization STREQUAL "O2" AND _backend STREQUAL "ptrace")
            string(APPEND _cases c)
          endif()
          set(_test native_repl_object_unpadded_${_optimization}_${_backend})
          add_test(NAME ${_test}
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_unpadded.py"
              $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
              $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:${_unpadded_target}>
              ${_cases} --backend ${_backend})
          set_tests_properties(${_test} PROPERTIES TIMEOUT 120)
        endforeach()
      endforeach()
      add_test(NAME native_repl_object_lifetime_tail_callers
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_unpadded.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-unpadded-O2-target>
          xy --backend code-cache --rejection "unverified tail caller")
      set_tests_properties(native_repl_object_lifetime_tail_callers PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_object_hidden_writer_call
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_unpadded.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-unpadded-O2-target>
          h --backend code-cache --rejection "cannot verify a potential code-cache call or branch")
      set_tests_properties(native_repl_object_hidden_writer_call PROPERTIES TIMEOUT 120)
      foreach(_optimization O0 O2)
        set(_string_target repl-object-string-lifetime-${_optimization}-target)
        add_executable(${_string_target} tests/fixtures/object_lifetime/unpadded.cpp
          tests/fixtures/object_lifetime/unpadded_main.cpp)
        set_target_properties(${_string_target} PROPERTIES CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_string_target} PRIVATE -g -${_optimization} -falign-functions=1
          -fno-sized-deallocation -fPIE -fcf-protection=none)
        target_link_options(${_string_target} PRIVATE -pie "LINKER:--build-id=sha1")
        add_test(NAME native_repl_object_string_lifetime_${_optimization}
          COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_unpadded.py"
            $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
            $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:${_string_target}> tp --backend code-cache)
        set_tests_properties(native_repl_object_string_lifetime_${_optimization} PROPERTIES TIMEOUT 120)
      endforeach()
      add_executable(repl-object-follow-lifetime-target tests/fixtures/object_lifetime/follow_main.cpp
        tests/fixtures/object_lifetime/follow_order.cpp)
      set_target_properties(repl-object-follow-lifetime-target PROPERTIES
        CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-follow-lifetime-target PRIVATE -g -O2 -fPIE -fcf-protection=none)
      target_link_options(repl-object-follow-lifetime-target PRIVATE -pie "LINKER:--build-id=sha1")
      add_test(NAME native_repl_object_follow_lifetime
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_follow_lifetime.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-follow-lifetime-target>)
      set_tests_properties(native_repl_object_follow_lifetime PROPERTIES TIMEOUT 120)
      foreach(_indirect_target repl-object-indirect-target repl-object-multi-indirect-target)
        add_executable(${_indirect_target}
          tests/fixtures/object_plan/record.cpp tests/fixtures/object_plan/indirect.cpp)
        set_target_properties(${_indirect_target} PROPERTIES
          CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
        target_compile_options(${_indirect_target} PRIVATE
          -g -O2 -fPIE -fomit-frame-pointer -fno-optimize-sibling-calls -fcf-protection=none)
        target_link_options(${_indirect_target} PRIVATE -pie "LINKER:--build-id=sha1")
      endforeach()
      target_compile_definitions(repl-object-multi-indirect-target PRIVATE TRACE_TEST_MULTI_INDIRECT)
      add_test(NAME native_repl_object_indirect
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_indirect.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-indirect-target>)
      set_tests_properties(native_repl_object_indirect PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_object_multi_indirect
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_indirect.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-multi-indirect-target>
          --entry-multi)
      set_tests_properties(native_repl_object_multi_indirect PROPERTIES TIMEOUT 120)
      add_executable(repl-object-origin-target tests/fixtures/object_origin/main.cpp
        tests/fixtures/object_origin/record.cpp tests/fixtures/object_origin/opaque.cpp)
      set_target_properties(repl-object-origin-target PROPERTIES
        CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-object-origin-target PRIVATE
        -g -O2 -fPIE -fno-optimize-sibling-calls -fcf-protection=none)
      target_link_options(repl-object-origin-target PRIVATE -pie "LINKER:--build-id=sha1")
      set_source_files_properties(tests/fixtures/object_origin/opaque.cpp
        PROPERTIES COMPILE_OPTIONS "-g0")
      add_test(NAME native_repl_object_origin
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_object_origin.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-object-origin-target> --limits)
      set_tests_properties(native_repl_object_origin PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_code_cache_faults
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_person_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-person-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/person/person.cpp" --backend code-cache --faults)
      set_tests_properties(native_repl_code_cache_faults PROPERTIES TIMEOUT 120)
      add_library(repl-shared-shapes-library SHARED tests/fixtures/shared_shapes/library.cpp)
      add_executable(repl-shared-shapes-target tests/fixtures/shared_shapes/driver.cpp)
      set_target_properties(repl-shared-shapes-library repl-shared-shapes-target
        PROPERTIES CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-shared-shapes-library PRIVATE
        -g -O2 -fno-optimize-sibling-calls -fcf-protection=none)
      target_compile_options(repl-shared-shapes-target PRIVATE
        -g -O2 -fno-pie -fno-optimize-sibling-calls -fcf-protection=none)
      target_link_options(repl-shared-shapes-library PRIVATE "LINKER:--build-id=sha1")
      target_link_options(repl-shared-shapes-target PRIVATE -no-pie "LINKER:--build-id=sha1")
      target_link_libraries(repl-shared-shapes-target PRIVATE ${CMAKE_DL_LIBS})
      add_test(NAME native_repl_code_cache_shared
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_shared_object_cache.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-shared-shapes-target>
          $<TARGET_FILE:repl-shared-shapes-library>)
      set_tests_properties(native_repl_code_cache_shared PROPERTIES TIMEOUT 120)
      add_executable(repl-atomic-person-target
        tests/fixtures/atomic_person/operations.cpp tests/fixtures/atomic_person/driver.cpp)
      set_target_properties(repl-atomic-person-target PROPERTIES CXX_STANDARD 17 INTERPROCEDURAL_OPTIMIZATION OFF)
      target_compile_options(repl-atomic-person-target PRIVATE -g -O2 -fno-omit-frame-pointer -fcf-protection=none -fno-pie)
      target_link_options(repl-atomic-person-target PRIVATE -no-pie "LINKER:--build-id=sha1")
      target_link_libraries(repl-atomic-person-target PRIVATE Threads::Threads)
      add_test(NAME native_repl_atomic_store
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_atomic_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-atomic-person-target>)
      set_tests_properties(native_repl_atomic_store PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_atomic_live_detach
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_atomic_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-atomic-person-target>
          --detach --iterations 1000 --trials 2)
      set_tests_properties(native_repl_atomic_live_detach PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_code_cache_atomic
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_atomic_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-atomic-person-target>
          --backend code-cache)
      add_test(NAME native_repl_code_cache_live_detach
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_atomic_store.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-atomic-person-target>
          --backend code-cache --detach --iterations 1000 --trials 2)
      set_tests_properties(native_repl_code_cache_atomic native_repl_code_cache_live_detach
        PROPERTIES TIMEOUT 120)
      add_executable(repl-reload-target tests/fixtures/repl_reload.cpp)
      target_compile_options(repl-reload-target PRIVATE -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
      target_link_libraries(repl-reload-target PRIVATE Threads::Threads)
      add_test(NAME native_repl_reload_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_reload_integration.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:repl-reload-target>)
      set_tests_properties(native_repl_reload_integration PROPERTIES TIMEOUT 240)
      add_executable(repl-bulk-concrete-target
        examples/prepared-injectors/concrete/target.cpp
        examples/prepared-injectors/concrete/virtual.cpp)
      target_compile_options(repl-bulk-concrete-target PRIVATE -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
      add_test(NAME native_repl_bulk_preview
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_bulk_preview.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_name_fixture.cpp" $<TARGET_FILE:repl-bulk-concrete-target>
          $<TARGET_FILE:repl-browser-target> $<TARGET_FILE:call-stacks-calculator>
          "${CMAKE_CXX_COMPILER}")
      set_tests_properties(native_repl_bulk_preview PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_bulk_apply
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_bulk_apply.py"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:cpp-name-fixture>)
      set_tests_properties(native_repl_bulk_apply PROPERTIES TIMEOUT 120)
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
      foreach(_templates_target repl-headerless-templates-target repl-elf-signature-target)
        add_executable(${_templates_target} tests/fixtures/repl_headerless_templates.cpp)
        target_compile_options(${_templates_target} PRIVATE -g -O0
          -fno-omit-frame-pointer -fcf-protection=none)
      endforeach()
      target_link_options(repl-elf-signature-target PRIVATE "LINKER:--strip-debug")
      add_test(NAME native_repl_headerless_templates
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_headerless_templates.sh"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-headerless-templates-target>)
      set_tests_properties(native_repl_headerless_templates PROPERTIES TIMEOUT 180)
      add_test(NAME native_repl_elf_signature
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_elf_signature.sh"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:repl-elf-signature-target>)
      set_tests_properties(native_repl_elf_signature PROPERTIES TIMEOUT 180)
      add_test(NAME native_repl_prepared_concrete
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_prepared_concrete.sh"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:prepared-concrete-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors/concrete/types.hpp")
      set_tests_properties(native_repl_prepared_concrete PROPERTIES TIMEOUT 240)
      add_test(NAME native_repl_flags_generator
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/flags_generator_regression.py"
          $<TARGET_FILE:clang-tracer-prepare-injectors-native> "${CMAKE_CXX_COMPILER}")
      set_tests_properties(native_repl_flags_generator PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_nested_dependency_generator
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/flags_generator_regression.py"
          $<TARGET_FILE:clang-tracer-prepare-injectors-native> "${CMAKE_CXX_COMPILER}"
          nested-dependency)
      set_tests_properties(native_repl_nested_dependency_generator PROPERTIES TIMEOUT 120)
      add_test(NAME native_repl_prepared_history
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_prepared_history.sh"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer-server>
          $<TARGET_FILE:clang_tracer_agent> $<TARGET_FILE:prepared-history-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors/concrete/history.hpp")
      set_tests_properties(native_repl_prepared_history PROPERTIES TIMEOUT 240)
      add_test(NAME native_repl_sqlite_types
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_sqlite_types.sh"
          $<TARGET_FILE:clang-tracer-repl> $<TARGET_FILE:clang-tracer>
          $<TARGET_FILE:clang-tracer-server> $<TARGET_FILE:clang_tracer_agent>
          $<TARGET_FILE:prepared-methods-target>
          "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors/methods")
      set_tests_properties(native_repl_sqlite_types PROPERTIES TIMEOUT 180)
      foreach(_example methods state)
        add_test(NAME native_repl_prepared_${_example}
          COMMAND "${CMAKE_COMMAND}" -E env
            "CLANG_TRACER_PREPARED_BUILD=${CMAKE_CURRENT_BINARY_DIR}"
            "CLANG_TRACER_REPL=$<TARGET_FILE:clang-tracer-repl>"
            "CLANG_TRACER_SERVER=$<TARGET_FILE:clang-tracer-server>"
            "CLANG_TRACER_AGENT=$<TARGET_FILE:clang_tracer_agent>"
            "CLANG_TRACER_PREPARED_TARGET=$<TARGET_FILE:prepared-methods-target>"
            bash "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors/run-native.sh"
            --${_example} --smoke)
        set_tests_properties(native_repl_prepared_${_example} PROPERTIES TIMEOUT 120)
      endforeach()
      foreach(_example pointee polymorphic-pointee pimpl-pointer pimpl-reference pimpl-custom pimpl-roles)
        string(REPLACE "-" "_" _test_suffix "${_example}")
        if(_example MATCHES "^pimpl-")
          set(_prepared_example_target prepared-pimpl-target)
        else()
          set(_prepared_example_target prepared-${_example}-target)
        endif()
        add_test(NAME native_repl_prepared_${_test_suffix}
          COMMAND "${CMAKE_COMMAND}" -E env
            "CLANG_TRACER_PREPARED_BUILD=${CMAKE_CURRENT_BINARY_DIR}"
            "CLANG_TRACER_REPL=$<TARGET_FILE:clang-tracer-repl>"
            "CLANG_TRACER_SERVER=$<TARGET_FILE:clang-tracer-server>"
            "CLANG_TRACER_AGENT=$<TARGET_FILE:clang_tracer_agent>"
            "CLANG_TRACER_PREPARED_TARGET=$<TARGET_FILE:${_prepared_example_target}>"
            bash "${CMAKE_CURRENT_SOURCE_DIR}/examples/prepared-injectors/run-native.sh"
            --${_example} --smoke)
        set_tests_properties(native_repl_prepared_${_test_suffix} PROPERTIES TIMEOUT 120)
      endforeach()
      add_test(NAME native_repl_integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_native_integration.py"
          --repl $<TARGET_FILE:clang-tracer-repl> --server $<TARGET_FILE:clang-tracer-server>
          --target $<TARGET_FILE:cpp-interceptors-target>
          --bool-target $<TARGET_FILE:signature-interceptor-fixture>
          --incompatible-agent $<TARGET_FILE:call-stacks-calculator>
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
