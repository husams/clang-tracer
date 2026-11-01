#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include <google/protobuf/descriptor.pb.h>
#include <grpcpp/grpcpp.h>

#include "context_collector.hpp"

namespace {
struct RpcServer {
  std::unique_ptr<grpc::Server> server;
  std::shared_ptr<grpc::Channel> channel;
};

RpcServer start_rpc_server(grpc::Service &service) {
  grpc::ServerBuilder network_builder;
  int selected_port = 0;
  network_builder.AddListeningPort(
      "127.0.0.1:0", grpc::InsecureServerCredentials(), &selected_port);
  network_builder.RegisterService(&service);
  auto server = network_builder.BuildAndStart();
  if (server) {
    return {std::move(server),
            grpc::CreateChannel("127.0.0.1:" + std::to_string(selected_port),
                                grpc::InsecureChannelCredentials())};
  }
  grpc::ServerBuilder in_process_builder;
  in_process_builder.RegisterService(&service);
  server = in_process_builder.BuildAndStart();
  assert(server);
  auto channel = server->InProcessChannel(grpc::ChannelArguments{});
  return {std::move(server), std::move(channel)};
}
} // namespace

void verify_context_collector() {
  using namespace std::chrono_literals;
  using namespace clangtracer::context::v1;
  std::mutex mutex;
  std::condition_variable cv;
  std::shared_ptr<const tracer::context_engine::DecodedEvent> received;
  ContextEventEnvelope received_envelope;

  tracer::context_delivery::ContextCollector collector(
      {}, {},
      [&](const ExportOpen &, const ContextEventEnvelope &envelope,
          std::shared_ptr<const tracer::context_engine::DecodedEvent> decoded) {
        std::lock_guard lock(mutex);
        received_envelope = envelope;
        received = std::move(decoded);
        cv.notify_all();
        return true;
      });
  auto rpc = start_rpc_server(*collector.service());
  auto &server = rpc.server;
  auto channel = rpc.channel;

  google::protobuf::FileDescriptorSet descriptors;
  auto *file = descriptors.add_file();
  file->set_name("collector_fixture.proto");
  file->set_package("collector_fixture");
  file->set_syntax("proto3");
  auto *element = file->add_message_type();
  element->set_name("Element");
  auto *items = element->add_field();
  items->set_name("items");
  items->set_number(2);
  items->set_label(google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
  items->set_type(google::protobuf::FieldDescriptorProto::TYPE_INT32);
  auto *message = file->add_message_type();
  message->set_name("Snapshot");
  auto *field = message->add_field();
  field->set_name("elements");
  field->set_number(1);
  field->set_label(google::protobuf::FieldDescriptorProto::LABEL_REPEATED);
  field->set_type(google::protobuf::FieldDescriptorProto::TYPE_MESSAGE);
  field->set_type_name(".collector_fixture.Element");
  std::string serialized_descriptors;
  assert(descriptors.SerializeToString(&serialized_descriptors));
  std::string lineage(16, '\0');
  lineage[0] = 'L';
  const std::string state_type = "collector_fixture::State";
  const std::string scope_type = "collector_fixture::Scope";
  const std::string role1 = "1=vector";
  const std::string role2 = "1.2=vector";
  tracer::context_engine::StateFamilyKey family;
  std::copy(lineage.begin(), lineage.end(), family.source_lineage_id.begin());
  family.cpp_state_type = state_type;
  family.cpp_scope_type = scope_type;
  const std::array<std::string, 2> roles{role2, role1};
  const auto revision = tracer::context_engine::compute_schema_revision_id(
      serialized_descriptors, family, "collector_fixture.Snapshot", roles);

  ExportOpen open;
  open.set_protocol_version(1);
  open.set_epoch_id(1);
  open.set_producer_session_id("collector-test-session");
  open.set_profile(BEST_EFFORT);
  tracer::context_delivery::ContextExportClient client(channel, open);
  assert(client.start());

  ContextBatch batch;
  auto *schema = batch.add_schemas();
  schema->set_schema_identity(revision);
  schema->set_revision_id(revision);
  schema->set_root_type("collector_fixture.Snapshot");
  schema->set_serialized_file_descriptor_set(serialized_descriptors);
  schema->set_source_lineage_id(lineage);
  schema->set_cpp_state_type(state_type);
  schema->set_cpp_scope_type(scope_type);
  schema->add_container_roles(role2);
  schema->add_container_roles(role1);
  auto *event = batch.mutable_event();
  event->set_event_id("collector-event");
  event->set_name("snapshot");
  event->set_effective_max_items(100);
  event->set_invocation_id("invocation-1");
  event->set_probe_id("probe-1");
  auto *state = event->add_states();
  state->set_schema_identity(revision);
  state->set_payload("");
  auto *truncation = state->add_truncations();
  truncation->add_path()->set_field_number(1);
  truncation->add_path()->set_list_index(0);
  truncation->add_path()->set_field_number(2);
  truncation->set_total_items(2);
  truncation->set_captured_items(1);

  const auto encoded = batch.SerializeAsString();
  assert(client.hook_ring().try_publish(
      std::as_bytes(std::span(encoded.data(), encoded.size()))));
  {
    std::unique_lock lock(mutex);
    assert(cv.wait_for(lock, 5s, [&] { return static_cast<bool>(received); }));
  }
  client.stop();
  server->Shutdown();
  server->Wait();

  assert(received->states.size() == 1);
  assert(received->states.front().message() != nullptr);
  assert(received->states.front().schema() != nullptr);
  assert(received->states.front().schema()->family.cpp_state_type ==
         "collector_fixture::State");
  assert(received->states.front().schema()->container_roles.size() == 2);
  assert(received->states.front().truncations().size() == 1);
  assert(received->states.front().truncations().front().path.size() == 3);
  assert(received->states.front().truncations().front().path.front().selector ==
         tracer::context_engine::CapturePathSelector::field_number);
  assert(received->states.front().truncations().front().path[1].selector ==
         tracer::context_engine::CapturePathSelector::list_index);
  assert(received->states.front().truncations().front().path[2].selector ==
         tracer::context_engine::CapturePathSelector::field_number);
  assert(received_envelope.event().effective_max_items() == 100);
  assert(received_envelope.event().invocation_id() == "invocation-1");
  assert(received_envelope.event().probe_id() == "probe-1");
  assert(received_envelope.producer_session_id() == "collector-test-session");
  assert(received_envelope.epoch_id() == 1);
  assert(received_envelope.schemas_size() == 1);
}
