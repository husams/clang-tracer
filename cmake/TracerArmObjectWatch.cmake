include(GNUInstallDirs)
include(${CMAKE_CURRENT_LIST_DIR}/TracerToolchain.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/TracerJitConfig.cmake)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED CONFIG)
find_package(Threads REQUIRED)

set(object_proto_dir "${CMAKE_CURRENT_BINARY_DIR}/object-generated")
file(MAKE_DIRECTORY "${object_proto_dir}")
set(object_proto_files tracer context_transport)
set(object_proto_sources)
foreach(proto IN LISTS object_proto_files)
  list(APPEND object_proto_sources
    "${object_proto_dir}/${proto}.pb.cc" "${object_proto_dir}/${proto}.pb.h")
endforeach()
list(APPEND object_proto_sources "${object_proto_dir}/tracer.grpc.pb.cc"
                                "${object_proto_dir}/tracer.grpc.pb.h")
add_custom_command(OUTPUT ${object_proto_sources}
  COMMAND protobuf::protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
    --cpp_out=${object_proto_dir} --grpc_out=${object_proto_dir}
    --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin> tracer.proto
  COMMAND protobuf::protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
    --cpp_out=${object_proto_dir} context_transport.proto
  DEPENDS proto/tracer.proto proto/context_transport.proto
  VERBATIM)
add_library(tracer_arm_object_rpc STATIC ${object_proto_sources})
set_target_properties(tracer_arm_object_rpc PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tracer_arm_object_rpc PUBLIC "${object_proto_dir}")
target_link_libraries(tracer_arm_object_rpc PUBLIC gRPC::grpc++ protobuf::libprotobuf)
add_library(tracer_arm_object_watch STATIC
  src/arm_object_watch.cpp src/object_watch.cpp src/object_watch_plan.cpp src/object_launch.cpp src/object_snapshot.cpp
  src/native_entry.cpp src/native_detour.cpp src/debug_resolver.cpp src/object_code_cache.cpp
  src/injector.cpp src/runtime_identity.cpp)
target_include_directories(tracer_arm_object_watch PUBLIC include)
target_include_directories(tracer_arm_object_watch SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(tracer_arm_object_watch PUBLIC tracer_arm_object_rpc
  PRIVATE LLVM Threads::Threads ${CMAKE_DL_LIBS})
target_compile_options(tracer_arm_object_watch PRIVATE -Wall -Wextra)
add_executable(clang-tracer-object-watch src/object_watch_main.cpp)
target_link_libraries(clang-tracer-object-watch PRIVATE tracer_arm_object_watch)
add_executable(clang-tracer-server src/arm_object_server.cpp)
target_link_libraries(clang-tracer-server PRIVATE tracer_arm_object_watch)
target_sources(clang-tracer-server PRIVATE src/arm_method_watch.cpp)
file(RELATIVE_PATH arm_agent_relative "${CMAKE_INSTALL_FULL_BINDIR}"
  "${CMAKE_INSTALL_FULL_LIBDIR}/libclang_tracer_object_cache_arm.so")
target_compile_definitions(clang-tracer-server PRIVATE
  TRACER_ARM_AGENT_RELATIVE_PATH="${arm_agent_relative}")
add_library(clang_tracer_object_cache_arm SHARED src/object_code_cache_agent.cpp src/native_stack.cpp
  src/native_entry.cpp src/arm_snapshot_codec.cpp src/context_paths.cpp src/context_schema.cpp
  src/value_snapshot_codegen.cpp src/value_snapshot_storage.cpp src/snapshot_encoder.cpp)
target_include_directories(clang_tracer_object_cache_arm PRIVATE include "${PROJECT_BINARY_DIR}/generated")
target_include_directories(clang_tracer_object_cache_arm SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
file(RELATIVE_PATH arm_context_headers "${CMAKE_INSTALL_FULL_LIBDIR}"
  "${CMAKE_INSTALL_FULL_DATADIR}/clang-tracer/context")
string(JOIN "|" arm_context_system_roots ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
target_compile_definitions(clang_tracer_object_cache_arm PRIVATE
  TRACER_CLANG_RESOURCE_DIR="${_tracer_resource_dir}"
  TRACER_CONTEXT_ENGINE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src"
  TRACER_CONTEXT_SYSTEM_ROOTS="${arm_context_system_roots}"
  TRACER_CONTEXT_HEADER_RELATIVE="${arm_context_headers}")
target_link_libraries(clang_tracer_object_cache_arm PRIVATE clang-cpp LLVM tracer_arm_object_rpc
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_options(clang_tracer_object_cache_arm PRIVATE -Wall -Wextra)
target_link_options(clang_tracer_object_cache_arm PRIVATE
  "LINKER:-Bsymbolic,--exclude-libs,ALL,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/arm_object_agent_exports.map")
set_property(TARGET clang_tracer_object_cache_arm APPEND PROPERTY LINK_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/arm_object_agent_exports.map")
install(TARGETS clang-tracer-object-watch clang-tracer-server clang_tracer_object_cache_arm
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES src/context_facade.hpp src/context_codec.hpp src/context_schema.hpp
  src/bounded_proto.hpp src/bounded_proto.cpp
  DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/context")
install(DIRECTORY include/tracer DESTINATION "${CMAKE_INSTALL_DATADIR}/clang-tracer/include")
