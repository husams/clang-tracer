#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <google/protobuf/descriptor.pb.h>
#include <grpcpp/grpcpp.h>
#include <unistd.h>

#include "controller.hpp"
#include "../src/sqlite_events.hpp"

using namespace tracer_repl;
namespace api = clangtracer::v1;

namespace {
void require(bool ok, const std::string &why) {
  if (!ok) {
    throw std::runtime_error(why);
  }
}

void check_trace_statistics() {
  EventBuffer buffer(20, 65536);
  const auto event = [](std::uint64_t call, api::TracePhase phase,
                        std::uint64_t time, std::uint64_t cursor) {
    api::TraceEvent value;
    value.set_server_instance_id("server");
    value.set_session_id("session");
    value.set_pid(7);
    value.set_probe_id(5);
    value.set_tid(9);
    value.set_call_id(call);
    value.set_phase(phase);
    value.set_monotonic_time_ns(time);
    value.set_sequence(cursor);
    value.set_function_name("callee(int)");
    return value;
  };
  const auto stack = [](api::TraceEvent &value, std::uint64_t caller_sp) {
    auto *trace = value.mutable_stack();
    trace->set_status(api::StackTrace::PARTIAL);
    auto *entry = trace->add_frames();
    entry->set_kind(api::StackFrame::TARGET_ENTRY);
    entry->set_pc(0x1010);
    entry->set_stack_pointer(0x1000);
    auto *caller = trace->add_frames();
    caller->set_pc(0x2030);
    caller->set_stack_pointer(caller_sp);
    return caller;
  };
  buffer.push(event(1, api::POST, 200, 2));
  auto pre = event(1, api::PRE, 100, 1);
  auto *caller = stack(pre, 0x1008);
  caller->set_function("parent()");
  caller->set_module("/app");
  pre.mutable_stack()->set_application_only(true);
  pre.mutable_stack()->set_hidden_frames(1);
  buffer.push(pre);
  buffer.push(pre); // The same server cursor is a replayed copy.
  auto no_post = event(2, api::PRE, 200, 3);
  stack(no_post, 0x1020)->set_function("hidden_caller_was_skipped()");
  no_post.mutable_stack()->set_application_only(true);
  no_post.mutable_stack()->set_hidden_frames(1);
  buffer.push(no_post);
  auto no_pre = event(3, api::POST, 300, 4);
  stack(no_pre, 0x1008);
  buffer.push(no_pre);
  buffer.push(event(4, api::PRE, 400, 5));
  buffer.push(event(4, api::POST, 390, 6));
  buffer.push(event(5, api::PRE, 500, 7));
  buffer.push(event(5, api::PRE, 510, 8));
  buffer.push(event(5, api::POST, 600, 9));
  buffer.push(event(6, api::PRE, 700, 10));
  buffer.push(event(6, api::POST, 700, 11));
  auto contextual = event(2, api::POST, 250, 12);
  contextual.mutable_context();
  buffer.push(contextual);
  buffer.push(event(0, api::PRE, 800, 13));
  auto other_server = event(2, api::POST, 900, 14);
  other_server.set_server_instance_id("new-server");
  buffer.push(other_server);
  auto other_probe = event(1, api::PRE, 1, 15);
  other_probe.set_probe_id(6);
  other_probe.set_function_name("other()");
  auto *offset = stack(other_probe, 0x1008);
  offset->set_module("/other");
  offset->set_module_offset(0x30);
  buffer.push(other_probe);
  other_probe.set_phase(api::POST);
  other_probe.set_sequence(16);
  other_probe.set_monotonic_time_ns(2000000001);
  buffer.push(other_probe);

  std::ostringstream output;
  event_statistics(buffer, output, false);
  const auto report = output.str();
  require(report.find("server=\"server\" session=\"session\" pid=7 probe=5 calls=6 "
                      "paired=4 timed=2 missing_pre=1 missing_post=1 untimed_pairs=2 "
                      "ambiguous=1 caller_unknown=4 histogram_ns=[0..99:1,100..999:1]") !=
              std::string::npos,
          "duration boundaries, incomplete pairs and ambiguous phases: " + report);
  require(report.find("server=\"new-server\" session=\"session\" pid=7 probe=5 calls=1 "
                      "paired=0 timed=0 missing_pre=1 missing_post=0") != std::string::npos &&
              report.find("unidentified_events=1") != std::string::npos,
          "separate server identities and missing call IDs: " + report);
  require(report.find("caller_module=\"/app\" caller=\"parent()\" calls=1") != std::string::npos &&
              report.find("caller_pc=0x2030 calls=1") != std::string::npos &&
              report.find("hidden_caller_was_skipped") == std::string::npos,
          "caller edges with symbol, raw PC and filtered stack evidence: " + report);
  require(report.find("pid=7 probe=6 calls=1 paired=1 timed=1 missing_pre=0 "
                      "missing_post=0 untimed_pairs=0 ambiguous=0 caller_unknown=0 "
                      "histogram_ns=[1000000000+:1]") != std::string::npos &&
              report.find("caller_module=\"/other\" caller_offset=0x30 calls=1") !=
                  std::string::npos,
          "separate probe identity, final bucket and module offset: " + report);
}

class Service final : public api::Tracer::Service {
public:
  std::atomic<bool> attached{false};
  std::atomic<bool> failed{false};
  std::atomic<int> attach_mode{0};
  std::atomic<unsigned> attach_calls{0};
  std::atomic<unsigned> installed{0}, detached{0};
  std::atomic<bool> pair_created{false}, detached_s1{false}, detached_s2{false};
  std::function<void()> on_watch;
  std::atomic<int> watch_mode{0};
  std::atomic<unsigned> active_watches{0};

