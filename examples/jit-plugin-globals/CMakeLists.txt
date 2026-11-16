cmake_minimum_required(VERSION 3.20)
project(jit_plugin_globals LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

add_executable(jit_plugin_globals main.cpp)
target_include_directories(jit_plugin_globals SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(jit_plugin_globals PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/repl")
if(TARGET LLVM)
  target_link_libraries(jit_plugin_globals PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(llvm_libraries orcjit native)
  target_link_libraries(jit_plugin_globals PRIVATE ${llvm_libraries})
endif()

add_library(plugin_a OBJECT plugin_a.cpp)
add_library(plugin_b OBJECT plugin_b.cpp)
foreach(plugin IN ITEMS plugin_a plugin_b)
  target_include_directories(${plugin} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src/repl")
endforeach()
set_target_properties(plugin_a plugin_b PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_custom_target(run-jit-plugin-globals
  COMMAND $<TARGET_FILE:jit_plugin_globals>
          $<TARGET_OBJECTS:plugin_a> $<TARGET_OBJECTS:plugin_b>
  DEPENDS jit_plugin_globals plugin_a plugin_b
  VERBATIM
)
