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

using namespace tracer_repl;
namespace api = clangtracer::v1;

namespace {
void require(bool ok, const std::string &why) {
  if (!ok) {
    throw std::runtime_error(why);
  }
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
    if (on_watch)
      on_watch();
    if (q->after_sequence() == 900) {
      while (!c->IsCancelled()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return {grpc::StatusCode::CANCELLED, "cancelled"};
    }
    for (unsigned i = 1; i <= 4; ++i) {
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
      if (!writer->Write(e)) {
        break;
      }
    }
    return grpc::Status::OK;
  }
};
} // namespace

int main(int argc, char **argv) {
  try {
    Service service;
    if (argc == 3 && std::string(argv[1]) == "--listen") {
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
    for (const auto &e : limited.events()) {
      write_record(fd, e->message);
    }
    ::close(fd);
    auto loaded = open_recording(path, 20, 4096);
    require(loaded.events().size() == 2 && loaded.complete, "recording replay");
    Controller offline(settings, out);
    offline.execute(parse_command("%events open \"" + path + "\" --as saved"));
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
