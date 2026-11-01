#include "context_journal.hpp"
#include "context_stream.hpp"
#include "context_capture.hpp"
#include "context_schema.hpp"
#include "context_transport.pb.h"

#include <grpcpp/grpcpp.h>

#include <cassert>
#include <array>
#include <filesystem>
#include <fstream>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
std::filesystem::path temp_path() {
  return std::filesystem::temp_directory_path() /
      ("clang-tracer-delivery-" + std::to_string(::getpid()));
}

struct RpcServer {
  std::unique_ptr<grpc::Server> server;
  std::shared_ptr<grpc::Channel> channel;
};

bool encode_delivery_state(const void *, tracer::context_engine::BoundedProtoWriter &writer,
                           const void *payload,
                           tracer::context_engine::EncodeBudget &) noexcept {
  return payload && writer.s64(1, *static_cast<const std::int64_t *>(payload));
}

bool publish_capture_to_client(
    void *opaque, std::span<const std::byte> event_record,
    const std::shared_ptr<const void> &schema_lease) noexcept {
  return static_cast<tracer::context_delivery::ContextExportClient *>(opaque)
      ->try_publish_capture(event_record, schema_lease);
}

class ReconnectService final : public clangtracer::context::v1::ContextTransport::Service {
public:
  grpc::Status Export(grpc::ServerContext *context,
      grpc::ServerReaderWriter<clangtracer::context::v1::CollectorControl,
                               clangtracer::context::v1::ProducerPacket> *stream) override {
    using namespace clangtracer::context::v1;
    ProducerPacket packet;
    if (!stream->Read(&packet) || !packet.has_open())
      return {grpc::StatusCode::INVALID_ARGUMENT, "open required"};
    const auto open = packet.open();
    const auto attempt = attempts.fetch_add(1, std::memory_order_relaxed);
    CollectorControl control;
    control.set_epoch_id(open.epoch_id());
    if (attempt == 0) {
      control.mutable_flow()->set_paused(true);
      if (!stream->Write(control))
        return {grpc::StatusCode::UNAVAILABLE, "disconnect before data"};
      control.Clear();
      control.set_epoch_id(open.epoch_id());
      set_ready(control.mutable_ready(), open.profile());
      (void)stream->Write(control);
      return {grpc::StatusCode::UNAVAILABLE, "force a fresh stream epoch"};
    }
    set_ready(control.mutable_ready(), open.profile());
    if (!stream->Write(control))
      return {grpc::StatusCode::UNAVAILABLE, "ready write failed"};
    while (stream->Read(&packet)) {
      if (!packet.has_data() || !packet.data().has_event()) continue;
      const auto &event = packet.data().event();
      std::lock_guard lock(mutex);
      received_epoch = open.epoch_id();
      received_schema_id = event.states().empty()
          ? 0 : event.states(0).epoch_schema_id();
      received = true;
      cv.notify_all();
      return grpc::Status::OK;
    }
    (void)context;
    return {grpc::StatusCode::UNAVAILABLE, "stream ended before event"};
  }

  void wait_for_event() {
    std::unique_lock lock(mutex);
    assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return received; }));
    assert(received_epoch == 2 && received_schema_id == 1);
  }

  std::atomic<unsigned> attempts{0};
private:
  static void set_ready(clangtracer::context::v1::StreamReady *ready,
                        clangtracer::context::v1::DeliveryProfile profile) {
    ready->set_protocol_version(1);
    ready->set_max_packet_bytes(2 * 1024 * 1024);
    ready->set_max_event_bytes(256 * 1024);
    ready->set_max_epoch_schemas(256);
    ready->set_max_epoch_schema_bytes(16 * 1024 * 1024);
    ready->set_accepted_profile(profile);
  }
  std::mutex mutex;
  std::condition_variable cv;
  bool received = false;
  std::uint64_t received_epoch = 0, received_schema_id = 0;
};

