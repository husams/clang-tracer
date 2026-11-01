# The engine can be tested on the development host without the Linux injector,
# LLVM, gRPC or generated protocol bindings. This does not enable native tracing
# on other platforms.
find_package(Threads REQUIRED)
add_library(tracer_context_engine STATIC src/context_engine.cpp src/callback_bridge.cpp src/bounded_proto.cpp
  src/context_capture.cpp
  src/context_package.cpp src/engine_storage.cpp src/context_storage_support.cpp src/context_execution.cpp
  src/context_scheduler.cpp src/context_pthread_detour.cpp)
target_compile_features(tracer_context_engine PUBLIC cxx_std_20)
set_target_properties(tracer_context_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tracer_context_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tracer_context_engine PUBLIC Threads::Threads)
target_compile_options(tracer_context_engine PRIVATE -Wall -Wextra -Wpedantic)
# Private support entry points and their inline helpers must not become ELF
# preemption points when this archive is linked into the injected agent.
target_compile_options(tracer_context_engine PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
if(BUILD_TESTING)
  file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/shared_context_nested.cpp _nested_source_hash)
  if(NOT _nested_source_hash STREQUAL "bd37dc43fb05e89cde09643b369937fe78dad38b3c6fe6ed40fad1037cbf9f5d")
    message(FATAL_ERROR "The original shared-context acceptance source has changed")
  endif()
  add_executable(context-engine-tests tests/context_engine_tests.cpp tests/engine_storage_checks.cpp
    tests/context_execution_checks.cpp tests/context_scheduler_checks.cpp
    tests/context_package_checks.cpp)
  target_link_libraries(context-engine-tests PRIVATE tracer_context_engine)
  target_compile_options(context-engine-tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME shared_context_engine COMMAND context-engine-tests)
  set_tests_properties(shared_context_engine PROPERTIES TIMEOUT 30)
  add_executable(generate-callback-fixture tests/generate_callback_fixture.cpp)
  target_link_libraries(generate-callback-fixture PRIVATE tracer_context_engine)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/callback_fixture.hpp
    COMMAND generate-callback-fixture ${CMAKE_CURRENT_BINARY_DIR}/callback_fixture.hpp
    DEPENDS generate-callback-fixture VERBATIM)
  add_executable(callback-lifecycle-tests tests/callback_lifecycle_tests.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/callback_fixture.hpp)
  target_compile_features(callback-lifecycle-tests PRIVATE cxx_std_20)
  target_include_directories(callback-lifecycle-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_options(callback-lifecycle-tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME generated_callback_lifecycle COMMAND callback-lifecycle-tests)
  set_tests_properties(generated_callback_lifecycle PROPERTIES TIMEOUT 30)
  add_executable(bounded-proto-tests tests/bounded_proto_tests.cpp)
  target_link_libraries(bounded-proto-tests PRIVATE tracer_context_engine)
  target_compile_options(bounded-proto-tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  add_test(NAME bounded_context_protobuf COMMAND bounded-proto-tests)
  set_tests_properties(bounded_context_protobuf PROPERTIES TIMEOUT 30)
endif()
