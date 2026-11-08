option(TRACER_BUNDLE_CLANG_HEADERS "Install selected Clang builtin headers with the runtime" ON)
include("${CMAKE_CURRENT_LIST_DIR}/TracerJitConfig.cmake")
set(_tracer_resource_destination "${CMAKE_INSTALL_DATADIR}/clang-tracer/clang-resource")
if(IS_ABSOLUTE "${_tracer_resource_destination}")
  set(_tracer_full_resource_destination "${_tracer_resource_destination}")
else()
  set(_tracer_full_resource_destination "${CMAKE_INSTALL_PREFIX}/${_tracer_resource_destination}")
endif()
foreach(_target clang-tracer clang-tracer-server clang_tracer_agent)
  target_sources(${_target} PRIVATE src/runtime_paths.cpp)
  target_include_directories(${_target} PRIVATE "${PROJECT_BINARY_DIR}/generated")
  if(_target STREQUAL "clang_tracer_agent")
    set(_origin "${CMAKE_INSTALL_FULL_LIBDIR}")
    target_compile_definitions(${_target} PRIVATE TRACER_RUNTIME_IS_LIBRARY=1)
  else()
    set(_origin "${CMAKE_INSTALL_FULL_BINDIR}")
  endif()
  file(RELATIVE_PATH _resource_relative "${_origin}" "${_tracer_full_resource_destination}")
  file(RELATIVE_PATH _library_relative "${_origin}" "${CMAKE_INSTALL_FULL_LIBDIR}")
  set(_agent_location "${_library_relative}/libclang_tracer_agent.so")
  if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(_agent_location "${CMAKE_INSTALL_LIBDIR}/libclang_tracer_agent.so")
  endif()
  if(IS_ABSOLUTE "${_tracer_resource_destination}")
    set(_resource_relative "${_tracer_resource_destination}")
  endif()
  target_compile_definitions(${_target} PRIVATE
    TRACER_RESOURCE_RELATIVE_DIR="${_resource_relative}"
    TRACER_AGENT_RELATIVE_PATH="${_agent_location}"
    TRACER_BUILD_CLANG_RESOURCE_DIR="${_tracer_resource_dir}")
  set_property(TARGET ${_target} APPEND PROPERTY INSTALL_RPATH "$ORIGIN/${_library_relative}")
  set_property(TARGET ${_target} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
  target_link_libraries(${_target} PRIVATE ${CMAKE_DL_LIBS})
endforeach()
install(TARGETS clang-tracer clang-tracer-server clang_tracer_agent
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Runtime)
if(TARGET tracer_context_stdlib)
  install(TARGETS tracer_context_stdlib
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Runtime)
endif()
install(FILES src/context_facade.hpp src/context_analysis.hpp src/context_execution.hpp
  src/context_engine.hpp src/context_codec.hpp src/context_schema.hpp
  src/bounded_proto.hpp src/bounded_proto.cpp
  DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/context" COMPONENT Runtime)
install(DIRECTORY include/tracer
  DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/context" COMPONENT Runtime)
install(FILES proto/context_transport.proto
  DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/proto" COMPONENT Development)
install(DIRECTORY include/tracer DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT Development)
install(FILES proto/tracer.proto DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/proto" COMPONENT Development)
install(FILES LICENSE README.md DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT Runtime)
install(FILES python/README.md DESTINATION "${CMAKE_INSTALL_DOCDIR}/python" COMPONENT Runtime)
install(DIRECTORY docs DESTINATION "${CMAKE_INSTALL_DOCDIR}" COMPONENT Runtime)
install(DIRECTORY examples DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer"
  COMPONENT Development FILES_MATCHING
  PATTERN "*.cpp" PATTERN "*.hpp" PATTERN "*.md" PATTERN "*.py" PATTERN "*.repl"
  PATTERN "__pycache__" EXCLUDE)
foreach(_example native-repl-values native-repl-jit)
  install(PROGRAMS examples/${_example}/run.sh
    DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/examples/${_example}"
    COMPONENT Development)
endforeach()
install(FILES LICENSES/LLVM.txt DESTINATION "${CMAKE_INSTALL_DOCDIR}/licenses" COMPONENT Runtime)
if(TRACER_BUNDLE_CLANG_HEADERS)
  install(DIRECTORY "${_tracer_resource_dir}/include"
    DESTINATION "${_tracer_resource_destination}" COMPONENT Runtime)
endif()
set(CPACK_PACKAGE_NAME "clang-tracer")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_CONTACT "https://github.com/husams/clang-tracer")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0 WITH LLVM-exception")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native C++ tracing with Clang/LLVM")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_GENERATOR "TGZ")
include(CPack)
