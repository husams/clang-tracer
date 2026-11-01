cmake_minimum_required(VERSION 3.20)
project(clang_tracer LANGUAGES C CXX)

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
target_link_libraries(tracer_proto PUBLIC protobuf::libprotobuf tracer_context_transport_proto)
add_library(tracer_grpc STATIC ${TRACER_PROTO_DIR}/tracer.grpc.pb.cc)
target_link_libraries(tracer_grpc PUBLIC tracer_proto gRPC::grpc++)

add_library(clang_tracer_agent SHARED src/agent.cpp src/debug_resolver.cpp src/native_stack.cpp
  src/event_transport.cpp src/context_agent_delivery.cpp)
target_include_directories(clang_tracer_agent PRIVATE include ${TRACER_PROTO_DIR} ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(clang_tracer_agent PRIVATE TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
target_link_libraries(clang_tracer_agent PRIVATE tracer_context_engine tracer_context_analysis tracer_proto clang-cpp LLVM ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_options(clang_tracer_agent PRIVATE -Wall -Wextra -Wpedantic)

add_executable(clang-tracer-server src/main.cpp src/server.cpp src/session.cpp src/injector.cpp src/admission.cpp)
target_include_directories(clang-tracer-server PRIVATE include ${LLVM_INCLUDE_DIRS})
target_link_libraries(clang-tracer-server PRIVATE tracer_grpc tracer_context_collector
  LLVM ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_options(clang-tracer-server PRIVATE -Wall -Wextra -Wpedantic)

add_executable(clang-tracer src/client.cpp src/debug_resolver.cpp src/header_context.cpp)
target_include_directories(clang-tracer PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(clang-tracer PRIVATE TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
target_link_libraries(clang-tracer PRIVATE tracer_grpc tracer_context_analysis clang-cpp LLVM Threads::Threads)
target_compile_options(clang-tracer PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/TracerInstall.cmake)

option(TRACER_BUILD_EXAMPLES "Build example target programs" ON)
if(TRACER_BUILD_EXAMPLES OR BUILD_TESTING)
add_executable(trace-target examples/target.cpp)
set_target_properties(trace-target PROPERTIES ENABLE_EXPORTS ON)
target_compile_options(trace-target PRIVATE -O0 -fno-omit-frame-pointer -fcf-protection=none)

add_executable(cpp-interceptors-target examples/cpp-interceptors/target.cpp)
target_compile_options(cpp-interceptors-target PRIVATE -g -O0 -fno-omit-frame-pointer
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

add_executable(trace-threaded-target examples/threaded_target.cpp)
add_library(trace-stack-b SHARED examples/shared_stack_b.cpp)
add_library(trace-stack-a SHARED examples/shared_stack_a.cpp)
target_link_libraries(trace-stack-a PRIVATE trace-stack-b)
foreach(stack_target trace-stack-a trace-stack-b)
  target_include_directories(${stack_target} PUBLIC examples)
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
  add_library(admission-leaf OBJECT tests/admission_leaf.cpp)
  target_compile_options(admission-leaf PRIVATE -O2 -fomit-frame-pointer -momit-leaf-frame-pointer
    -fasynchronous-unwind-tables -Wall -Wextra -Wpedantic)
  add_executable(admission-integration tests/admission_integration.cpp src/admission.cpp)
  target_include_directories(admission-integration PRIVATE src ${LLVM_INCLUDE_DIRS})
  target_link_libraries(admission-integration PRIVATE admission-leaf trace-stack-a trace-stack-b LLVM ${CMAKE_DL_LIBS})
  target_compile_options(admission-integration PRIVATE -O2 -fomit-frame-pointer
    -fno-optimize-sibling-calls -fasynchronous-unwind-tables -Wall -Wextra -Wpedantic)
  add_test(NAME native_admission_integration COMMAND admission-integration
    --expected-module $<TARGET_FILE:trace-stack-a>
    --expected-module $<TARGET_FILE:trace-stack-b>)
  set_tests_properties(native_admission_integration PROPERTIES TIMEOUT 30)
  add_executable(admission-frame-pointer-integration tests/admission_integration.cpp src/admission.cpp)
  target_include_directories(admission-frame-pointer-integration PRIVATE src ${LLVM_INCLUDE_DIRS})
  target_link_libraries(admission-frame-pointer-integration PRIVATE admission-leaf trace-stack-a trace-stack-b LLVM ${CMAKE_DL_LIBS})
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
  target_link_libraries(module-identity-integration PRIVATE LLVM ${CMAKE_DL_LIBS})
  target_compile_options(module-identity-integration PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME native_module_identity_integration COMMAND module-identity-integration
    --module $<TARGET_FILE:trace-stack-b>)
  set_tests_properties(native_module_identity_integration PROPERTIES TIMEOUT 30)
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
      --source ${CMAKE_CURRENT_SOURCE_DIR}/examples/target.cpp
      --callback ${CMAKE_CURRENT_SOURCE_DIR}/examples/trace_callback.cpp
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
endif()
