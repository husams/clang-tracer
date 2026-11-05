cmake_minimum_required(VERSION 3.20)
project(clang_tracer LANGUAGES C CXX)
option(TRACER_BUILD_REPL "Build the native C++17 interactive client" ON)
option(TRACER_REPL_ONLY "Build only the portable C++17 REPL client and its tests" OFF)
if(TRACER_REPL_ONLY)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  include(GNUInstallDirs)
  include(CTest)
  include(cmake/TracerNativeRepl.cmake)
  tracer_add_native_repl()
  return()
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/TracerCompatibility.cmake)

option(TRACER_CONTEXT_ENGINE_ONLY "Build only portable context engine checks" OFF)
if(TRACER_CONTEXT_ENGINE_ONLY)
  include(CTest)
  include(cmake/TracerContextEngine.cmake)
  option(TRACER_CONTEXT_ANALYSIS_TESTS "Also build Clang declaration checks" OFF)
  if(TRACER_CONTEXT_ANALYSIS_TESTS)
    include(cmake/TracerToolchain.cmake)
    include(cmake/TracerContextAnalysis.cmake)
  endif()
  return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  message(FATAL_ERROR "The native tracing backend requires Linux x86_64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)
include(CTest)
include(cmake/TracerContextEngine.cmake)
include(cmake/TracerToolchain.cmake)
include(cmake/TracerContextAnalysis.cmake)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)

set(TRACER_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${TRACER_PROTO_DIR})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/callback.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/context.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/protocol.hpp)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/protocol.hpp TRACER_PROTOCOL_HEADER)
# Incremental input is a synthetic main file, not an includable disk header.
string(REPLACE "#pragma once" "" TRACER_PROTOCOL_HEADER "${TRACER_PROTOCOL_HEADER}")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/callback.hpp TRACER_CALLBACK_HEADER)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/include/tracer/context.hpp TRACER_CONTEXT_HEADER)
configure_file(src/callback_source.hpp.in
  ${TRACER_PROTO_DIR}/callback_source.hpp @ONLY)
add_custom_command(
  OUTPUT ${TRACER_PROTO_DIR}/tracer.pb.cc ${TRACER_PROTO_DIR}/tracer.pb.h
         ${TRACER_PROTO_DIR}/tracer.grpc.pb.cc ${TRACER_PROTO_DIR}/tracer.grpc.pb.h
  COMMAND protobuf::protoc
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --proto_path=${CMAKE_CURRENT_BINARY_DIR}/context-generated
       --cpp_out=${TRACER_PROTO_DIR} --grpc_out=${TRACER_PROTO_DIR}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
  DEPENDS proto/tracer.proto proto/context_transport.proto protobuf::protoc
          gRPC::grpc_cpp_plugin tracer_context_transport_proto
  VERBATIM)
add_library(tracer_proto STATIC ${TRACER_PROTO_DIR}/tracer.pb.cc)
set_target_properties(tracer_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tracer_proto PUBLIC ${TRACER_PROTO_DIR})
target_link_libraries(tracer_proto PUBLIC protobuf::libprotobuf tracer_context_transport_proto tracer_compat)
add_library(tracer_grpc STATIC ${TRACER_PROTO_DIR}/tracer.grpc.pb.cc)
target_link_libraries(tracer_grpc PUBLIC tracer_proto gRPC::grpc++)

add_library(clang_tracer_agent SHARED src/agent.cpp src/value_snapshot_codegen.cpp src/value_snapshot_storage.cpp src/callback_pch.cpp src/debug_resolver.cpp src/native_stack.cpp
  src/event_transport.cpp src/context_agent_delivery.cpp)