  grpc::Status ListSessions(grpc::ServerContext *,
                            const api::ListSessionsRequest *,
                            api::ListSessionsResponse *r) override {
    if (attached || pair_created) {
      auto *s = r->add_sessions();
      s->set_id("s1");
      s->set_pid(7);
      s->set_state(pair_created && detached_s1 ? api::Session::DETACHED
                   : failed                    ? api::Session::FAILED
                                               : api::Session::ATTACHED);
    }
    if (pair_created) {
      auto *s = r->add_sessions();
      s->set_id("s2");
      s->set_pid(7);
      s->set_state(detached_s2 ? api::Session::DETACHED
                               : api::Session::ATTACHED);
    }
    return grpc::Status::OK;
  }

  grpc::Status Attach(grpc::ServerContext *, const api::AttachRequest *q,
                      api::Session *r) override {
    ++attach_calls;
    const int mode = attach_mode.load();
    if (mode == 3)
      std::this_thread::sleep_for(std::chrono::milliseconds(1250));
    attached = true;
    failed = mode == 1;
    if (mode == 4)
      pair_created = true;
    r->set_id("s1");
    r->set_pid(q->pid());
    r->set_state(api::Session::ATTACHED);
    if (mode == 1)
      return {grpc::StatusCode::INTERNAL, "retained attach failure"};
    if (mode == 2)
      return {grpc::StatusCode::DEADLINE_EXCEEDED, "lost attach reply"};
    if (mode == 4)
      return {grpc::StatusCode::INTERNAL, "ambiguous concurrent attach"};
    return grpc::Status::OK;
  }

  grpc::Status Detach(grpc::ServerContext *, const api::DetachRequest *q,
                      api::Session *r) override {
    if (q->session_id() == "s1") {
      attached = false;
      detached_s1 = true;
    }
    if (q->session_id() == "s2")
      detached_s2 = true;
    failed = false;
    ++detached;
    r->set_id(q->session_id());
    r->set_pid(7);
    r->set_state(api::Session::DETACHED);
    return grpc::Status::OK;
  }

  grpc::Status ResolveProbe(grpc::ServerContext *,
                            const api::ResolveProbeRequest *q,
                            api::ProbeSpec *r) override {
    r->set_symbol(q->selector());
    r->mutable_signature()->set_result(api::INT32);
    return grpc::Status::OK;
  }

