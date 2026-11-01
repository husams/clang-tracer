if(NOT BUILD_TESTING OR NOT TARGET context-analysis-tests)
  return()
endif()

if(NOT TARGET protobuf::libprotobuf)
  find_package(Protobuf REQUIRED)
endif()

add_executable(generate-context-schema tests/generate_context_schema.cpp)
target_link_libraries(generate-context-schema PRIVATE tracer_context_analysis
  protobuf::libprotobuf)
target_include_directories(generate-context-schema SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_include_directories(generate-context-schema PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(generate-context-schema PRIVATE
  TRACER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  TRACER_TEST_RESOURCE_DIR="${_tracer_resource_dir}")

set(_context_schema_generated
  ${CMAKE_CURRENT_BINARY_DIR}/context_schema_generated.hpp)
add_custom_command(OUTPUT ${_context_schema_generated}
  COMMAND generate-context-schema ${_context_schema_generated}.tmp
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${_context_schema_generated}.tmp ${_context_schema_generated}
  DEPENDS generate-context-schema
    tests/fixtures/shared_context_nested.cpp
    tests/fixtures/context_schema_catalog.cpp
    tests/fixtures/context_schema_history_v1.cpp
    tests/fixtures/context_schema_history_v2.cpp
    tests/fixtures/context_schema_query.cpp
  VERBATIM)
target_sources(context-analysis-tests PRIVATE
  tests/context_schema_runtime.cpp ${_context_schema_generated})
target_include_directories(context-analysis-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(context-analysis-tests PRIVATE protobuf::libprotobuf)

add_executable(context-query-tests tests/context_query_test_main.cpp
  tests/context_schema_runtime.cpp ${_context_schema_generated})
target_include_directories(context-query-tests PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
  ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(context-query-tests PRIVATE tracer_context_analysis
  tracer_context_engine protobuf::libprotobuf)