RpcServer start_rpc_server(grpc::Service &service) {
  grpc::ServerBuilder network_builder;
  int selected_port = 0;
  network_builder.AddListeningPort("127.0.0.1:0",
                                   grpc::InsecureServerCredentials(),
                                   &selected_port);
  network_builder.RegisterService(&service);
  auto server = network_builder.BuildAndStart();
  if (server) {
    return {std::move(server), grpc::CreateChannel(
        "127.0.0.1:" + std::to_string(selected_port),
        grpc::InsecureChannelCredentials())};
  }
  // Sandboxed macOS runners deny bind(2); retain an RPC-level fallback there.
  grpc::ServerBuilder in_process_builder;
  in_process_builder.RegisterService(&service);
  server = in_process_builder.BuildAndStart();
  assert(server);
  auto channel = server->InProcessChannel(grpc::ChannelArguments{});
  return {std::move(server), std::move(channel)};
}
}

void verify_context_delivery() {
  using tracer::context_delivery::ContextJournal;
  using namespace clangtracer::context::v1;
  const auto dir = temp_path();
  std::filesystem::remove_all(dir);
  std::uint64_t p1 = 0, p2 = 0;
  {
    ContextJournal journal(dir);
    assert(journal.healthy());
    assert(journal.begin_create("recording-A", "sink-A"));
    assert(!journal.state().creation_confirmed);
    // A failed checkpoint write is represented by invalid advancement; no
    // checkpoint or record can be reclaimed on this path.
    assert(!journal.save_ack("recording-A", 1));
    ContextBatch first;
    auto *first_schema = first.add_schemas();
    first_schema->set_schema_identity("schema-rev-1");
    first_schema->set_root_type("demo.Request");
    first_schema->set_serialized_file_descriptor_set("opaque-fds");
    first.mutable_event()->set_event_id("event-before");
    first.mutable_event()->set_name("request");
    assert(journal.append(first, p1) && p1 == 1);
    assert(journal.read_after(0, 10, 4096).empty()); // Offline data waits for CREATE confirmation.
    assert(journal.confirm_create("recording-A", 0));
    assert(journal.save_ack("recording-A", 1));
    assert(!journal.save_ack("recording-A", 0)); // Monotonic ACK.
    ContextBatch second;
    *second.add_schemas() = *first_schema;
    auto *event = second.mutable_event();
    event->set_event_id("event-stable");
    event->set_name("request");
    event->set_recording_position(2);
    auto *state = event->add_states();
    state->set_scope_id(9); state->set_schema_identity("schema-rev-1"); state->set_payload("original-bytes");
    assert(journal.append(second, p2) && p2 == 2);
  }
  {
    ContextJournal recovered(dir);
    assert(recovered.healthy());
    assert(recovered.state().creation_confirmed);
    assert(recovered.state().acknowledged == 1);
    const auto replay = recovered.read_after(recovered.state().acknowledged, 10, 4096);
    assert(replay.size() == 1 && replay.front().position == 2);
    ContextBatch decoded;
    assert(decoded.ParseFromString(replay.front().serialized_frame));
    assert(decoded.schemas_size() == 1 && decoded.schemas(0).serialized_file_descriptor_set() == "opaque-fds");
    assert(decoded.event().event_id() == "event-stable");
    assert(decoded.event().states(0).payload() == "original-bytes");
    // Persisted event bytes remain unchanged and caller-visible positions stay stable.
    assert(!recovered.save_ack("wrong-recording", 2));
    assert(recovered.save_ack("recording-A", 2));
  }
  {
    ContextJournal recovered(dir);
    assert(recovered.healthy() && recovered.state().acknowledged == 2);
    assert(recovered.read_after(2, 8, 1024).empty());
  }
  // Simulate a crash during a record append: recovery removes only the invalid
  // tail and preserves the previously committed prefix and checkpoint.
  {
    std::ofstream tail(dir / "segment-00000000.bin", std::ios::binary | std::ios::app);
    tail.write("partial", 7);
  }
  {
    ContextJournal recovered(dir);
    assert(recovered.healthy());
    assert(recovered.committed_prefix() == 2);
    assert(recovered.state().acknowledged == 2);
  }
  std::filesystem::remove_all(dir);

  const auto pressure_dir = dir.string() + "-pressure";
  std::filesystem::remove_all(pressure_dir);
  ContextFrame small;
  small.mutable_event()->set_name("bounded");

  const auto collector_dir = dir.string() + "-collector";
  std::filesystem::remove_all(collector_dir);

  const auto segment_dir = dir.string() + "-segments";
  std::filesystem::remove_all(segment_dir);
  {
    ContextJournal segmented(segment_dir, {4096, 40, {}});
    assert(segmented.begin_create("recording-C", "sink-C"));
    std::uint64_t p = 0;
    assert(segmented.append(small, p) && p == 1);
    assert(segmented.append(small, p) && p == 2);
    assert(segmented.append(small, p) && p == 3);
    assert(segmented.confirm_create("recording-C", 0));
    const auto charged = segmented.data_bytes();
    assert(segmented.save_ack("recording-C", 1));
    assert(segmented.data_bytes() < charged); // ACK persisted before complete-segment reclaim.
  }
  {
    ContextJournal segmented(segment_dir, {4096, 40, {}});
    assert(segmented.healthy());
    assert(segmented.state().acknowledged == 1);
    const auto replay = segmented.read_after(1, 8, 2048);
    assert(replay.size() == 2 && replay[0].position == 2 && replay[1].position == 3);
    assert(segmented.save_ack("recording-C", 3));
    assert(segmented.data_bytes() == 0);
    std::uint64_t next = 0;
    assert(segmented.append(small, next) && next == 4);
  }
  std::filesystem::remove_all(segment_dir);

  const auto fault_dir = dir.string() + "-fault";
  std::filesystem::remove_all(fault_dir);

  const auto global_a = dir.string() + "-global-a";
  const auto global_b = dir.string() + "-global-b";
  auto shared_budget = std::make_shared<tracer::context_delivery::JournalBudget>(60);
  {
    ContextJournal first(global_a, {1000, 1024, {}}, shared_budget);
    ContextJournal second(global_b, {1000, 1024, {}}, shared_budget);
    assert(first.begin_create("global-a", "sink"));
    assert(second.begin_create("global-b", "sink"));
    std::uint64_t position = 0;
    assert(first.append(small, position));
    assert(shared_budget->used() <= 60);
    assert(!second.append(small, position)); // Shared cross-sink data credits.
  }
  std::filesystem::remove_all(global_a);
  std::filesystem::remove_all(global_b);

  const auto collector_fault_dir = dir.string() + "-collector-fault";
  std::filesystem::remove_all(collector_fault_dir);
  ContextFrame collector_event;
  collector_event.mutable_event()->set_event_id("collector-fault-event");
  collector_event.mutable_event()->set_recording_position(1);
  int collector_checkpoints = 0;
  {
    tracer::context_delivery::ContextCollectorStore failing(collector_fault_dir,
        {4096, 1024, [&](std::string_view op) {
          return op == "checkpoint" && ++collector_checkpoints == 2;
        }});
    assert(failing.open(true, "recording-E", "sink-E", 0).accepted);
    assert(!failing.persist(collector_event, 1).accepted);
    assert(failing.durable_prefix() == 0); // No ACK on a failed checkpoint save.
  }
  {
    tracer::context_delivery::ContextCollectorStore recovered(collector_fault_dir);
    auto resumed = recovered.open(false, "recording-E", "sink-E", 0);
    assert(resumed.accepted && resumed.durable_prefix == 1); // Recovery finishes the committed transaction.
    assert(recovered.persist(collector_event, 1).accepted);
  }
  std::filesystem::remove_all(collector_fault_dir);
  int checkpoints = 0;
  {
    ContextJournal failing(fault_dir, {4096, 1024, [&](std::string_view op) {
      return op == "checkpoint" && ++checkpoints == 3;
    }});
    assert(failing.begin_create("recording-D", "sink-D"));
    ContextFrame frame; frame.mutable_event()->set_name("committed");
    std::uint64_t position = 0; assert(failing.append(frame, position));
    assert(failing.confirm_create("recording-D", 0));
    assert(!failing.save_ack("recording-D", 1));
    assert(failing.state().acknowledged == 0);
    assert(failing.data_bytes() != 0); // Failed ACK checkpoint cannot reclaim data.
  }
  {
    ContextJournal recovered(fault_dir);
    assert(recovered.healthy() && recovered.state().acknowledged == 0);
    assert(recovered.read_after(0, 2, 4096).size() == 1);
  }
  std::filesystem::remove_all(fault_dir);
  ContextFrame durable_event;
  durable_event.mutable_event()->set_event_id("durable-event");
  durable_event.mutable_event()->set_recording_position(1);
  auto *durable_state = durable_event.mutable_event()->add_states();
  durable_state->set_schema_identity("stable-schema-revision");
  durable_state->set_epoch_schema_id(3);
  durable_state->set_payload("same-original-payload");
  {
    tracer::context_delivery::ContextCollectorStore collector(collector_dir);
    auto created = collector.open(true, "recording-B", "sink-B", 0);
    assert(created.accepted && created.durable_prefix == 0);
    auto repeated_create = collector.open(true, "recording-B", "sink-B", 0);
    assert(repeated_create.accepted && repeated_create.durable_prefix == 0);
    assert(!collector.open(false, "missing", "sink-B", 0).accepted);
    assert(collector.persist(durable_event, 1).accepted);
    assert(collector.durable_prefix() == 1);
    auto remapped = durable_event;
    remapped.mutable_event()->mutable_states(0)->set_epoch_schema_id(91);
    assert(collector.persist(remapped, 1).accepted); // Epoch remapping does not alter record identity.
    auto conflict = durable_event; conflict.mutable_event()->set_event_id("conflict");
    assert(!collector.persist(conflict, 1).accepted);
  }
  {
    tracer::context_delivery::ContextCollectorStore collector(collector_dir);
    auto resumed = collector.open(false, "recording-B", "sink-B", 0);
    assert(resumed.accepted && resumed.durable_prefix == 1);
    assert(!collector.open(false, "recording-B", "sink-B", 2).accepted);
    auto duplicate = collector.persist(durable_event, 1);
    assert(duplicate.accepted && duplicate.durable_prefix == 1);
  }
  std::filesystem::remove_all(collector_dir);
  const auto first_charge = 24 + small.ByteSizeLong() + 8;
  {
    ContextJournal pressure(pressure_dir,
        {static_cast<std::uint64_t>(first_charge + 1), 1024});
    std::uint64_t position = 0;
    assert(pressure.append(small, position));
    assert(position == 1);
    assert(!pressure.append(small, position)); // Full data budget rejects the newest capture.
    assert(pressure.committed_prefix() == 1);
  }
  std::filesystem::remove_all(pressure_dir);

  // A lease remains held by the consumer after SDK completion until the
  // consumer releases its returned packet object.
  {
    tracer::context_delivery::ContextHookRing ring(256, 4, 16);
    int released = 0;
    struct Witness { int *released = nullptr; ~Witness() { ++*released; } };
    auto owner = std::make_shared<Witness>();
    owner->released = &released;
    std::shared_ptr<const void> lease(owner, owner.get());
    owner.reset();
    const std::string bytes = "serialized";
    assert(ring.try_publish(std::as_bytes(std::span(bytes.data(), bytes.size())), lease));
    lease.reset();
    tracer::context_delivery::HookPacket packet;
    assert(ring.try_take(packet));
    assert(packet.serialized_frame == bytes && released == 0);
    ring.complete(packet.reservation);
    assert(released == 0);
    packet.lease.reset();
    assert(released == 1);
  }

  // Packet credits are process-wide across independently configured sinks;
  // an atomic batch cannot partially consume the final shared slot.
  {
    tracer::context_delivery::ContextHookRing first(64ull * 1024 * 1024, 4096, 32);
    tracer::context_delivery::ContextHookRing second(64ull * 1024 * 1024, 4096, 32);
    const std::string one = "x";
    const auto bytes = std::as_bytes(std::span(one.data(), one.size()));
    for (std::size_t i = 0; i < 4095; ++i) assert(first.try_publish(bytes));
    const tracer::context_delivery::HookPacketView pair[] = {{bytes, {}}, {bytes, {}}};
    assert(!second.try_publish_batch(pair));
    tracer::context_delivery::HookPacket packet;
    for (std::size_t i = 0; i < 4095; ++i) {
      assert(first.try_take(packet));
      first.complete(packet.reservation);
    }
    assert(second.try_publish(bytes));
  }

  // Worker staging is reserved from the same process-wide EngineStorage as
  // queued bytes. Independent clients cannot each admit a private 64 MiB.
  {
    tracer::context_delivery::ContextHookRing first;
    tracer::context_delivery::ContextHookRing second;
    const std::string large(1024 * 1024, 's');
    const auto bytes = std::as_bytes(std::span(large.data(), large.size()));
    std::size_t first_count = 0, second_count = 0;
    for (std::size_t i = 0; i < 32; ++i) {
      const bool admitted_first = first.try_publish(bytes);
      const bool admitted_second = second.try_publish(bytes);
      if (admitted_first) ++first_count;
      if (admitted_second) ++second_count;
      if (!admitted_first && !admitted_second) break;
    }
    assert(first_count + second_count > 0);
    assert(first_count + second_count < 32);
    tracer::context_delivery::HookPacket packet;
    for (std::size_t i = 0; i < first_count; ++i) {
      assert(first.try_take(packet));
      first.complete(packet.reservation);
    }
    packet = {};
    for (std::size_t i = 0; i < second_count; ++i) {
      assert(second.try_take(packet));
      second.complete(packet.reservation);
    }
  }

  // Exercise the actual bidi client/server path, epoch schema IDs, and hook
  // byte-ring capture on a loopback gRPC server.
  {
    using namespace std::chrono_literals;
    using namespace clangtracer::context::v1;
    std::mutex mutex;
    std::condition_variable cv;
    bool received = false;
    int durable_controls = 0;
    auto collector_store = std::make_shared<tracer::context_delivery::ContextCollectorStore>(
        std::filesystem::temp_directory_path() / ("clang-tracer-grpc-" + std::to_string(::getpid())));
    const auto producer_journal_path = std::filesystem::temp_directory_path() /
        ("clang-tracer-producer-journal-" + std::to_string(::getpid()));
    std::filesystem::remove_all(producer_journal_path);
    auto producer_journal = std::make_shared<tracer::context_delivery::ContextJournal>(producer_journal_path);
    tracer::context_delivery::ContextExportService service(collector_store, [&](const ContextFrame &frame) {
      if (frame.has_event()) { std::lock_guard lock(mutex); received = true; cv.notify_all(); }
      return true;
    });
    auto rpc = start_rpc_server(service);
    auto &server = rpc.server;
    auto channel = rpc.channel;
    ExportOpen open;
    open.set_protocol_version(1); open.set_epoch_id(1); open.set_producer_session_id("session");
    open.set_profile(DURABLE); open.set_operation(CREATE);
    open.set_recording_id("grpc-recording"); open.set_sink_id("grpc-sink");
    tracer::context_delivery::ContextExportClient client(channel, open, {}, [&](const DurableControl &control) {
      std::lock_guard lock(mutex); ++durable_controls; cv.notify_all();
      return control.result() == DurableControl::ACCEPTED && control.recording_id() == "grpc-recording";
    }, producer_journal);
    assert(client.start());
    ContextBatch batch;
    auto *schema = batch.add_schemas();
    schema->set_schema_identity("revision-1"); schema->set_revision_id("revision-1");
    schema->set_root_type("demo.State"); schema->set_serialized_file_descriptor_set("fds");
    auto *event = batch.mutable_event(); event->set_name("event"); event->set_event_id("event-1");
    auto *attachment = event->add_states(); attachment->set_schema_identity("revision-1");
    attachment->set_payload("bytes");
    const auto batch_bytes = batch.SerializeAsString();
    assert(client.hook_ring().try_publish(std::as_bytes(std::span(batch_bytes.data(), batch_bytes.size()))));
    {
      std::unique_lock lock(mutex);
      assert(cv.wait_for(lock, 5s, [&] { return received && durable_controls >= 2; }));
    }
    client.stop();
    assert(collector_store->durable_prefix() == 1);
    assert(producer_journal->state().creation_confirmed && producer_journal->state().acknowledged == 1);
    open.set_operation(RESUME); open.set_saved_ack_position(1); open.set_epoch_id(2);
    ContextBatch replay_batch;
    assert(replay_batch.ParseFromString(batch_bytes));
    replay_batch.mutable_event()->set_recording_position(1);
    const auto replay_bytes = replay_batch.SerializeAsString();
    int resume_controls = 0;
    tracer::context_delivery::ContextExportClient resumed(channel, open, {}, [&](const DurableControl &control) {
      std::lock_guard lock(mutex); ++resume_controls; cv.notify_all();
      return control.result() == DurableControl::ACCEPTED && control.durable_prefix() == 1;
    }, producer_journal);
    assert(resumed.start());
    assert(resumed.hook_ring().try_publish(std::as_bytes(std::span(replay_bytes.data(), replay_bytes.size()))));
    {
      std::unique_lock lock(mutex);
      assert(cv.wait_for(lock, 5s, [&] { return resume_controls >= 2; }));
    }
    resumed.stop();
    assert(collector_store->durable_prefix() == 1);
    server->Shutdown(); server->Wait();
    std::filesystem::remove_all(producer_journal_path);
    std::filesystem::remove_all(std::filesystem::temp_directory_path() /
        ("clang-tracer-grpc-" + std::to_string(::getpid())));
  }

  // One durable service can keep independent recording/sink checkpoints
  // under one configured root instead of binding every producer to one store.
  {
    using namespace std::chrono_literals;
    using namespace clangtracer::context::v1;
    const auto root = std::filesystem::temp_directory_path() /
        ("clang-tracer-multi-recording-" + std::to_string(::getpid()));
    std::filesystem::remove_all(root);
    std::mutex mutex;
    std::condition_variable cv;
    std::array<std::shared_ptr<tracer::context_delivery::ContextCollectorStore>, 2>
        stores{};
    const auto store_factory = [&](std::string_view recording,
                                  std::string_view sink) {
      const std::size_t index = recording == "recording-A" ? 0 : 1;
      assert(recording == (index == 0 ? "recording-A" : "recording-B"));
      assert(sink == (index == 0 ? "sink-A" : "sink-B"));
      std::lock_guard lock(mutex);
      if (!stores[index])
        stores[index] = std::make_shared<
            tracer::context_delivery::ContextCollectorStore>(
            root / (index == 0 ? "recording-A" : "recording-B"));
      return stores[index];
    };
    tracer::context_delivery::ContextExportService service(
        std::shared_ptr<tracer::context_delivery::ContextCollectorStore>{},
        {}, {}, {}, {}, store_factory);
    auto rpc = start_rpc_server(service);
    const auto send_recording = [&](std::size_t index) {
      const std::string recording = index == 0 ? "recording-A" : "recording-B";
      const std::string sink = index == 0 ? "sink-A" : "sink-B";
      const auto producer_dir = root / ("producer-" + recording);
      std::filesystem::remove_all(producer_dir);
      auto journal = std::make_shared<tracer::context_delivery::ContextJournal>(
          producer_dir);
      ExportOpen open;
      open.set_protocol_version(1);
      open.set_epoch_id(1);
      open.set_producer_session_id("producer-" + recording);
      open.set_profile(DURABLE);
      open.set_operation(CREATE);
      open.set_recording_id(recording);
      open.set_sink_id(sink);
      unsigned confirmations = 0;
      tracer::context_delivery::ContextExportClient client(
          rpc.channel, open, {}, [&](const DurableControl &control) {
            std::lock_guard lock(mutex);
            ++confirmations;
            cv.notify_all();
            return control.result() == DurableControl::ACCEPTED &&
                   control.recording_id() == recording;
          }, journal);
      assert(client.start());
      ContextFrame frame;
      frame.mutable_event()->set_event_id("event-" + recording);
      frame.mutable_event()->set_name("event");
      const auto bytes = frame.SerializeAsString();
      assert(client.hook_ring().try_publish(
          std::as_bytes(std::span(bytes.data(), bytes.size()))));
      {
        std::unique_lock lock(mutex);
        assert(cv.wait_for(lock, 5s, [&] { return confirmations >= 2; }));
      }
      client.stop();
      assert(stores[index] && stores[index]->durable_prefix() == 1);
    };
    send_recording(0);
    send_recording(1);
    rpc.server->Shutdown();
    rpc.server->Wait();
    for (std::size_t index = 0; index < stores.size(); ++index) {
      stores[index].reset();
      const auto recording = index == 0 ? "recording-A" : "recording-B";
      const auto sink = index == 0 ? "sink-A" : "sink-B";
      tracer::context_delivery::ContextCollectorStore recovered(
          root / recording);
      const auto resumed = recovered.open(false, recording, sink, 1);
      assert(resumed.accepted && resumed.durable_prefix == 1);
    }
    std::filesystem::remove_all(root);
  }

  // Complete schema sets are reserved together. A second set that fits an
  // empty epoch rotates once and carries the unsent logical batch forward.
  {
    using namespace std::chrono_literals;
    using namespace clangtracer::context::v1;
    std::mutex mutex;
    std::condition_variable cv;
    std::size_t events_received = 0;
    tracer::context_delivery::StreamLimits limits;
    limits.max_epoch_schemas = 1;
    tracer::context_delivery::ContextExportService service(
        [&](const ContextFrame &frame) {
          if (frame.has_event()) { std::lock_guard lock(mutex); ++events_received; cv.notify_all(); }
          return true;
        }, {}, limits);
    auto rpc = start_rpc_server(service);
    auto &server = rpc.server;
    auto channel = rpc.channel;
    ExportOpen open;
    open.set_protocol_version(1); open.set_epoch_id(1); open.set_producer_session_id("batch-session");
    open.set_profile(BEST_EFFORT);
    tracer::context_delivery::ContextExportClient client(channel, open, limits);
    assert(client.start());
    for (int i = 0; i != 2; ++i) {
      ContextBatch batch;
      auto *schema = batch.add_schemas();
      const auto identity = std::string("batch-schema-") + std::to_string(i);
      schema->set_schema_identity(identity); schema->set_revision_id(identity);
      schema->set_root_type("demo.State"); schema->set_serialized_file_descriptor_set("fds");
      auto *event = batch.mutable_event(); event->set_name("batched-event");
      event->set_event_id(std::string("batch-event-") + std::to_string(i));
      auto *state = event->add_states(); state->set_schema_identity(identity); state->set_payload("state");
      const auto encoded = batch.SerializeAsString();
      assert(client.hook_ring().try_publish(std::as_bytes(std::span(encoded.data(), encoded.size()))));
    }
    {
      std::unique_lock lock(mutex);
      assert(cv.wait_for(lock, 5s, [&] { return events_received == 2; }));
    }
    client.stop();
    server->Shutdown(); server->Wait();
  }

  // A batch held before gRPC submission survives a disconnected epoch and is
  // remapped from its logical schema identity in the next stream.
  {
    using namespace clangtracer::context::v1;
    ReconnectService service;
    auto rpc = start_rpc_server(service);
    ExportOpen open;
    open.set_protocol_version(1);
    open.set_epoch_id(1);
    open.set_producer_session_id("reconnect-session");
    open.set_profile(BEST_EFFORT);
    tracer::context_delivery::ContextExportClient client(rpc.channel, open);
    assert(client.start());
    ContextBatch batch;
    auto *schema = batch.add_schemas();
    schema->set_schema_identity("reconnect-revision");
    schema->set_revision_id("reconnect-revision");
    schema->set_root_type("demo.State");
    schema->set_serialized_file_descriptor_set("fds");
    auto *event = batch.mutable_event();
    event->set_event_id("reconnect-event");
    event->set_name("reconnect");
    auto *state = event->add_states();
    state->set_schema_identity("reconnect-revision");
    state->set_payload("owned-bytes");
    const auto encoded = batch.SerializeAsString();
    assert(client.hook_ring().try_publish(
        std::as_bytes(std::span(encoded.data(), encoded.size()))));
    service.wait_for_event();
    client.stop();
    rpc.server->Shutdown();
    rpc.server->Wait();
    assert(service.attempts.load() >= 2);
  }

  // A large immutable descriptor is leased as native metadata and serialized
  // by the background worker; the 256 KiB capture event remains independent.
  {
    using namespace std::chrono_literals;
    using namespace clangtracer::context::v1;
    using namespace tracer::context_engine;
    std::mutex mutex;
    std::condition_variable cv;
    bool schema_received = false, event_received = false;
    constexpr std::size_t descriptor_bytes = 1024 * 1024;
    tracer::context_delivery::ContextExportService service(
        [&](const ContextFrame &frame) {
          std::lock_guard lock(mutex);
          if (frame.has_schema()) {
            schema_received =
                frame.schema().serialized_file_descriptor_set().size() ==
                descriptor_bytes;
          } else if (frame.has_event()) {
            event_received = frame.event().states_size() == 1 &&
                frame.event().states(0).schema_identity() == "large-revision" &&
                frame.event().states(0).epoch_schema_id() == 1;
          }
          cv.notify_all();
          return true;
        });
    auto rpc = start_rpc_server(service);
    ExportOpen open;
    open.set_protocol_version(1);
    open.set_epoch_id(1);
    open.set_producer_session_id("event-only-session");
    open.set_profile(BEST_EFFORT);
    tracer::context_delivery::ContextExportClient client(rpc.channel, open);
    assert(client.start());

    StateFamilyKey family;
    family.source_lineage_id[0] = 0x42;
    family.cpp_state_type = "fixture::State";
    family.cpp_scope_type = "fixture::Scope";
    auto record = std::make_shared<const tracer::context_engine::SchemaRecord>(
        family, "fixture.State", "large-revision",
        std::string(descriptor_bytes, 'D'), nullptr);
    const auto lineage = std::string_view(
        reinterpret_cast<const char *>(family.source_lineage_id.data()),
        family.source_lineage_id.size());
    const std::array<std::string, 0> roles{};
    const CaptureSchemaView schema{
        record->revision_id, record->root_type, record->descriptor_set, lineage,
        family.cpp_state_type, family.cpp_scope_type, record->revision_id,
        roles, 1, record};
    std::int64_t value = 73;
    const CaptureStateView state{7, record->revision_id, 1, &value, nullptr,
                                 &encode_delivery_state};
    constexpr std::string_view identifier = "0123456789abcdef";
    const CaptureEventMetadata metadata{identifier, identifier, 1, identifier,
        {}, {}, 0, "large-descriptor.event"};
    ContextCapturePool capture_pool;
    auto capture = capture_pool.try_reserve(4);
    assert(capture);
    const auto captured = capture.capture(metadata, std::span(&schema, 1),
                                          std::span(&state, 1));
    assert(captured && captured.bytes < 256 * 1024);
    assert(capture.publish_leased(&publish_capture_to_client, &client));
    {
      std::unique_lock lock(mutex);
      assert(cv.wait_for(lock, 10s,
                         [&] { return schema_received && event_received; }));
    }
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (capture_pool.outstanding(4) != 0 &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(1ms);
    assert(capture_pool.outstanding(4) == 0);
    client.stop();
    rpc.server->Shutdown();
    rpc.server->Wait();
  }
}
