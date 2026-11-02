include(${CMAKE_CURRENT_LIST_DIR}/TracerContextDelivery.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/TracerContextStdlib.cmake)
add_library(tracer_context_analysis STATIC src/context_analysis.cpp src/context_lifecycle.cpp
  src/context_binding.cpp src/context_contract.cpp src/context_compile.cpp src/context_owner.cpp
  src/context_hooks.cpp src/context_schema.cpp src/context_receiver.cpp src/context_stdlib.cpp
  src/context_unwind.cpp src/context_support_effects.cpp src/context_registry.cpp
  src/context_facade.cpp src/context_runtime.cpp src/context_activation.cpp src/context_paths.cpp
  src/context_query.cpp src/runtime_identity.cpp)
set_target_properties(tracer_context_analysis PROPERTIES POSITION_INDEPENDENT_CODE ON)
# LLVM's default SDK profile may omit RTTI for its polymorphic base classes.
# The importer and memory-manager translation units derive from LLVM/Clang
# classes. Keep state/runtime RTTI enabled for std::any/exception contracts.
if(NOT LLVM_ENABLE_RTTI)
  set_property(SOURCE src/context_binding.cpp src/agent.cpp APPEND PROPERTY COMPILE_OPTIONS -fno-rtti)
endif()
target_compile_features(tracer_context_analysis PUBLIC cxx_std_20)
target_include_directories(tracer_context_analysis SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_include_directories(tracer_context_analysis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tracer_context_analysis PUBLIC clang-cpp LLVM tracer_context_engine
  tracer_context_delivery ${CMAKE_DL_LIBS})
target_compile_options(tracer_context_analysis PRIVATE -Wall -Wextra -Wpedantic
  -fvisibility=hidden -fvisibility-inlines-hidden)
add_library(tracer_context_collector STATIC src/context_collector.cpp)
set_target_properties(tracer_context_collector PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(tracer_context_collector PUBLIC cxx_std_20)
target_include_directories(tracer_context_collector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tracer_context_collector PUBLIC tracer_context_analysis
  tracer_context_delivery)
target_compile_options(tracer_context_collector PRIVATE -Wall -Wextra -Wpedantic
  -fvisibility=hidden -fvisibility-inlines-hidden)
string(JOIN "|" _context_physical_roots ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
include(GNUInstallDirs)
file(RELATIVE_PATH _context_header_relative "${CMAKE_INSTALL_FULL_LIBDIR}"
  "${CMAKE_INSTALL_FULL_DATADIR}/clang-tracer/context")
target_compile_definitions(tracer_context_analysis PRIVATE
  TRACER_CONTEXT_SYSTEM_ROOTS="${_context_physical_roots}"
  TRACER_CONTEXT_ENGINE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src"
  TRACER_CONTEXT_HEADER_RELATIVE="${_context_header_relative}")
if(TARGET tracer_context_stdlib)
  add_dependencies(tracer_context_analysis tracer_context_stdlib)
  target_compile_definitions(tracer_context_analysis PRIVATE
    TRACER_CONTEXT_STDLIB_PATH="$<TARGET_FILE:tracer_context_stdlib>"
    TRACER_CONTEXT_ARCHIVE_SHA256="${_context_stdlib_archive_sha256}")
endif()
if(BUILD_TESTING)
  add_executable(context-analysis-tests tests/context_analysis_tests.cpp tests/context_binding_checks.cpp
    tests/context_owner_checks.cpp tests/context_hooks_checks.cpp tests/context_facade_checks.cpp
    tests/context_runtime_checks.cpp tests/context_schema_checks.cpp
    tests/context_activation_checks.cpp
    tests/context_delivery_checks.cpp tests/context_collector_checks.cpp
    tests/context_capture_checks.cpp tests/context_registry_checks.cpp
    tests/context_support_effects_checks.cpp tests/context_stdlib_checks.cpp
    tests/context_generic_lineage_checks.cpp)
  target_compile_options(context-analysis-tests PRIVATE -UNDEBUG)
  target_link_libraries(context-analysis-tests PRIVATE tracer_context_analysis
    tracer_context_collector)
  target_include_directories(context-analysis-tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_include_directories(context-analysis-tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  set(_context_system_arguments "  \"-nostdinc++\",\n")
  foreach(_include IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    string(APPEND _context_system_arguments "  \"-isystem\", \"${_include}\",\n")
  endforeach()
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/context_owner_generated.hpp
    COMMAND generate-context-lifecycle
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/context_owner.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/context_owner_generated.hpp.tmp owner
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_BINARY_DIR}/context_owner_generated.hpp.tmp
      ${CMAKE_CURRENT_BINARY_DIR}/context_owner_generated.hpp
    DEPENDS generate-context-lifecycle tests/fixtures/context_owner.cpp VERBATIM)
  target_sources(context-analysis-tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/context_owner_generated.hpp)
  configure_file(tests/context_test_config.hpp.in context_test_config.hpp @ONLY)
  add_executable(generate-context-lifecycle tests/generate_context_lifecycle.cpp)
  target_link_libraries(generate-context-lifecycle PRIVATE tracer_context_analysis)
  target_include_directories(generate-context-lifecycle SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
  target_include_directories(generate-context-lifecycle PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(generate-context-lifecycle PRIVATE
    TRACER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    TRACER_TEST_RESOURCE_DIR="${_tracer_resource_dir}")
  foreach(_fixture shared_context_nested context_lifecycle)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_fixture}_generated.hpp
      COMMAND generate-context-lifecycle
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${_fixture}.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${_fixture}_generated.hpp.tmp
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/${_fixture}_generated.hpp.tmp
        ${CMAKE_CURRENT_BINARY_DIR}/${_fixture}_generated.hpp
      DEPENDS generate-context-lifecycle tests/fixtures/${_fixture}.cpp
      VERBATIM)
    target_sources(context-analysis-tests PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/${_fixture}_generated.hpp)
  endforeach()
  target_link_libraries(context-analysis-tests PRIVATE tracer_context_engine)
  target_include_directories(context-analysis-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_definitions(context-analysis-tests PRIVATE
    TRACER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    TRACER_TEST_RESOURCE_DIR="${_tracer_resource_dir}")
  add_executable(context-delivery-tests tests/context_delivery_test_main.cpp
    tests/context_delivery_checks.cpp)
  target_compile_options(context-delivery-tests PRIVATE -UNDEBUG)
  target_include_directories(context-delivery-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(context-delivery-tests PRIVATE tracer_context_analysis
    tracer_context_collector)
  add_test(NAME shared_context_declarations COMMAND context-analysis-tests)
  set_tests_properties(shared_context_declarations PROPERTIES TIMEOUT 180)
  include(${CMAKE_CURRENT_LIST_DIR}/TracerContextSchemaTests.cmake)
endif()