target_include_directories(clang_tracer_agent PRIVATE include ${TRACER_PROTO_DIR} ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(clang_tracer_agent PRIVATE TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
target_link_libraries(clang_tracer_agent PRIVATE tracer_context_engine tracer_context_analysis tracer_proto clang-cpp LLVM ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_options(clang_tracer_agent PRIVATE -Wall -Wextra -Wpedantic)
# Localize archive definitions as well as symbols emitted by agent sources.
# --exclude-libs,ALL would also hide the required pthread detour entry point
# from tracer_context_engine; the version script preserves that exact export.
target_link_options(clang_tracer_agent PRIVATE
  "LINKER:--no-undefined,-Bsymbolic,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/agent_exports.map")
set_property(TARGET clang_tracer_agent APPEND PROPERTY LINK_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/agent_exports.map
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckAgentSymbols.cmake)
add_custom_command(TARGET clang_tracer_agent POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DAGENT=$<TARGET_FILE:clang_tracer_agent>
    -DNM=${CMAKE_NM} -DREADELF=${CMAKE_READELF}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckAgentSymbols.cmake
  VERBATIM)

add_executable(clang-tracer-server src/main.cpp src/server.cpp src/session.cpp src/injector.cpp src/admission.cpp
  src/debug_resolver.cpp src/header_context.cpp)
target_include_directories(clang-tracer-server PRIVATE include ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(clang-tracer-server PRIVATE TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
target_link_libraries(clang-tracer-server PRIVATE tracer_grpc tracer_context_collector
  tracer_context_analysis clang-cpp LLVM ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_options(clang-tracer-server PRIVATE -Wall -Wextra -Wpedantic)

add_executable(clang-tracer src/client.cpp src/debug_resolver.cpp src/header_context.cpp)
target_include_directories(clang-tracer PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(clang-tracer PRIVATE TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
target_link_libraries(clang-tracer PRIVATE tracer_grpc tracer_context_analysis clang-cpp LLVM Threads::Threads SQLite::SQLite3)
target_compile_options(clang-tracer PRIVATE -Wall -Wextra -Wpedantic)

if(TRACER_BUILD_REPL)
  include(cmake/TracerNativeRepl.cmake)
  tracer_add_native_repl()
endif()
include(cmake/TracerInstall.cmake)

option(TRACER_BUILD_EXAMPLES "Build example target programs" ON)
if(TRACER_BUILD_EXAMPLES OR BUILD_TESTING)
add_executable(trace-target tests/fixtures/target.cpp)
target_link_libraries(trace-target PRIVATE Threads::Threads)
set_target_properties(trace-target PROPERTIES ENABLE_EXPORTS ON)
target_compile_options(trace-target PRIVATE -O0 -fno-omit-frame-pointer -fcf-protection=none)

add_executable(cpp-interceptors-target examples/cpp-interceptors/target.cpp)
target_compile_options(cpp-interceptors-target PRIVATE -g -O0 -fno-omit-frame-pointer
  -fcf-protection=none)

add_executable(signature-interceptor-fixture tests/signature_interceptor_fixture.cpp)
add_executable(repl-complex-fixture tests/repl_complex_fixture.cpp)
if(BUILD_TESTING)
  add_executable(repl-values-example-target examples/native-repl-values/target.cpp)
  target_link_libraries(repl-values-example-target PRIVATE Threads::Threads)
  set_target_properties(repl-values-example-target PROPERTIES ENABLE_EXPORTS ON
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  target_compile_options(repl-values-example-target PRIVATE -g -O0
    -fno-omit-frame-pointer -fcf-protection=none)
endif()
target_link_libraries(repl-complex-fixture PRIVATE Threads::Threads)
set_target_properties(repl-complex-fixture PROPERTIES ENABLE_EXPORTS ON CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(repl-complex-fixture PRIVATE -g -O0 -fno-omit-frame-pointer -fcf-protection=none)
set_target_properties(signature-interceptor-fixture PROPERTIES ENABLE_EXPORTS ON
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_options(signature-interceptor-fixture PRIVATE -g -O0
  -fno-omit-frame-pointer -fcf-protection=none)

add_executable(shared-context-target examples/shared-context/target.cpp)
target_link_libraries(shared-context-target PRIVATE Threads::Threads)
target_compile_options(shared-context-target PRIVATE -g -O0 -fno-omit-frame-pointer
  -fcf-protection=none)

add_executable(order-arguments-target examples/order-arguments/target.cpp)
target_compile_options(order-arguments-target PRIVATE -g -O0 -fno-omit-frame-pointer -fcf-protection=none)

add_library(call-stacks-calculator SHARED examples/call-stacks/calculator.cpp)
target_compile_options(call-stacks-calculator PRIVATE -g -O0 -fomit-frame-pointer
  -fno-optimize-sibling-calls -fasynchronous-unwind-tables -fcf-protection=none)
add_library(call-stacks-no-cfi OBJECT examples/call-stacks/no_cfi.cpp)
target_compile_options(call-stacks-no-cfi PRIVATE -g -O1 -fomit-frame-pointer
  -fno-optimize-sibling-calls -fno-exceptions -fno-asynchronous-unwind-tables
  -fno-unwind-tables -fcf-protection=none)
add_executable(call-stacks-target examples/call-stacks/target.cpp
  $<TARGET_OBJECTS:call-stacks-no-cfi>)
target_link_libraries(call-stacks-target PRIVATE call-stacks-calculator Threads::Threads)
target_compile_options(call-stacks-target PRIVATE -g -O1 -fno-inline -fomit-frame-pointer
  -fno-optimize-sibling-calls -fasynchronous-unwind-tables -fcf-protection=none)

# Separate demo proves an application caller taking std::vector stays visible.
add_executable(call-stacks-filter-target examples/call-stacks/filter-target.cpp
  $<TARGET_OBJECTS:call-stacks-no-cfi>)
target_link_libraries(call-stacks-filter-target PRIVATE call-stacks-calculator Threads::Threads)
target_compile_options(call-stacks-filter-target PRIVATE -g -O1 -fno-inline -fomit-frame-pointer
  -fno-optimize-sibling-calls -fasynchronous-unwind-tables -fcf-protection=none)

add_executable(trace-threaded-target tests/fixtures/threaded/target.cpp)
add_library(trace-stack-b SHARED tests/fixtures/threaded/shared_stack_b.cpp)
add_library(trace-stack-a SHARED tests/fixtures/threaded/shared_stack_a.cpp)
target_link_libraries(trace-stack-a PRIVATE trace-stack-b)
foreach(stack_target trace-stack-a trace-stack-b)
  target_include_directories(${stack_target} PUBLIC tests/fixtures/threaded)
  target_compile_options(${stack_target} PRIVATE -O2 -fomit-frame-pointer
    -fno-optimize-sibling-calls -fasynchronous-unwind-tables -fcf-protection=none)
endforeach()
set_target_properties(trace-threaded-target PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(trace-threaded-target PRIVATE Threads::Threads trace-stack-a trace-stack-b)
target_compile_options(trace-threaded-target PRIVATE -O0 -fno-omit-frame-pointer
  -fno-optimize-sibling-calls -fcf-protection=none)

endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(callback-pch-tests tests/callback_pch_tests.cpp src/callback_pch.cpp)
  target_include_directories(callback-pch-tests PRIVATE src ${TRACER_PROTO_DIR})
  target_include_directories(callback-pch-tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_link_libraries(callback-pch-tests PRIVATE clang-cpp LLVM tracer_compat)
  target_compile_options(callback-pch-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME callback_pch_reuse
    COMMAND callback-pch-tests "${_tracer_resource_dir}")
  add_test(NAME callback_pch_unavailable
    COMMAND callback-pch-tests "${_tracer_resource_dir}" --unavailable-cache)
  set_tests_properties(callback_pch_reuse callback_pch_unavailable PROPERTIES TIMEOUT 60)
  add_test(NAME agent_symbol_isolation
    COMMAND ${CMAKE_COMMAND} -DAGENT=$<TARGET_FILE:clang_tracer_agent>
      -DNM=${CMAKE_NM} -DREADELF=${CMAKE_READELF}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckAgentSymbols.cmake)
  add_executable(trace-collision-target tests/fixtures/target.cpp
    tests/fixtures/dependency_collision.cpp)
  set_target_properties(trace-collision-target PROPERTIES ENABLE_EXPORTS ON)
  target_link_libraries(trace-collision-target PRIVATE Threads::Threads)
  target_compile_options(trace-collision-target PRIVATE -O0 -fno-omit-frame-pointer
    -fcf-protection=none)
  add_test(NAME grpc_agent_dependency_collision
    COMMAND server-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-collision-target>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/collision-test-results)
  set_tests_properties(agent_symbol_isolation PROPERTIES TIMEOUT 30)
  set_tests_properties(grpc_agent_dependency_collision PROPERTIES TIMEOUT 120)
  add_test(NAME grpc_agent_shared_dependency_collision
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/class_interceptor_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-collision-target>
      --threaded-target $<TARGET_FILE:trace-threaded-target>
      --protoc $<TARGET_FILE:protobuf::protoc>
      --proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
      --compdb ${CMAKE_CURRENT_BINARY_DIR}
      --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/target.cpp
      --threaded-source ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/threaded/target.cpp
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/collision-class-results)
  set_tests_properties(grpc_agent_shared_dependency_collision PROPERTIES TIMEOUT 360)
  add_executable(runtime-identity-tests tests/runtime_identity_checks.cpp src/runtime_identity.cpp)
  target_include_directories(runtime-identity-tests PRIVATE src)
  target_link_libraries(runtime-identity-tests PRIVATE tracer_compat Threads::Threads ${CMAKE_DL_LIBS})
  add_test(NAME runtime_module_identity COMMAND runtime-identity-tests)
  add_library(admission-leaf OBJECT tests/admission_leaf.cpp)
  target_compile_options(admission-leaf PRIVATE -O2 -fomit-frame-pointer -momit-leaf-frame-pointer
    -fasynchronous-unwind-tables -Wall -Wextra -Wpedantic)
  add_executable(admission-integration tests/admission_integration.cpp src/admission.cpp)
  target_include_directories(admission-integration PRIVATE src ${LLVM_INCLUDE_DIRS})
  target_link_libraries(admission-integration PRIVATE tracer_compat admission-leaf trace-stack-a trace-stack-b LLVM ${CMAKE_DL_LIBS})
  target_compile_options(admission-integration PRIVATE -O2 -fomit-frame-pointer
    -fno-optimize-sibling-calls -fasynchronous-unwind-tables -Wall -Wextra -Wpedantic)
  add_test(NAME native_admission_integration COMMAND admission-integration
    --expected-module $<TARGET_FILE:trace-stack-a>
    --expected-module $<TARGET_FILE:trace-stack-b>)
  set_tests_properties(native_admission_integration PROPERTIES TIMEOUT 30)
  add_executable(admission-frame-pointer-integration tests/admission_integration.cpp src/admission.cpp)
  target_include_directories(admission-frame-pointer-integration PRIVATE src ${LLVM_INCLUDE_DIRS})
  target_link_libraries(admission-frame-pointer-integration PRIVATE tracer_compat admission-leaf trace-stack-a trace-stack-b LLVM ${CMAKE_DL_LIBS})
  target_compile_options(admission-frame-pointer-integration PRIVATE -O2 -fno-omit-frame-pointer
    -fno-optimize-sibling-calls -fasynchronous-unwind-tables -Wall -Wextra -Wpedantic)
  add_test(NAME native_admission_frame_pointer_integration COMMAND admission-frame-pointer-integration
    --expected-module $<TARGET_FILE:trace-stack-a>
    --expected-module $<TARGET_FILE:trace-stack-b>)
  set_tests_properties(native_admission_frame_pointer_integration PROPERTIES TIMEOUT 30)
  add_custom_command(TARGET admission-integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:admission-integration>
      ${CMAKE_CURRENT_BINARY_DIR}/admission-integration-stripped
    COMMAND ${CMAKE_STRIP} --strip-all ${CMAKE_CURRENT_BINARY_DIR}/admission-integration-stripped
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:trace-stack-a>
      ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-a>
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:trace-stack-b>
      ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-b>
    COMMAND ${CMAKE_STRIP} --strip-all
      ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-a>
      ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-b>
    VERBATIM)
  add_test(NAME native_admission_stripped_integration
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/stripped-modules
      ${CMAKE_CURRENT_BINARY_DIR}/admission-integration-stripped
      --expected-module ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-a>
      --expected-module ${CMAKE_CURRENT_BINARY_DIR}/stripped-modules/$<TARGET_FILE_NAME:trace-stack-b>)
  set_tests_properties(native_admission_stripped_integration PROPERTIES TIMEOUT 30)
  add_executable(module-identity-integration tests/module_identity_integration.cpp src/admission.cpp)
  target_include_directories(module-identity-integration PRIVATE src ${LLVM_INCLUDE_DIRS})
  target_link_libraries(module-identity-integration PRIVATE tracer_compat LLVM ${CMAKE_DL_LIBS})
  target_compile_options(module-identity-integration PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME native_module_identity_integration COMMAND module-identity-integration
    --module $<TARGET_FILE:trace-stack-b>)
  set_tests_properties(native_module_identity_integration PROPERTIES TIMEOUT 30)
  add_test(NAME native_module_cfi_cache_integration COMMAND module-identity-integration
    --module $<TARGET_FILE:trace-stack-b> --cfi-cache-only)
  set_tests_properties(native_module_cfi_cache_integration PROPERTIES TIMEOUT 30)
  add_executable(server-integration tests/server_integration.cpp)
  target_link_libraries(server-integration PRIVATE tracer_grpc Threads::Threads)
  target_compile_options(server-integration PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME grpc_native_integration
    COMMAND server-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-target>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/test-results)
  set_tests_properties(grpc_native_integration PROPERTIES TIMEOUT 120)
  add_test(NAME grpc_client_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/client_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-target>
      --protoc $<TARGET_FILE:protobuf::protoc>
      --proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/client-test-results)
  set_tests_properties(grpc_client_integration PROPERTIES TIMEOUT 90)
  add_test(NAME grpc_friendly_client_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/friendly_client_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-target>
      --protoc $<TARGET_FILE:protobuf::protoc>
      --proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/friendly-test-results)
  set_tests_properties(grpc_friendly_client_integration PROPERTIES TIMEOUT 90)
  add_executable(cpp-name-fixture tests/cpp_name_fixture.cpp)
  target_compile_options(cpp-name-fixture PRIVATE -g -O0 -fno-omit-frame-pointer
    -fcf-protection=none)
  add_test(NAME grpc_cpp_name_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_name_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-target>
      --negative-target $<TARGET_FILE:cpp-name-fixture>
      --protoc $<TARGET_FILE:protobuf::protoc>
      --proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
      --compdb ${CMAKE_CURRENT_BINARY_DIR}
      --source ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/target.cpp
      --callback ${CMAKE_CURRENT_SOURCE_DIR}/examples/legacy-callbacks/callback.cpp
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/cpp-name-test-results)
  set_tests_properties(grpc_cpp_name_integration PROPERTIES TIMEOUT 180)
  configure_file(tests/installed_class_interceptor.cmake.in
    installed_class_interceptor.cmake.in @ONLY)
  file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/installed_class_interceptor-$<CONFIG>.cmake
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/installed_class_interceptor.cmake.in)
  add_test(NAME grpc_class_interceptor_integration
    COMMAND ${CMAKE_COMMAND} -P
      ${CMAKE_CURRENT_BINARY_DIR}/installed_class_interceptor-$<CONFIG>.cmake)
  set_tests_properties(grpc_class_interceptor_integration PROPERTIES TIMEOUT 360)
  add_executable(typed-this-fixture tests/typed_this_fixture.cpp)
  set_target_properties(typed-this-fixture PROPERTIES ENABLE_EXPORTS ON)
  target_compile_options(typed-this-fixture PRIVATE -O0 -fno-omit-frame-pointer
    -fcf-protection=none)
  add_test(NAME grpc_typed_this_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/typed_this_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:cpp-interceptors-target>
      --unsupported-target $<TARGET_FILE:typed-this-fixture>
      --protoc $<TARGET_FILE:protobuf::protoc>
      --proto ${CMAKE_CURRENT_SOURCE_DIR}/proto/tracer.proto
      --compdb ${CMAKE_CURRENT_BINARY_DIR}
      --source ${CMAKE_CURRENT_SOURCE_DIR}/examples/cpp-interceptors/target.cpp
      --unsupported-source ${CMAKE_CURRENT_SOURCE_DIR}/tests/typed_this_fixture.cpp
      --callback ${CMAKE_CURRENT_SOURCE_DIR}/examples/cpp-interceptors/method.cpp
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/typed-this-results)
  set_tests_properties(grpc_typed_this_integration PROPERTIES TIMEOUT 180)
  add_executable(blocked-main-fixture tests/fixtures/blocked_main.cpp)
  target_link_libraries(blocked-main-fixture PRIVATE Threads::Threads)
  set_target_properties(blocked-main-fixture PROPERTIES ENABLE_EXPORTS ON)
  target_compile_options(blocked-main-fixture PRIVATE -g -O0 -fno-omit-frame-pointer
    -fcf-protection=none)
  add_test(NAME grpc_blocked_main_integration
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/blocked_main_integration.py
      --server $<TARGET_FILE:clang-tracer-server>
      --client $<TARGET_FILE:clang-tracer>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:blocked-main-fixture>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/blocked-main-test-results)
  set_tests_properties(grpc_blocked_main_integration PROPERTIES TIMEOUT 180)
  add_executable(threaded-integration tests/threaded_integration.cpp)
  target_link_libraries(threaded-integration PRIVATE tracer_grpc Threads::Threads)
  target_compile_options(threaded-integration PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME grpc_threaded_integration
    COMMAND threaded-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-threaded-target>
      --expected-module $<TARGET_FILE:trace-stack-a>
      --expected-module $<TARGET_FILE:trace-stack-b>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/threaded-test-results)
  set_tests_properties(grpc_threaded_integration PROPERTIES TIMEOUT 180)
  add_test(NAME grpc_busy_main_integration
    COMMAND threaded-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-threaded-target>
      --busy-main
      --expected-module $<TARGET_FILE:trace-stack-a>
      --expected-module $<TARGET_FILE:trace-stack-b>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/busy-test-results)
  set_tests_properties(grpc_busy_main_integration PROPERTIES TIMEOUT 180)
  add_test(NAME grpc_nested_main_integration
    COMMAND threaded-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-threaded-target>
      --nested-main
      --expected-module $<TARGET_FILE:trace-stack-a>
      --expected-module $<TARGET_FILE:trace-stack-b>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/nested-test-results)
  set_tests_properties(grpc_nested_main_integration PROPERTIES TIMEOUT 180)
  add_test(NAME grpc_shared_main_integration
    COMMAND threaded-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:trace-threaded-target>
      --shared-main
      --expected-module $<TARGET_FILE:trace-stack-a>
      --expected-module $<TARGET_FILE:trace-stack-b>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/auto-test-results)
  set_tests_properties(grpc_shared_main_integration PROPERTIES TIMEOUT 180)
  add_executable(blocked-main-target tests/blocked_main_target.cpp)
  set_target_properties(blocked-main-target PROPERTIES ENABLE_EXPORTS ON)
  target_compile_options(blocked-main-target PRIVATE -g -O0 -fno-omit-frame-pointer
    -fcf-protection=none -Wall -Wextra -Wpedantic)
  target_link_libraries(blocked-main-target PRIVATE Threads::Threads)
  add_executable(blocked-main-integration tests/blocked_main_integration.cpp)
  target_link_libraries(blocked-main-integration PRIVATE tracer_grpc Threads::Threads)
  target_compile_options(blocked-main-integration PRIVATE -Wall -Wextra -Wpedantic)
  foreach(blocked_mode IN ITEMS positive-hot positive-sleeping all-blocked)
    string(REPLACE "-" "_" blocked_test_suffix "${blocked_mode}")
    add_test(NAME grpc_blocked_main_${blocked_test_suffix}
      COMMAND blocked-main-integration
        --server $<TARGET_FILE:clang-tracer-server>
        --agent $<TARGET_FILE:clang_tracer_agent>
        --target $<TARGET_FILE:blocked-main-target>
        --output-dir ${CMAKE_CURRENT_BINARY_DIR}/blocked-main-${blocked_test_suffix}
        --mode ${blocked_mode}
        --leader futex)
    set_tests_properties(grpc_blocked_main_${blocked_test_suffix} PROPERTIES TIMEOUT 90)
  endforeach()
  foreach(leader_mode IN ITEMS epoll read)
    add_test(NAME grpc_blocked_main_sleeping_${leader_mode}
      COMMAND blocked-main-integration
        --server $<TARGET_FILE:clang-tracer-server>
        --agent $<TARGET_FILE:clang_tracer_agent>
        --target $<TARGET_FILE:blocked-main-target>
        --output-dir ${CMAKE_CURRENT_BINARY_DIR}/blocked-main-sleeping-${leader_mode}
        --mode positive-sleeping
        --leader ${leader_mode})
    set_tests_properties(grpc_blocked_main_sleeping_${leader_mode} PROPERTIES TIMEOUT 90)
  endforeach()
  add_test(NAME grpc_blocked_main_external_job_stop
    COMMAND blocked-main-integration
      --server $<TARGET_FILE:clang-tracer-server>
      --agent $<TARGET_FILE:clang_tracer_agent>
      --target $<TARGET_FILE:blocked-main-target>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/blocked-main-external-job-stop
      --mode job-stopped
      --leader futex)
  set_tests_properties(grpc_blocked_main_external_job_stop PROPERTIES TIMEOUT 90)
endif()