  grpc::Status AddProbe(grpc::ServerContext *, const api::AddProbeRequest *q,
                        api::Probe *r) override {
    if (q->spec().callback_source() != "// client-only interceptor\n") {
      return {grpc::StatusCode::INVALID_ARGUMENT, "source content mismatch"};
    }
    ++installed;
    r->set_active(true);
    r->set_id(5);
    r->set_session_id(q->session_id());
    return grpc::Status::OK;
  }

  grpc::Status
  WatchEvents(grpc::ServerContext *c, const api::WatchEventsRequest *q,
              grpc::ServerWriter<api::TraceEvent> *writer) override {
    struct Active {
      std::atomic<unsigned>& count;

      explicit Active(std::atomic<unsigned>& n)
          : count(n) {
        ++count;
      }

      ~Active() { --count; }
    } active(active_watches);

    const int mode = watch_mode;
    if (mode && (q->session_ids_size() != 1 || q->session_ids(0) != "s1")) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "missing session filter"};
    }
    if (mode == 2) {
      return {grpc::StatusCode::UNAVAILABLE, "background failure"};
    }
    if (on_watch)
      on_watch();
    if (q->after_sequence() == 900) {
      while (!c->IsCancelled()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return {grpc::StatusCode::CANCELLED, "cancelled"};
    }
    for (unsigned i = 1; mode != 3 && i <= 4; ++i) {
      api::TraceEvent e;
      e.set_sequence(i);
      e.set_session_id("s1");
      e.set_probe_id(5);
      e.set_call_id((i + 1) / 2);
      e.set_tid(9);
      e.set_pid(7);
      e.set_monotonic_time_ns(i * 10);
      e.set_phase(i % 2 ? api::PRE : api::POST);
      e.set_function_name("multiply(int, int)");
      auto *arg = e.add_arguments();
      arg->set_type(api::INT32);
      arg->set_signed_value(21);
      if (i % 2 == 0) {
        e.mutable_result()->set_type(api::INT32);
        e.mutable_result()->set_signed_value(42);
      }
      if (q->include_replay_types()) {
        e.set_server_instance_id("fixture");
        e.mutable_context()->mutable_event()->add_states();
        if (i == 1) {
          auto *types = e.mutable_replay_types();
          types->set_format_version(4);
          types->set_adapter_declarations("struct ReplayMarker { int value; };");
        }
      }
      if (!writer->Write(e)) {
        break;
      }
    }
    if (mode == 1 || mode == 3) {
      while (!c->IsCancelled()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
    return grpc::Status::OK;
  }
};
} // namespace

int main(int argc, char **argv) {
  try {
    const bool unsafe_replay = argc == 3 && std::string(argv[1]) == "--write-unsafe-replay-db";
    if (argc == 3 && (std::string(argv[1]) == "--write-replay-db" || unsafe_replay)) {
      api::TraceEvent event;
      event.set_server_instance_id("server");
      event.set_session_id("s1");
      event.set_pid(7);
      event.set_sequence(1);
      event.set_function_name("multiply(int, int)");
      event.set_phase(api::PRE);
      std::vector<api::TraceEvent> events{event};
      event.set_sequence(2);
      event.set_phase(api::POST);
      event.mutable_result()->set_type(api::INT32);
      event.mutable_result()->set_signed_value(42);
      events.push_back(event);
      event.set_sequence(10);
      event.set_session_id("s2");
      event.set_pid(8);
      events.push_back(event);
      event.set_sequence(11);
      auto *context = event.mutable_context();
      context->mutable_event()->set_name("custom.event");
      google::protobuf::FileDescriptorSet descriptors;
      auto *file = descriptors.add_file();
      file->set_name("replay_state.proto");
      file->set_package("tracer.generated.state");
      auto *record = file->add_message_type();
      record->set_name("M_8b906fc299f87a61"); // Canonical name for ReplayState.
      auto *field = record->add_field();
      field->set_name("label");
      field->set_number(1);
      field->set_type(google::protobuf::FieldDescriptorProto::TYPE_STRING);
      field->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
      auto *schema = context->add_schemas();
      schema->set_epoch_schema_id(1);
      schema->set_cpp_state_type("ReplayState");
      schema->set_root_type("tracer.generated.state." + record->name());
      schema->set_serialized_file_descriptor_set(descriptors.SerializeAsString());
      auto *state = context->mutable_event()->add_states();
      state->set_epoch_schema_id(1);
      state->set_payload("\x0a\x05" "saved");
      if (unsafe_replay) {
        auto *adapter = event.mutable_replay_types();
        adapter->set_format_version(4);
        adapter->set_adapter_declarations(
            "namespace replay_test { int init = *reinterpret_cast<volatile int *>(1); }");
      }
      events.push_back(event);
      tracer::client::SQLiteEvents(argv[2]).write(events);
      return 0;
    }
    Service service;
    if (argc == 3 &&
        (std::string(argv[1]) == "--listen" || std::string(argv[1]) == "--listen-live")) {
      if (std::string(argv[1]) == "--listen-live") {
        service.watch_mode = 1;
      }
      grpc::ServerBuilder builder;
      builder.AddListeningPort(argv[2], grpc::InsecureServerCredentials());
      builder.RegisterService(&service);
      auto server = builder.BuildAndStart();
      require(bool(server), "fixture server startup");
      std::cout << "ready\n" << std::flush;
      server->Wait();
      return 0;
    }
    grpc::ServerBuilder builder;
    int port = 0;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(),
                             &port);
    builder.RegisterService(&service);
    auto server = builder.BuildAndStart();
    require(server && port, "test service startup");
    Settings settings;
    settings.endpoint = "127.0.0.1:" + std::to_string(port);
    std::ostringstream out;
    {
      Controller controller(settings, out);
      auto session = controller.attach(7);
      require(session.pid == 7, "selected session");
      controller.inject(session, "multiply(int, int)",
                        "// client-only interceptor\n", Command{});
      require(service.installed == 1, "upload source by value");
      controller.execute(parse_command("%collect --count 4 --as batch"));
      auto captured = controller.batch("batch");
      require(captured.complete && captured.size() == 4, "bounded collection");
      require(captured.at(0).argument(0) == "21" &&
                  captured.at(1).result() == "42",
              "typed C++ inspection");
      controller.execute(parse_command("%events show batch --index 1"));
      controller.execute(parse_command("%events calls batch"));
      require(out.str().find("duration_ns=10") != std::string::npos,
              "paired calls");
      captured.events.clear();
      std::atomic<bool> replacement_released{false};
      service.on_watch = [&] {
        try {
          (void)controller.batch("batch");
        } catch (const std::runtime_error &) {
          replacement_released = true;
        }
      };
      controller.execute(parse_command("%collect --count 4 --as batch"));
      service.on_watch = {};
      require(replacement_released, "same-name capture releases the old ring "
                                    "before receiving new events");
      auto foreign = session;
      ++foreign.connection;
      try {
        controller.require_session(foreign);
        throw std::logic_error("accepted stale handle");
      } catch (const std::runtime_error &) {
      }
      interrupted = false;
      std::thread cancel([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        interrupted = true;
      });
      controller.execute(
          parse_command("%collect --after 900 --as interrupted"));
      cancel.join();
      require(controller.batch("interrupted").cancelled,
              "continuous stream cancellation");
      interrupted = false;
      controller.execute(parse_command("%server status"));
      require(service.attached, "cancel preserves attachment");
      controller.detach(session);
      require(service.detached == 1, "detach RPC");
    }
    service.attached = true;
    {
      Controller observer(settings, out);
      observer.attach(7);
      observer.shutdown();
    }
    require(service.attached && service.detached == 1,
            "pre-existing session ownership preserved");
    service.attached = false;
    {
      Controller disconnected(settings, out);
      Facade facade(disconnected);
      try {
        facade.detach({"s1", 7, 1});
        throw std::logic_error("accepted disconnected handle");
      } catch (const std::runtime_error &) {
      }
    }
    for (int mode : {1, 2, 3, 4}) {
      service.attach_mode = mode;
      service.detached_s1 = false;
      service.detached_s2 = false;
      Settings recovery_settings = settings;
      recovery_settings.timeout_seconds = 1;
      Controller recovery(recovery_settings, out);
      const auto attempts = service.attach_calls.load();
      const auto detach_count = service.detached.load();
      try {
        recovery.attach(7);
        throw std::logic_error("accepted failed attach");
      } catch (const std::runtime_error &error) {
        const std::string message = error.what();
        require(message.find("outcome uncertain") != std::string::npos &&
                    message.find("pid=7") != std::string::npos &&
                    message.find(settings.endpoint) != std::string::npos,
                "failed attach reports endpoint and PID recovery identifiers");
      }
      try {
        recovery.attach(7);
        throw std::logic_error("retried uncertain attach");
      } catch (const std::runtime_error &) {
      }
      require(service.attach_calls == attempts + 1,
              "uncertain attach is not retried or adopted");
      for (unsigned i = 0; i < 100 && !service.attached; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      require(service.attached, "late attach fixture published its session");
      try {
        recovery.shutdown();
        throw std::logic_error("lost external recovery state");
      } catch (const std::runtime_error &error) {
        require(std::string(error.what()).find("candidate_session=s1") !=
                    std::string::npos,
                "shutdown reconciles late session IDs");
      }
      require(service.detached == detach_count,
              "ambiguous external sessions are never claimed for automatic "
              "cleanup");
      recovery.execute(parse_command("%detach s1"));
      if (mode == 4) {
        const auto start = out.str().size();
        recovery.execute(parse_command("%status"));
        const auto status = out.str().substr(start);
        require(
            status.find("candidate_session=s1") == std::string::npos &&
                status.find("candidate_session=s2") != std::string::npos,
            "explicitly resolved candidates do not reappear in reconciliation");
        recovery.execute(parse_command("%detach s2"));
      }
      recovery.shutdown();
    }
    service.pair_created = false;
    service.attach_mode = 0;
    auto wait_for = [](auto predicate) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (!predicate()) {
        require(std::chrono::steady_clock::now() < deadline, "background operation did not finish");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    };
    settings.max_events = 2;
    service.watch_mode = 1;
    {
      Controller background(settings, out);
      Facade tracer(background);
      auto session = tracer.attach(7);
      {
        auto sync = tracer.events(session, 4, 2);
        require(!sync.running() && sync.snapshot().complete,
                "synchronous range waits for requested events");
        unsigned count = 0;
        for (const auto& event : sync) {
          (void)event;
          ++count;
        }
        require(count == 2, "sync range preserves bounded retention");
      }
      {
        const auto start = std::chrono::steady_clock::now();
        auto live = tracer.events_async(session);
        require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1),
                "background start returns promptly");
        wait_for([&] { return live.snapshot().received == 4; });
        require(live.running(), "collector remains active between REPL inputs");
        auto first_event = live | take(1) | first();
        require(first_event && first_event->sequence() == 3,
                "a limit ends iteration without waiting for a further event");
        auto post_event = live | filter([](const auto &e) { return e.phase() == "POST"; }) | take(1) | first();
        require(post_event && post_event->sequence() == 4,
                "event predicates compose with limits");
        background.execute(parse_command("%sessions"));
        const auto snapshot = live.snapshot();
        require(snapshot.size() == 2 && snapshot.evicted == 2,
                "background retention respects the shared count budget");
        unsigned count = 0;
        for (const auto& event : live) {
          require(event.sequence() == ++count + 2, "C++17 snapshot range order");
          live.stop();
        }
        require(count == 2 && !live.running() && live.snapshot().cancelled && live.error().empty(),
                "stop preserves iterable snapshots");
        live.stop();
        try {
          (void)tracer.events_async(session);
          throw std::logic_error("exceeded background storage reservation");
        } catch (const std::runtime_error&) {
        }
      }
      wait_for([&] { return service.active_watches == 0; });
      {
        auto counted = tracer.events_async(session, 4, 2);
        wait_for([&] { return !counted.running(); });
        require(counted.snapshot().complete && counted.snapshot().received == 4,
                "counted collection completes automatically");
      }
      service.watch_mode = 3;
      {
        auto timed = tracer.events(session, 1, 1);
        require(!timed.running() && timed.snapshot().timed_out,
                "synchronous quiet collection returns on deadline");
      }
      {
        std::thread cancel([] {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          interrupted = true;
        });
        auto cancelled = tracer.events(session, 1, 10);
        cancel.join();
        interrupted = false;
        require(cancelled.snapshot().cancelled, "synchronous collection responds to Ctrl+C");
      }
      {
        auto quiet = tracer.events_async(session, 0, 1);
        unsigned count = 0;
        for (const auto& event : quiet) {
          (void)event;
          ++count;
        }
        require(count == 0 && !quiet.running(), "iteration waits for the stream to end");
        require(quiet.snapshot().timed_out && quiet.error().empty(),
                "quiet stream deadline is reported");
      }
      {
        auto abandoned = tracer.events_async(session);
        wait_for([&] { return service.active_watches == 1; });
      }
      wait_for([&] { return service.active_watches == 0; });
      service.watch_mode = 2;
      {
        auto broken = tracer.events_async(session);
        wait_for([&] { return !broken.running(); });
        require(broken.error().find("background failure") != std::string::npos,
                "worker reports RPC errors without throwing on another input");
      }
      service.watch_mode = 3;
      {
        auto waiting = tracer.events_async(session);
        std::thread cancel([] {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          interrupted = true;
        });
        const bool empty = !(waiting | first());
        cancel.join();
        interrupted = false;
        require(empty && waiting.running(), "Ctrl+C ends a waiting iteration, not the collector");
      }
      wait_for([&] { return service.active_watches == 0; });
      auto quiet = tracer.events_async(session);
      wait_for([&] { return service.active_watches == 1; });
      tracer.detach(session);
      require(!quiet.running(), "detach joins the session's worker");
    }
    {
      Controller background(settings, out);
      auto live = background.events_async(background.attach(7), 0, 0);
      background.shutdown();
      require(!live.running(), "shutdown joins workers while handles survive");
    }
    service.watch_mode = 0;
    server->Shutdown();
    EventBuffer limited(2, 4096);
    for (unsigned i = 0; i < 5; ++i) {
      api::TraceEvent e;
      e.set_sequence(i + 1);
      limited.push(e);
    }
    require(limited.snapshot().size() == 2 && limited.evicted == 3,
            "bounded memory eviction");
    std::ostringstream unknown_phase;
    event_statistics(limited, unknown_phase, true);
    require(unknown_phase.str().find("other_phases=2") != std::string::npos,
            "unspecified native phases remain inspectable without a call pair");
    EventBuffer losses(10, 16384);
    for (unsigned probe = 1; probe <= 2; ++probe) {
      api::TraceEvent event;
      event.set_server_instance_id("server");
      event.set_session_id("session");
      event.set_probe_id(probe);
      event.set_phase(api::PRE);
      event.set_agent_dropped_events(5);
      event.set_server_dropped_events(7);
      losses.push(event);
      losses.push(event);
    }
    std::ostringstream loss_summary;
    event_statistics(losses, loss_summary, false);
    require(
        loss_summary.str().find("lost(agent=10,server=7)") != std::string::npos,
        "per-probe losses are summed without counting repeated samples twice");
    check_trace_statistics();
    EventBuffer too_small(2, 1);
    api::TraceEvent large_event;
    large_event.set_sequence(1);
    too_small.push(large_event);
    require(too_small.evicted == 1 && too_small.events().empty(),
            "oversize event is disclosed without exceeding the byte budget");
    api::TraceEvent contextual;
    auto *envelope = contextual.mutable_context();
    auto *schema = envelope->add_schemas();
    schema->set_epoch_schema_id(1);
    schema->set_root_type("repltest.Counters");
    schema->set_cpp_state_type("Counter");
    google::protobuf::FileDescriptorSet descriptors;
    auto *file = descriptors.add_file();
    file->set_name("repl_state.proto");
    file->set_package("repltest");
    auto *record = file->add_message_type();
    record->set_name("Counters");
    auto *field = record->add_field();
    field->set_name("total");
    field->set_number(1);
    field->set_type(google::protobuf::FieldDescriptorProto::TYPE_INT64);
    field->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
    schema->set_serialized_file_descriptor_set(descriptors.SerializeAsString());
    auto *state = envelope->mutable_event()->add_states();
    state->set_epoch_schema_id(1);
    state->set_payload(std::string("\x08\x2a", 2));
    EventBuffer states(2, 4096);
    states.push(contextual);
    const auto decoded = states.snapshot().at(0).describe();
    require(decoded.find("decoded state Counter") != std::string::npos &&
                decoded.find("42") != std::string::npos,
            "shared state decodes with its attached descriptor");
    char directory[] = "/tmp/tracer-repl-records.XXXXXX";
    require(::mkdtemp(directory), "recording directory");
    const auto path = std::string(directory) + "/events.pb";
    const int fd = create_recording(path);
    auto with_stack = limited.events().front()->message;
    with_stack.set_function_name("event_summary_marker");
    auto *stack = with_stack.mutable_stack();
    stack->set_status(api::StackTrace::PARTIAL);
    stack->set_detail("unwind stopped");
    stack->set_hidden_frames(2);
    for (int i = 0; i < 1025; ++i)
      stack->add_frames()->set_pc(0x1000 + i);
    auto *last = stack->mutable_frames(1024);
    last->set_function("last_frame()");
    last->set_module("/app");
    last->set_module_offset(0x40);
    last->set_file("/app/main.cpp");
    last->set_line(42);
    last->set_stack_pointer(0x8000);
    last->set_application_frame(true);
    write_record(fd, with_stack);
    write_record(fd, limited.events().back()->message);
    ::close(fd);
    auto loaded = open_recording(path, 20, 512 * 1024);
    require(loaded.events().size() == 2 && loaded.complete, "recording replay");
    Controller offline(settings, out);
    offline.execute(parse_command("%events open \"" + path + "\" --as saved"));
    out.str("");
    offline.execute(parse_command("%events stack saved --index 0"));
    const auto printed_stack = out.str();
    require(printed_stack.find("stack PARTIAL frames=1025 hidden=2") == 0 &&
                std::count(printed_stack.begin(), printed_stack.end(), '\n') == 1026 &&
                printed_stack.find("#1024 0x1400 NATIVE function=\"last_frame()\" "
                                   "module=\"/app\"+0x40 file=\"/app/main.cpp\":42") !=
                    std::string::npos &&
                printed_stack.find("event_summary_marker") == std::string::npos,
            "stack view prints every retained frame and stack status");
    out.str("");
    offline.execute(parse_command("%events stack saved --index 1"));
    require(out.str() == "no stack captured\n", "missing stack has a clear response");
    const auto saved = offline.batch("saved");
    const auto frames = saved.at(0).stack();
    require(frames.size() == 1025 && frames.front().pc == 0x1000 &&
                frames.back().pc == 0x1400 && frames.back().stack_pointer == 0x8000 &&
                frames.back().kind == "NATIVE" && frames.back().function == "last_frame()" &&
                frames.back().module == "/app" && frames.back().module_offset == 0x40 &&
                frames.back().file == "/app/main.cpp" && frames.back().line == 42 &&
                frames.back().application_frame,
            "Event::stack exposes every retained frame and its metadata in order");
    require(saved.at(1).stack().empty(), "Event::stack is empty without a stack");
    try {
      create_recording(path);
      throw std::logic_error("overwrote recording");
    } catch (const std::runtime_error &) {
    }
    std::ofstream(path, std::ios::binary | std::ios::app).put('x');
    try {
      offline.execute(
          parse_command("%events open \"" + path + "\" --as saved"));
      throw std::logic_error("overwrote an existing collection");
    } catch (const std::runtime_error &error) {
      require(std::string(error.what()).find("collection already exists") !=
                  std::string::npos,
              "replay requires a new or explicitly cleared collection name");
    }
    try {
      offline.execute(parse_command("%events open \"" + path + "\" --as bad"));
      throw std::logic_error("accepted corrupt replay");
    } catch (const std::runtime_error &) {
    }
    require(offline.batch("saved").size() == 2,
            "failed replay preserves prior captures");
    try {
      open_recording(path, 20, 4096);
      throw std::logic_error("accepted truncated frame");
    } catch (const std::runtime_error &) {
    }
    std::filesystem::remove_all(directory);
    std::cout << "native REPL client checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
