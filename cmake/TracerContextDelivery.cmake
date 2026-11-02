# gRPC's module-provider builds reference protobuf targets without finding
# them, while newer config providers also export upb targets. Prefer the full
# config export so a legacy FindProtobuf import cannot create a partial set.
find_package(Protobuf QUIET CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()
find_package(gRPC REQUIRED CONFIG)
set(_context_proto_dir ${CMAKE_CURRENT_BINARY_DIR}/context-generated)
file(MAKE_DIRECTORY ${_context_proto_dir})
add_custom_command(
  OUTPUT ${_context_proto_dir}/context_transport.pb.cc ${_context_proto_dir}/context_transport.pb.h
         ${_context_proto_dir}/context_transport.grpc.pb.cc ${_context_proto_dir}/context_transport.grpc.pb.h
  COMMAND protobuf::protoc
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${_context_proto_dir} --grpc_out=${_context_proto_dir}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       ${CMAKE_CURRENT_SOURCE_DIR}/proto/context_transport.proto
  DEPENDS proto/context_transport.proto protobuf::protoc gRPC::grpc_cpp_plugin
  VERBATIM)
add_library(tracer_context_transport_proto STATIC
  ${_context_proto_dir}/context_transport.pb.cc)
target_compile_features(tracer_context_transport_proto PUBLIC cxx_std_20)
set_target_properties(tracer_context_transport_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tracer_context_transport_proto PUBLIC ${_context_proto_dir})
target_link_libraries(tracer_context_transport_proto PUBLIC protobuf::libprotobuf)
add_library(tracer_context_delivery STATIC src/context_journal.cpp src/context_stream.cpp
  ${_context_proto_dir}/context_transport.grpc.pb.cc)
target_compile_features(tracer_context_delivery PUBLIC cxx_std_20)
set_target_properties(tracer_context_delivery PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(tracer_context_delivery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${_context_proto_dir})
target_link_libraries(tracer_context_delivery PUBLIC protobuf::libprotobuf gRPC::grpc++
  Threads::Threads tracer_context_engine tracer_context_transport_proto)
target_compile_options(tracer_context_delivery PRIVATE -Wall -Wextra -Wpedantic
  -fvisibility=hidden -fvisibility-inlines-hidden)
