if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  foreach(scenario IN ITEMS startup syscall)
    add_executable(object-watch-${scenario}-fixture "tests/fixtures/object_${scenario}/fixture.cpp")
    target_compile_options(object-watch-${scenario}-fixture PRIVATE -g -O0 -fno-inline -fno-omit-frame-pointer)
    add_test(NAME native_object_watch_${scenario}
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/object_watch_${scenario}.py"
        $<TARGET_FILE:clang-tracer-object-watch> $<TARGET_FILE:object-watch-${scenario}-fixture>)
    set_tests_properties(native_object_watch_${scenario} PROPERTIES TIMEOUT 90)
  endforeach()
  if(TARGET clang_tracer_object_cache_arm)
    set(_startup_agent clang_tracer_object_cache_arm)
  else()
    set(_startup_agent clang_tracer_agent)
  endif()
  add_test(NAME native_object_watch_startup_code_cache
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/object_watch_startup.py"
      $<TARGET_FILE:clang-tracer-object-watch> $<TARGET_FILE:object-watch-startup-fixture>
      --agent $<TARGET_FILE:${_startup_agent}>)
  set_tests_properties(native_object_watch_startup_code_cache PROPERTIES TIMEOUT 90)
  add_executable(object-watch-preinit-fixture tests/fixtures/object_startup/fixture.cpp)
  target_compile_options(object-watch-preinit-fixture PRIVATE -g -O0 -fno-inline -fno-omit-frame-pointer)
  target_compile_definitions(object-watch-preinit-fixture PRIVATE TRACE_TEST_PREINIT)
  add_test(NAME native_object_watch_preinit_code_cache
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/object_watch_startup.py"
      $<TARGET_FILE:clang-tracer-object-watch> $<TARGET_FILE:object-watch-preinit-fixture>
      --agent $<TARGET_FILE:${_startup_agent}> --loader-ready)
  set_tests_properties(native_object_watch_preinit_code_cache PROPERTIES TIMEOUT 90)
  add_library(object-watch-startup-library SHARED tests/fixtures/object_startup/fixture.cpp)
  target_compile_options(object-watch-startup-library PRIVATE -g -O0 -fno-inline -fno-omit-frame-pointer)
  target_link_options(object-watch-startup-library PRIVATE "LINKER:--build-id=sha1")
  target_compile_definitions(object-watch-startup-library PRIVATE TRACE_TEST_STARTUP_LIBRARY)
  add_executable(object-watch-loader-fixture tests/fixtures/object_startup/loader_driver.cpp)
  target_link_libraries(object-watch-loader-fixture PRIVATE object-watch-startup-library)
  add_test(NAME native_object_watch_loader_dso_code_cache
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/object_watch_startup.py"
      $<TARGET_FILE:clang-tracer-object-watch> $<TARGET_FILE:object-watch-loader-fixture>
      --agent $<TARGET_FILE:${_startup_agent}> --loader-ready
      --module $<TARGET_FILE:object-watch-startup-library>)
  set_tests_properties(native_object_watch_loader_dso_code_cache PROPERTIES TIMEOUT 90)
endif()
