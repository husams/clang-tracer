#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <google/protobuf/text_format.h>
#include <grpcpp/grpcpp.h>

#include "context_query.hpp"
#include "context_receiver.hpp"
#include "context_schema.hpp"
#include "context_transport.pb.h"
#include "debug_resolver.hpp"
#include "header_context.hpp"
#include "sqlite_events.hpp"
#include "tracer.grpc.pb.h"
#include "tracer/compat.hpp"

namespace {

namespace api = clangtracer::v1;
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> interrupted{false};

void on_signal(int) { interrupted.store(true, std::memory_order_relaxed); }

struct Options {
  std::string endpoint;
  std::string request_path;
  std::string output_path;
  std::string command;
  std::string format = "text";
  std::string selector;
  std::string positional_selector;
  std::string compdb;
  std::string module;
  std::string return_type;
  std::string callback;
  std::string context_field;
  std::string sqlite_path;
  bool listen = false;
  std::vector<std::string> argument_types;
  std::vector<std::string> sources;
  std::vector<std::string> compile_options;
  std::string validation_level = "compile";
  std::uint64_t pid = 0;
  std::uint64_t probe_id = 0;
  std::uint64_t count = 0;
  std::uint64_t after = 0;
  std::uint64_t timeout_seconds = 0;
  std::uint64_t debug_max_entries = 0;
  std::uint32_t debug_max_depth = 0;
  unsigned stack_depth = 0;
  bool stack_application_only = true;
  bool stack_application_only_set = false;
  bool callback_print_arguments = false;
  bool callback_print_result = false;
  bool callback_print_set = false;
  bool count_set = false;
  bool after_set = false;
  bool timeout_set = false;
  bool format_set = false;
  bool direct = false;
  bool compdb_set = false;
  bool return_set = false;
  bool validation_level_set = false;
};

std::uint64_t parse_number(const std::string &text, std::string_view name,
                           std::uint64_t maximum, bool allow_zero = false) {
  if (text.empty() ||
      text.find_first_not_of("0123456789") != std::string::npos) {
    throw std::runtime_error("invalid number for " + std::string(name));
  }
  std::size_t consumed = 0;
  const auto value = std::stoull(text, &consumed);
  if (consumed != text.size() || (!allow_zero && value == 0) ||
      value > maximum) {
    throw std::runtime_error(std::string(name) + " must be between " +
                             (allow_zero ? "0" : "1") + " and " +
                             std::to_string(maximum));
  }
  return value;
}

std::string take_value(int &i, int argc, char **argv,
                       const std::string &option) {
  if (++i >= argc) {
    throw std::runtime_error("missing value for " + option);
  }
  return argv[i];
}

Options parse_options(int argc, char **argv) {
  Options o;
  const char *env_endpoint = std::getenv("CLANG_TRACER_ENDPOINT");
  o.endpoint = env_endpoint && *env_endpoint ? env_endpoint : "127.0.0.1:50051";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      std::cout
          << "Usage: clang-tracer [--endpoint HOST:PORT] [--format "
             "protobuf] [--output FILE] "
             "COMMAND\n"
             "  attach PID\n"
             "  trace PID 'C++ declaration' --callback FILE "
             "[--include HEADER ...] [--compdb FILE|DIR] "
             "[--compile-option ARG ...] [--stack DEPTH|full]\n"
             "  resolve PID 'C++ selector' [--module PATH] "
             "[--debug-max-entries N] [--debug-max-depth N] "
             "[--compile-option ARG ...]\n"
             "  validate PID 'C++ declaration' --callback FILE "
             "[--include HEADER ...] [--compdb FILE|DIR] "
             "[--module PATH] [--level syntax|compile] "
             "[--compile-option ARG ...]\n"
             "    --stack-application-only true|false (default: true); false "
             "shows all frames.\n"
             "    --callback-print-arguments true|false (default: false)\n"
             "    --callback-print-result true|false (default: false)\n"
             "    Header flags are auto-detected or inferred by Clang from "
             "the compilation database; target .cpp files are not read.\n"
             "  trace PID (--function NAME|--method NAME) [--return "
             "TYPE [--arg TYPE ...]] --callback FILE\n"
             "  events PID [--count N] [--after N] [--timeout SEC]\n"
             "    --sqlite FILE saves events without consuming history.\n"
             "  drain [PID] --sqlite FILE [--listen] [--count N] [--after N] "
             "[--timeout SEC]\n"
             "    Commit retained events to SQLite, then consume them; "
             "--listen continues until "
             "interrupted.\n"
             "    [--context-field path[,path...]] selects typed context "
             "fields across schema "
             "revisions; omit to show the retained field union.\n"
             "  detach PID | remove PID PROBE_ID | list\n"
             "  CALLBACK FILE may define class Interceptor with optional\n"
             "  pre(const Call&) and post(const Call&) methods; each may\n"
             "  return void or long long. Legacy trace_callback is "
             "entry-only.\n"
             "Legacy: --request FILE [--output FILE] "
             "attach|add|remove|detach|list|watch\n";
      std::exit(0);
    }
    if (a == "--listen") {
      o.listen = true;
    } else if (a == "--sqlite") {
      o.sqlite_path = take_value(i, argc, argv, a);
      if (o.sqlite_path.empty()) {
        throw std::runtime_error("--sqlite requires a filename");
      }
    } else if (a == "--endpoint" || a == "--request" || a == "--output" ||
               a == "--format") {
      const auto v = take_value(i, argc, argv, a);
      if (a == "--endpoint") {
        o.endpoint = v;
      } else if (a == "--request") {
        o.request_path = v;
      } else if (a == "--output") {
        o.output_path = v;
      } else {
        o.format = v;
        o.format_set = true;
      }
    } else if (a == "--function" || a == "--method" || a == "--return" ||
               a == "--arg" || a == "--callback" || a == "--count" ||
               a == "--after" || a == "--timeout" || a == "--include" ||
               a == "--compdb" || a == "--stack" ||
               a == "--compile-option" || a == "--level" ||
               a == "--module" || a == "--debug-max-entries" ||
               a == "--debug-max-depth" ||
               a == "--stack-application-only" ||
               a == "--callback-print-arguments" ||
               a == "--callback-print-result" || a == "--context-field") {
      if (a != "--count" && a != "--timeout") {
        o.direct = true;
      }
      const auto v = take_value(i, argc, argv, a);
      if (a == "--function" || a == "--method") {
        if (!o.selector.empty()) {
          throw std::runtime_error("choose only one of --function or --method");
        }
        o.selector = a + " " + v;
      } else if (a == "--return") {
        o.return_type = v;
        o.return_set = true;
      } else if (a == "--arg") {
        o.argument_types.push_back(v);
      } else if (a == "--callback") {
        o.callback = v;
      } else if (a == "--stack") {
        o.stack_depth = v == "full" ? UINT32_MAX
                                    : static_cast<unsigned>(parse_number(v, a, UINT32_MAX));
      } else if (a == "--stack-application-only") {
        if (v != "true" && v != "false")
          throw std::runtime_error("--stack-application-only must be true or false");
        o.stack_application_only = v == "true";
        o.stack_application_only_set = true;
      } else if (a == "--callback-print-arguments" || a == "--callback-print-result") {
        if (v != "true" && v != "false")
          throw std::runtime_error(a + " must be true or false");
        if (a == "--callback-print-arguments") o.callback_print_arguments = v == "true";
        else o.callback_print_result = v == "true";
        o.callback_print_set = true;
      } else if (a == "--context-field") {
        if (v.empty() || v.size() > 4096)
          throw std::runtime_error("--context-field must contain paths and be at most 4096 bytes");
        if (v.back() == ',')
          throw std::runtime_error("--context-field contains an empty path");
        if (!o.context_field.empty())
          o.context_field.push_back(',');
        o.context_field += v;
      } else if (a == "--count") {
        o.count = parse_number(v, a, 1'000'000);
        o.count_set = true;
      } else if (a == "--after") {
        o.after =
            parse_number(v, a, std::numeric_limits<std::uint64_t>::max(), true);
        o.after_set = true;
      } else if (a == "--timeout") {
        o.timeout_seconds = parse_number(v, a, 86'400);
        o.timeout_set = true;
      } else if (a == "--compdb") {
        if (o.compdb_set || v.empty())
          throw std::runtime_error("provide one nonempty --compdb FILE|DIR");
        o.compdb = v;
        o.compdb_set = true;
      } else if (a == "--compile-option") {
        o.compile_options.push_back(v);
      } else if (a == "--level") {
        if (v != "syntax" && v != "compile")
          throw std::runtime_error("--level must be syntax or compile");
        o.validation_level = v;
        o.validation_level_set = true;
      } else if (a == "--module") {
        o.module = v;
      } else if (a == "--debug-max-entries") {
        o.debug_max_entries = parse_number(v, a, std::numeric_limits<std::uint64_t>::max(), true);
      } else if (a == "--debug-max-depth") {
        o.debug_max_depth = static_cast<std::uint32_t>(
            parse_number(v, a, std::numeric_limits<std::uint32_t>::max(), true));
      } else {
        o.sources.push_back(v);
      }
    } else if (::tracer::compat::starts_with(a, "--")) {
      throw std::runtime_error("unknown option: " + a);
    } else if (o.command.empty()) {
      o.command = a;
    } else if ((o.command == "attach" || o.command == "trace" ||
                o.command == "validate" || o.command == "resolve" ||
                o.command == "events" || o.command == "drain" ||
                o.command == "detach" || o.command == "remove") &&
               o.pid == 0) {
      o.direct = true;
      o.pid = parse_number(a, "PID", std::numeric_limits<std::uint32_t>::max());
    } else if ((o.command == "trace" || o.command == "validate" ||
                o.command == "resolve") &&
               o.pid != 0 && o.positional_selector.empty()) {
      o.direct = true;
      o.positional_selector = a;
    } else if (o.command == "remove" && o.probe_id == 0) {
      o.direct = true;
      o.probe_id = parse_number(a, "PROBE_ID",
                                std::numeric_limits<std::uint64_t>::max());
    } else {
      throw std::runtime_error("unexpected argument: " + a);
    }
  }
  if (o.endpoint.empty()) {
    throw std::runtime_error("--endpoint must not be empty");
  }
  if (o.format != "text" && o.format != "protobuf") {
    throw std::runtime_error("--format must be text or protobuf");
  }
  if (!o.request_path.empty()) {
    if (!o.sqlite_path.empty() || o.listen) {
      throw std::runtime_error("SQLite and listen require direct commands");
    }
    if (o.direct) {
      throw std::runtime_error(
          "--request cannot be combined with direct command arguments");
    }
    if (o.command != "attach" && o.command != "add" && o.command != "remove" &&
        o.command != "detach" && o.command != "list" && o.command != "watch") {
      throw std::runtime_error("unknown legacy command: " + o.command);
    }
    if (o.format_set) {
      throw std::runtime_error("--format applies only to direct commands; "
                               "legacy requests always write protobuf frames");
    }
    if (o.command == "watch" && !o.count_set) {
      throw std::runtime_error("watch requires --count N");
    }
    if (o.command != "watch" && o.count_set) {
      throw std::runtime_error(
          "--count is only valid for legacy watch or direct events");
    }
    if (!o.timeout_set) {
      o.timeout_seconds = 30;
    }
    return o;
  }
  o.direct = true;
  if (o.command == "trace") {
    if (o.compdb_set && o.sources.empty())
      throw std::runtime_error("--compdb requires --include HEADER");
    if (o.pid == 0 || o.callback.empty() ||
        (o.selector.empty() && o.positional_selector.empty())) {
      throw std::runtime_error("trace requires PID, a C++ declaration or "
                               "--function/--method, and --callback FILE");
    }
    if (!o.selector.empty() && !o.positional_selector.empty()) {
      throw std::runtime_error("choose either a positional C++ declaration or "
                               "--function/--method");
    }
    if (o.argument_types.size() > 8) {
      throw std::runtime_error("trace accepts at most eight --arg types");
    }
    if (!o.return_set && !o.argument_types.empty()) {
      throw std::runtime_error("--arg requires --return in raw-symbol mode");
    }
    if (!o.return_set && o.selector.empty() && o.positional_selector.empty()) {
      throw std::runtime_error("trace requires a C++ declaration");
    }
    if (o.return_set && o.positional_selector.size()) {
      throw std::runtime_error("positional C++ declarations cannot be combined "
                               "with --return or --arg");
    }
  } else if (o.command == "validate") {
    if (o.pid == 0 || o.callback.empty() || o.positional_selector.empty())
      throw std::runtime_error("validate requires PID, a C++ selector, and --callback FILE");
    if (o.compdb_set && o.sources.empty())
      throw std::runtime_error("--compdb requires --include HEADER");
    if (o.return_set || !o.argument_types.empty() || o.stack_depth ||
        o.stack_application_only_set || o.callback_print_set) {
      throw std::runtime_error(
          "--return, --arg, and trace display options are only valid for trace");
    }
  } else if (o.command == "resolve") {
    if (o.pid == 0 || o.positional_selector.empty())
      throw std::runtime_error("resolve requires PID and a C++ selector");
    if (o.compdb_set && o.sources.empty())
      throw std::runtime_error("--compdb requires --include HEADER");
    if (!o.callback.empty() || o.return_set || !o.argument_types.empty() ||
        o.stack_depth || o.stack_application_only_set || o.callback_print_set) {
      throw std::runtime_error(
          "callback, ABI, and trace display options are not valid for resolve");
    }
  } else if (o.command == "attach" || o.command == "events" ||
             o.command == "detach") {
    if (o.pid == 0) {
      throw std::runtime_error(o.command + " requires PID");
    }
  } else if (o.command == "remove") {
    if (o.pid == 0 || o.probe_id == 0) {
      throw std::runtime_error("remove requires PID and PROBE_ID");
    }
  } else if (o.command != "list" && o.command != "drain") {
    throw std::runtime_error("unknown command: " + o.command);
  }
  if (o.validation_level_set && o.command != "validate")
    throw std::runtime_error("--level is only valid for validate");
  if (o.command != "events" && o.command != "drain" &&
      (o.count_set || o.after_set)) {
    throw std::runtime_error(
        "--count and --after are only valid for events or drain");
  }
  if (o.listen && o.command != "drain") {
    throw std::runtime_error("--listen is only valid for drain");
  }
  if (o.command == "drain" && o.sqlite_path.empty()) {
    throw std::runtime_error("drain requires --sqlite FILE");
  }
  if (!o.sqlite_path.empty() &&
      (o.command != "events" && o.command != "drain")) {
    throw std::runtime_error("--sqlite is only valid for events or drain");
  }
  if (!o.sqlite_path.empty() && (o.format_set || !o.output_path.empty())) {
    throw std::runtime_error(
        "--sqlite cannot be combined with --format or --output");
  }
  const bool has_trace_args =
      !o.selector.empty() || !o.return_type.empty() ||
      !o.argument_types.empty() || !o.callback.empty() ||
      !o.positional_selector.empty() || o.compdb_set || !o.sources.empty() || o.stack_depth ||
      !o.module.empty() || o.debug_max_entries || o.debug_max_depth ||
      !o.compile_options.empty() ||
      o.stack_application_only_set || o.callback_print_set;
  if (o.command != "trace" && o.command != "validate" && o.command != "resolve" && has_trace_args) {
    throw std::runtime_error("--function, --method, --return, --arg, and "
                             "--callback are only valid for trace");
  }
  return o;
}

api::ValueType parse_type(std::string s, bool allow_void) {
  for (char &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (s == "int32") {
    return api::INT32;
  }
  if (s == "bool") {
    return api::BOOL;
  }
  if (s == "int64") {
    return api::INT64;
  }
  if (s == "uint64") {
    return api::UINT64;
  }
  if (s == "float32") {
    return api::FLOAT32;
  }
  if (s == "float64") {
    return api::FLOAT64;
  }
  if (s == "pointer") {
    return api::POINTER;
  }
  if (s == "void" && allow_void) {
    return api::VOID;
  }
  throw std::runtime_error(
      "invalid type '" + s +
      "' (expected bool, int32, int64, uint64, float32, float64, pointer" +
      (allow_void ? ", or void" : "") + ")");
}

template <class Message> Message read_textproto(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open request file: " + path);
  }
  std::string text((std::istreambuf_iterator<char>(in)), {});
  Message message;
  if (!google::protobuf::TextFormat::ParseFromString(text, &message)) {
    throw std::runtime_error("request is not valid protobuf text format: " +
                             path);
  }
  return message;
}

class FrameWriter {
public:
  explicit FrameWriter(const std::string &path) {
    if (path.empty() || path == "-") {
      stream_ = &std::cout;
    } else {
      file_.open(path, std::ios::binary | std::ios::trunc);
      if (!file_) {
        throw std::runtime_error("cannot open output file: " + path);
      }
      stream_ = &file_;
    }
  }

  template <class Message> void write(const Message &message) {
    std::string payload;
    if (!message.SerializeToString(&payload) ||
        payload.size() > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("could not serialize protobuf response");
    }
    const auto n = static_cast<std::uint32_t>(payload.size());
    const std::array<char, 4> header = {
        static_cast<char>(n), static_cast<char>(n >> 8U),
        static_cast<char>(n >> 16U), static_cast<char>(n >> 24U)};
    stream_->write(header.data(), 4);
    stream_->write(payload.data(),
                   static_cast<std::streamsize>(payload.size()));
    stream_->flush();
    if (!*stream_) {
      throw std::runtime_error("failed to write protobuf response frame");
    }
  }

private:
  std::ofstream file_;
  std::ostream *stream_ = nullptr;
};

class TextWriter {
public:
  explicit TextWriter(const std::string &path) {
    if (path.empty() || path == "-") {
      out_ = &std::cout;
    } else {
      file_.open(path);
      if (!file_) {
        throw std::runtime_error("cannot open output file: " + path);
      }
      out_ = &file_;
    }
  }

  template <class... T> void line(T &&...parts) {
    ((*out_) << ... << std::forward<T>(parts)) << '\n';
    out_->flush();
    if (!*out_) {
      throw std::runtime_error("failed to write output");
    }
  }

private:
  std::ofstream file_;
  std::ostream *out_ = nullptr;
};

void require_ok(const grpc::Status &s, std::string_view op) {
  if (!s.ok()) {
    throw std::runtime_error(std::string(op) + " failed (gRPC code " +
                             std::to_string(static_cast<int>(s.error_code())) +
                             "): " + s.error_message());
  }
}

void set_deadline(grpc::ClientContext &c, std::uint64_t secs) {
  c.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(secs));
}

std::optional<api::Session>
find_attached_session(api::Tracer::Stub &stub, std::uint32_t pid,
                      std::uint64_t timeout_seconds) {
  api::ListSessionsRequest req;
  api::ListSessionsResponse res;
  grpc::ClientContext ctx;
  set_deadline(ctx, timeout_seconds);
  require_ok(stub.ListSessions(&ctx, req, &res), "ListSessions");
  std::optional<api::Session> found;
  for (const auto &s : res.sessions()) {
    if (s.pid() == pid && s.state() == api::Session::ATTACHED) {
      if (found) {
        throw std::runtime_error("multiple attached sessions match PID " +
                                 std::to_string(pid));
      }
      found = s;
    }
  }
  return found;
}

api::Session find_session(api::Tracer::Stub &stub, std::uint32_t pid,
                          std::uint64_t timeout_seconds) {
  auto found = find_attached_session(stub, pid, timeout_seconds);
  if (!found) {
    throw std::runtime_error("no attached session found for PID " +
                             std::to_string(pid));
  }
  return std::move(*found);
}

const char *state_name(api::Session::State state) {
  switch (state) {
  case api::Session::ATTACHED:
    return "ATTACHED";
  case api::Session::DETACHED:
    return "DETACHED";
  case api::Session::EXITED:
    return "EXITED";
  case api::Session::FAILED:
    return "FAILED";
  default:
    return "UNKNOWN";
  }
}

void print_session(TextWriter &out, const api::Session &s,
                   std::string_view operation) {
  out.line(operation, " pid=", s.pid(), " session=", s.id(),
           " state=", state_name(s.state()));
}

void print_detached(TextWriter &out, const api::Session &s) {
  std::string probes;
  for (const auto &p : s.probes()) {
    if (!probes.empty()) {
      probes += ",";
    }
    probes += std::to_string(p.id()) +
              ":restored=" + (p.original_bytes_restored() ? "true" : "false");
  }
  out.line("detached pid=", s.pid(), " session=", s.id(),
           " state=", state_name(s.state()), " probes=[", probes, "]");
}

std::string value_text(const api::Value &v) {
  const char *type = "unknown";
  switch (v.type()) {
  case api::VOID:
    return "void";
  case api::INT32:
    type = "int32";
    return std::string(type) + ":" + std::to_string(v.signed_value());
  case api::BOOL:
    return std::string("bool:") + (v.signed_value() ? "true" : "false");
  case api::INT64:
    type = "int64";
    return std::string(type) + ":" + std::to_string(v.signed_value());
  case api::UINT64:
    type = "uint64";
    return std::string(type) + ":" + std::to_string(v.unsigned_value());
  case api::FLOAT32:
  case api::FLOAT64: {
    std::ostringstream value;
    const bool single = v.type() == api::FLOAT32;
    value << (single ? "float32:" : "float64:")
          << std::setprecision(single
                                   ? std::numeric_limits<float>::max_digits10
                                   : std::numeric_limits<double>::max_digits10)
          << v.real_value();
    return value.str();
  }
  case api::OBJECT:
  case api::POINTER:
    return std::string(v.type() == api::OBJECT ? "object:0x" : "pointer:0x") +
           [&] {
             char buf[32];
             std::snprintf(buf, sizeof(buf), "%llx",
                           static_cast<unsigned long long>(v.unsigned_value()));
             return std::string(buf);
           }();
  default:
    return std::string(type) + ":?";
  }
}

class ContextEventDecoder final {
  struct Epoch final {
    std::unique_ptr<tracer::context_engine::ContextReceiver> receiver;
    std::unordered_map<std::uint64_t, std::string> schema_fingerprints;
    std::size_t fingerprint_bytes = 0;
    std::string producer_session_id;
  };

public:
  ContextEventDecoder() : ledger_(std::make_shared<tracer::context_engine::ReceiverLedger>()) {}

  std::optional<tracer::context_engine::DecodedEvent> decode(
      const clangtracer::context::v1::ContextEventEnvelope &envelope,
      std::string &error) {
    using namespace clangtracer::context::v1;
    const std::string key = envelope.producer_session_id() +
                            std::string("\0", 1) +
                            std::to_string(envelope.epoch_id());
    auto found = epochs_.find(key);
    if (found == epochs_.end()) {
      if (epochs_.size() == 32) {
        fingerprint_bytes_ -= epochs_.begin()->second.fingerprint_bytes;
        epochs_.begin()->second.receiver->close_epoch();
        epochs_.erase(epochs_.begin());
        ++schema_generation_;
        field_path_cache_key_.clear();
      }
      Epoch value;
      value.receiver = std::make_unique<tracer::context_engine::ContextReceiver>(ledger_);
      value.producer_session_id = envelope.producer_session_id();
      found = epochs_.emplace(key, std::move(value)).first;
    }
    auto &epoch = found->second;
    std::vector<tracer::context_engine::SchemaRegistration> registrations;
    std::unordered_map<std::uint64_t, std::string> new_fingerprints;
    std::set<std::uint64_t> ids;
    for (const auto &schema : envelope.schemas()) {
      if (schema.epoch_schema_id() == 0 || !ids.insert(schema.epoch_schema_id()).second) {
        error = "context envelope contains a duplicate or zero schema ID";
        return std::nullopt;
      }
      google::protobuf::FileDescriptorSet descriptors;
      if (!descriptors.ParseFromString(schema.serialized_file_descriptor_set())) {
        error = "context envelope has an invalid descriptor set";
        return std::nullopt;
      }
      std::string canonical;
      if (!descriptors.SerializeToString(&canonical) ||
          canonical != schema.serialized_file_descriptor_set() ||
          schema.source_lineage_id().size() != 16) {
        error = "context envelope schema metadata is not canonical";
        return std::nullopt;
      }
      tracer::context_engine::StateFamilyKey family;
      std::copy(schema.source_lineage_id().begin(),
                schema.source_lineage_id().end(),
                family.source_lineage_id.begin());
      family.cpp_state_type = schema.cpp_state_type();
      family.cpp_scope_type = schema.cpp_scope_type();
      const std::vector<std::string> roles(schema.container_roles().begin(),
                                           schema.container_roles().end());
      const auto identity = tracer::context_engine::compute_schema_revision_id(
          canonical, family, schema.root_type(), roles);
      if (schema.schema_identity() != identity ||
          schema.revision_id() != identity) {
        error = "context envelope schema identity does not match its descriptor revision";
        return std::nullopt;
      }
      const auto wire = schema.SerializeAsString();
      const auto existing = epoch.schema_fingerprints.find(schema.epoch_schema_id());
      if (existing != epoch.schema_fingerprints.end()) {
        if (existing->second != wire) {
          error = "context envelope redefined a schema ID within an epoch";
          return std::nullopt;
        }
        continue;
      }
      tracer::context_engine::SchemaRegistration registration;
      registration.id = schema.epoch_schema_id();
      registration.descriptor_set = std::move(descriptors);
      registration.root = schema.root_type();
      std::copy(schema.source_lineage_id().begin(), schema.source_lineage_id().end(),
                registration.family.source_lineage_id.begin());
      registration.family.cpp_state_type = schema.cpp_state_type();
      registration.family.cpp_scope_type = schema.cpp_scope_type();
      registration.container_roles.assign(schema.container_roles().begin(),
                                           schema.container_roles().end());
      registrations.push_back(std::move(registration));
      new_fingerprints.emplace(schema.epoch_schema_id(), wire);
    }
    std::size_t additional_fingerprint_bytes = 0;
    for (const auto &[id, wire] : new_fingerprints) {
      (void)id;
      additional_fingerprint_bytes += wire.size();
    }
    constexpr std::size_t kFingerprintCacheBytes = 64 * 1024 * 1024;
    while (fingerprint_bytes_ + additional_fingerprint_bytes >
               kFingerprintCacheBytes && epochs_.size() > 1) {
      auto victim = epochs_.begin();
      if (victim == found)
        ++victim;
      if (victim == epochs_.end())
        break;
      fingerprint_bytes_ -= victim->second.fingerprint_bytes;
      victim->second.receiver->close_epoch();
      epochs_.erase(victim);
      ++schema_generation_;
      field_path_cache_key_.clear();
    }
    if (fingerprint_bytes_ + additional_fingerprint_bytes >
        kFingerprintCacheBytes) {
      error = "context schema fingerprint cache is full";
      return std::nullopt;
    }
    if (!registrations.empty() &&
        !epoch.receiver->register_schema_set(registrations, &error))
      return std::nullopt;
    for (auto &[id, fingerprint] : new_fingerprints) {
      epoch.fingerprint_bytes += fingerprint.size();
      fingerprint_bytes_ += fingerprint.size();
      epoch.schema_fingerprints.emplace(id, std::move(fingerprint));
    }
    if (!new_fingerprints.empty()) {
      ++schema_generation_;
      field_path_cache_.clear();
    }

    const auto &event = envelope.event();
    std::vector<std::vector<tracer::context_engine::CapturePathStep>> paths;
    std::vector<std::vector<tracer::context_engine::CaptureLimitView>> limits;
    std::vector<tracer::context_engine::StateAttachmentView> attachments;
    std::size_t path_count = 0;
    for (const auto &state : event.states())
      path_count += static_cast<std::size_t>(state.truncations_size());
    if (path_count > 4096 || event.states_size() > 256) {
      error = "context event exceeds receiver attachment limits";
      return std::nullopt;
    }
    paths.reserve(path_count);
    limits.reserve(event.states_size());
    attachments.reserve(event.states_size());
    for (const auto &state : event.states()) {
      auto &state_limits = limits.emplace_back();
      state_limits.reserve(state.truncations_size());
      for (const auto &truncation : state.truncations()) {
        auto &path = paths.emplace_back();
        path.reserve(truncation.path_size());
        for (const auto &step : truncation.path()) {
          tracer::context_engine::CapturePathStep item;
          switch (step.value_case()) {
          case TruncationPathStep::kFieldNumber:
            item.selector = tracer::context_engine::CapturePathSelector::field_number;
            item.number = step.field_number(); break;
          case TruncationPathStep::kListIndex:
            item.selector = tracer::context_engine::CapturePathSelector::list_index;
            item.number = step.list_index(); break;
          case TruncationPathStep::kStringKey:
            item.selector = tracer::context_engine::CapturePathSelector::string_key;
            item.string_value = step.string_key(); break;
          case TruncationPathStep::kSignedKey:
            item.selector = tracer::context_engine::CapturePathSelector::signed_key;
            item.signed_value = step.signed_key(); break;
          case TruncationPathStep::kUnsignedKey:
            item.selector = tracer::context_engine::CapturePathSelector::unsigned_key;
            item.number = step.unsigned_key(); break;
          case TruncationPathStep::kBoolKey:
            item.selector = tracer::context_engine::CapturePathSelector::bool_key;
            item.bool_value = step.bool_key(); break;
          case TruncationPathStep::VALUE_NOT_SET:
            error = "context event has an empty truncation path step";
            return std::nullopt;
          }
          path.push_back(std::move(item));
        }
        state_limits.push_back({path, truncation.total_items(),
                                truncation.captured_items()});
      }
      const auto *bytes = reinterpret_cast<const std::byte *>(state.payload().data());
      attachments.push_back({state.epoch_schema_id(),
                             ::tracer::compat::span<const std::byte>(bytes, state.payload().size()),
                             state_limits});
    }
    return epoch.receiver->decode_event(attachments, &error);
  }

  const std::set<std::string> &field_paths(
      const tracer::context_engine::StateFamilyKey &family) {
    const std::string key(
        reinterpret_cast<const char *>(family.source_lineage_id.data()),
        family.source_lineage_id.size());
    std::string family_key = key;
    family_key.push_back('\0');
    family_key += family.cpp_state_type;
    family_key.push_back('\0');
    family_key += family.cpp_scope_type;
    if (family_key == field_path_cache_key_ &&
        field_path_cache_generation_ == schema_generation_)
      return field_path_cache_;
    field_path_cache_key_ = std::move(family_key);
    field_path_cache_generation_ = schema_generation_;
    field_path_cache_.clear();
    field_path_cache_truncated_ = false;
    for (const auto &[epoch_key, epoch] : epochs_) {
      (void)epoch_key;
      for (const auto &schema : epoch.receiver->admitted_schemas()) {
        if (!(schema.family() == family))
          continue;
        auto discovered = tracer::context_query::discover_field_paths(schema);
        field_path_cache_truncated_ |= discovered.truncated;
        for (auto &path : discovered.paths) {
          if (field_path_cache_.size() >=
              tracer::context_query::kDefaultFieldPathLimit) {
            field_path_cache_truncated_ = true;
            break;
          }
          field_path_cache_.insert(std::move(path));
        }
        if (field_path_cache_.size() >=
            tracer::context_query::kDefaultFieldPathLimit) {
          field_path_cache_truncated_ = true;
          break;
        }
      }
      if (field_path_cache_.size() >=
          tracer::context_query::kDefaultFieldPathLimit) {
        field_path_cache_truncated_ = true;
        break;
      }
    }
    return field_path_cache_;
  }

  bool field_paths_truncated() const noexcept {
    return field_path_cache_truncated_;
  }

  std::string query_field(const google::protobuf::Message &message,
                          const tracer::context_engine::SchemaRecord &record,
                          std::string_view path) const {
    for (const auto &[epoch_key, epoch] : epochs_) {
      (void)epoch_key;
      for (const auto &schema : epoch.receiver->admitted_schemas()) {
        if (schema.revision_id() == record.revision_id &&
            schema.family() == record.family)
          return tracer::context_query::query_field(message, schema, path);
      }
    }
    return "<missing>";
  }

private:
  std::shared_ptr<tracer::context_engine::ReceiverLedger> ledger_;
  std::map<std::string, Epoch> epochs_;
  std::size_t fingerprint_bytes_ = 0;
  std::uint64_t schema_generation_ = 1;
  std::string field_path_cache_key_;
  std::uint64_t field_path_cache_generation_ = 0;
  std::set<std::string> field_path_cache_;
  bool field_path_cache_truncated_ = false;
};

std::string family_label(const tracer::context_engine::SchemaRecord &schema) {
  std::ostringstream lineage;
  lineage << std::hex << std::setfill('0');
  for (const auto byte : schema.family.source_lineage_id)
    lineage << std::setw(2) << static_cast<unsigned>(byte);
  return schema.family.cpp_state_type + "@" + lineage.str();
}

std::vector<std::string> selected_field_paths(std::string_view requested) {
  std::vector<std::string> paths;
  std::size_t begin = 0;
  while (begin < requested.size()) {
    const auto comma = requested.find(',', begin);
    const auto end = comma == std::string_view::npos ? requested.size() : comma;
    const auto path = requested.substr(begin, end - begin);
    if (path.empty() || path.size() > 256 || paths.size() >= 256)
      throw std::runtime_error("invalid or excessive --context-field paths");
    paths.emplace_back(path);
    if (comma == std::string_view::npos)
      break;
    begin = comma + 1;
  }
  return paths;
}

void print_context_event(TextWriter &out, const api::TraceEvent &e,
                         const Options &options, ContextEventDecoder &decoder) {
  const auto &envelope = e.context();
  out.line("  shared-context event=", envelope.event().name(),
           " epoch=", envelope.epoch_id(), " states=", envelope.event().states_size());
  std::string error;
  auto decoded = decoder.decode(envelope, error);
  if (!decoded) {
    out.line("    context decode failed: ", error);
    return;
  }
  const auto selected = selected_field_paths(options.context_field);
  bool reported_path_truncation = false;
  for (const auto &state : decoded->states) {
    const auto *message = state.message();
    if (!message)
      continue;
    const auto print_field = [&](std::string_view path) {
      out.line("    ", family_label(*state.schema()), ".", path,
               " [revision=", state.schema()->revision_id,
               "] =", decoder.query_field(*message, *state.schema(), path));
    };
    if (options.context_field.empty()) {
      const auto &paths = decoder.field_paths(state.schema()->family);
      if (decoder.field_paths_truncated() && !reported_path_truncation) {
        out.line("    field-path discovery truncated at ",
                 tracer::context_query::kDefaultFieldPathLimit, " paths");
        reported_path_truncation = true;
      }
      for (const auto &path : paths)
        print_field(path);
    } else {
      for (const auto &path : selected)
        print_field(path);
    }
  }
}

void print_event(TextWriter &out, const api::TraceEvent &e,
                 const Options &options, ContextEventDecoder &decoder) {
  const bool show_callback_arguments =
      !e.has_context() || e.context().event().print_arguments();
  const bool show_callback_result =
      !e.has_context() || e.context().event().print_result();
  const char *phase = "unspecified";
  if (e.phase() == api::PRE) {
    phase = "pre";
  } else if (e.phase() == api::POST) {
    phase = "post";
  }
  out.line(
      "cursor=", e.sequence(), " pid=", e.pid(), " tid=",
      e.has_context() ? "-" : std::to_string(e.tid()),
      " probe=", e.probe_id(), " this=",
      e.object_address() ? "0x" +
                               [&] {
                                 char b[32];
                                 std::snprintf(b, sizeof(b), "%llx",
                                               static_cast<unsigned long long>(
                                                   e.object_address()));
                                 return std::string(b);
                               }()
                         : "-",
      " args=[",
      [&] {
        std::string s;
        if (!show_callback_arguments)
          return s;
        for (int i = 0; i < e.arguments_size(); ++i) {
          if (i) {
            s += ", ";
          }
          const api::NamedValue *named = nullptr;
          for (const auto &item : e.named_arguments()) {
            if (item.argument_index() == static_cast<unsigned>(i))
              named = &item;
          }
          if (named)
            s += named->name() + "=";
          s += value_text(e.arguments(i));
        }
        return s;
      }(),
      "]",
      [&] {
        if (e.object_fields().empty() || !show_callback_arguments)
          return std::string{};
        std::string s = " fields=[";
        for (const auto &field : e.object_fields()) {
          if (s.back() != '[')
            s += ", ";
          s += field.name() + "=" + value_text(field.value());
        }
        return s + "]";
      }(),
      !e.has_context() && e.named_arguments().empty() && e.object_fields().empty()
          ? " callback=" + std::to_string(e.callback_value())
          : "",
      " phase=", phase, " call=", e.call_id(),
      e.phase() == api::POST && e.has_result() && show_callback_result
          ? " result=" + value_text(e.result())
          : "",
      " lost(agent=", e.agent_dropped_events(),
      ",server=", e.server_dropped_events(), ")");
  if (e.has_stack()) {
    const auto &stack = e.stack();
    out.line("  entry-stack ", api::StackTrace::Status_Name(stack.status()),
             " depth=", stack.frames_size(), "/",
             stack.requested_depth() == UINT32_MAX
                 ? "full" : std::to_string(stack.requested_depth()),
             " application-only=", stack.application_only() ? "true" : "false",
             " captured=", stack.captured_depth(), " hidden=", stack.hidden_frames(),
             " (", stack.detail(), ")");
    for (int i = 0; i < stack.frames_size(); ++i) {
      const auto &frame = stack.frames(i);
      std::ostringstream address;
      address << "0x" << std::hex << frame.pc();
      std::ostringstream offset;
      offset << "0x" << std::hex << frame.module_offset();
      out.line("    #", i, " ", api::StackFrame::Kind_Name(frame.kind()),
               " ", address.str(), " ", frame.function().empty() ? "??" : frame.function(),
               frame.module().empty() ? "" : " [" + frame.module() + "+" + offset.str() + "]",
               frame.file().empty() ? "" : " " + frame.file() + ":" + std::to_string(frame.line()));
    }
  }
  if (e.has_context())
    print_context_event(out, e, options, decoder);
}

class SignalGuard {
public:
  SignalGuard() {
    interrupted.store(false, std::memory_order_relaxed);
    old_int_ = std::signal(SIGINT, on_signal);
    old_term_ = std::signal(SIGTERM, on_signal);
  }

  ~SignalGuard() {
    std::signal(SIGINT, old_int_);
    std::signal(SIGTERM, old_term_);
  }

private:
  using Handler = void (*)(int);
  Handler old_int_ = SIG_DFL;
  Handler old_term_ = SIG_DFL;
};

class StreamCanceller {
public:
  explicit StreamCanceller(grpc::ClientContext &context)
      : context_(context), worker_([this] {
          while (!finished_.load()) {
            if (interrupted.load(std::memory_order_relaxed)) {
              context_.TryCancel();
              return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
        }) {}

  ~StreamCanceller() {
    if (worker_.joinable()) {
      context_.TryCancel();
    }
    stop();
  }

  void stop() {
    finished_ = true;
    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  grpc::ClientContext &context_;
  std::atomic<bool> finished_{false};
  std::thread worker_;
};

void set_callback_package_location(api::ProbeSpec &spec,
                                   const Options &options) {
  const auto callback = std::filesystem::canonical(options.callback);
  spec.set_callback_package_path(callback.string());
}

int run_drain(const Options &o, api::Tracer::Stub &stub) {
  tracer::client::SQLiteEvents storage(o.sqlite_path);
  SignalGuard signals;
  const auto deadline = o.timeout_set
                            ? std::chrono::system_clock::now() +
                                  std::chrono::seconds(o.timeout_seconds)
                            : std::chrono::system_clock::time_point::max();
  std::string instance;
  std::uint64_t through = 0, received = 0, reported_drops = 0;
  while (!interrupted.load(std::memory_order_relaxed) &&
         std::chrono::system_clock::now() < deadline) {
    api::ReadEventsRequest request;
    request.mutable_filter()->set_include_replay_types(true);
    request.mutable_filter()->set_pid(static_cast<std::uint32_t>(o.pid));
    request.mutable_filter()->set_after_sequence(o.after);
    request.set_limit(static_cast<std::uint32_t>(
        o.count_set ? std::min<std::uint64_t>(256, o.count - received) : 256));
    request.set_through_sequence(through);
    request.set_server_instance_id(instance);
    api::ReadEventsResponse batch;
    grpc::ClientContext read_context;
    read_context.set_deadline(std::min(
        deadline, std::chrono::system_clock::now() + std::chrono::seconds(30)));
    StreamCanceller read_canceller(read_context);
    const auto read_status = stub.ReadEvents(&read_context, request, &batch);
    read_canceller.stop();
    if (interrupted.load(std::memory_order_relaxed)) {
      break;
    }
    require_ok(read_status, "ReadEvents");
    if (instance.empty()) {
      instance = batch.server_instance_id();
    }
    if (instance.empty() || instance != batch.server_instance_id()) {
      throw std::runtime_error("snapshot server instance changed or missing");
    }
    if (through == 0) {
      through = batch.through_sequence();
    }
    if (batch.server_dropped_events() > reported_drops) {
      reported_drops = batch.server_dropped_events();
      std::cerr << "warning: server reports " << reported_drops
                << " history drops\n";
    }
    if (batch.events().empty()) {
      if (!o.listen) {
        break;
      }
      through = 0;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }
    api::AcknowledgeEventsRequest acknowledgement;
    acknowledgement.set_server_instance_id(instance);
    for (const auto &event : batch.events()) {
      if (event.server_instance_id() != instance) {
        throw std::runtime_error("event server instance differs from snapshot");
      }
      acknowledgement.add_sequences(event.sequence());
    }
    storage.write(batch.events());
    api::AcknowledgeEventsResponse result;
    grpc::ClientContext ack_context;
    ack_context.set_deadline(std::min(
        deadline, std::chrono::system_clock::now() + std::chrono::seconds(30)));
    StreamCanceller ack_canceller(ack_context);
    const auto ack_status =
        stub.AcknowledgeEvents(&ack_context, acknowledgement, &result);
    ack_canceller.stop();
    if (interrupted.load(std::memory_order_relaxed)) {
      break;
    }
    require_ok(
        ack_status,
        "AcknowledgeEvents (batch committed; retry with the same SQLite file)");
    received += static_cast<std::uint64_t>(batch.events_size());
    if (o.count_set && received >= o.count) {
      break;
    }
  }
  std::cout << "drained " << received << " events to " << o.sqlite_path << '\n';
  return 0;
}

int run_direct(const Options &o, api::Tracer::Stub &stub) {
  if (o.command == "drain") {
    return run_drain(o, stub);
  }
  std::unique_ptr<tracer::client::SQLiteEvents> storage;
  if (!o.sqlite_path.empty()) {
    storage = std::make_unique<tracer::client::SQLiteEvents>(o.sqlite_path);
  }
  TextWriter text(o.format == "protobuf" ? std::string{} : o.output_path);
  std::unique_ptr<FrameWriter> binary;
  if (o.format == "protobuf") {
    binary = std::make_unique<FrameWriter>(o.output_path);
  }
  std::string callback_source;
  std::unique_ptr<tracer::client::ResolvedCallable> resolved;
  std::string declaration;
  std::string declaration_source;
  if (o.command == "trace") {
    const bool raw_symbol = o.return_set;
    if (raw_symbol) {
      parse_type(o.return_type, true);
      for (const auto &type : o.argument_types) {
        parse_type(type, false);
      }
      const auto symbol_start = o.selector.find(' ');
      if (symbol_start == std::string::npos ||
          symbol_start + 1 >= o.selector.size()) {
        throw std::runtime_error(
            "raw-symbol trace requires a nonempty exact symbol");
      }
      if (o.selector.substr(symbol_start + 1).size() > 511) {
        throw std::runtime_error("symbol exceeds 511 bytes");
      }
      if (o.compdb_set || !o.sources.empty()) {
        throw std::runtime_error("--include/--compdb require debug name resolution");
      }
    }
    std::ifstream cb(o.callback);
    if (!cb) {
      throw std::runtime_error("cannot open callback file: " + o.callback);
    }
    callback_source.reserve(4096);
    char ch;
    while (cb.get(ch)) {
      if (callback_source.size() == 64 * 1024) {
        throw std::runtime_error("callback file exceeds 64 KiB");
      }
      callback_source.push_back(ch);
    }
    if (callback_source.empty()) {
      throw std::runtime_error("callback file is empty");
    }
    if (callback_source.find('\0') != std::string::npos) {
      throw std::runtime_error("callback file contains a NUL byte");
    }
    if (!raw_symbol) {
      if (o.return_set || !o.argument_types.empty()) {
        throw std::runtime_error("C++ name resolution infers the signature; "
                                 "do not provide --return or --arg");
      }
      const auto name = o.positional_selector.empty()
                            ? o.selector.substr(o.selector.find(' ') + 1)
                            : o.positional_selector;
      tracer::client::DebugLookupOptions debug_options;
      debug_options.module = o.module;
      debug_options.max_entries = o.debug_max_entries;
      debug_options.max_depth = o.debug_max_depth;
      resolved = std::make_unique<tracer::client::ResolvedCallable>(
          tracer::client::resolve_debug_callable(
              static_cast<std::uint32_t>(o.pid), name, debug_options));
      if (!o.positional_selector.empty()) {
        declaration = resolved->declaration;
      } else {
        const auto expected =
            ::tracer::compat::starts_with(o.selector, "--method") ? api::METHOD : api::FUNCTION;
        if (resolved->spec.signature().kind() != expected) {
          throw std::runtime_error("resolved declaration kind does not match " +
                                   o.selector.substr(0, o.selector.find(' ')));
        }
        declaration = resolved->declaration;
      }
      declaration_source = resolved->source;
      if (resolved->spec.symbol().size() > 511) {
        throw std::runtime_error("resolved symbol exceeds 511 bytes");
      }
    }
  }
  if (o.command == "attach") {
    api::AttachRequest req;
    req.set_pid(static_cast<std::uint32_t>(o.pid));
    api::Session res;
    grpc::ClientContext ctx;
    set_deadline(ctx, o.timeout_set ? o.timeout_seconds : 30);
    require_ok(stub.Attach(&ctx, req, &res), "Attach");
    if (binary) {
      binary->write(res);
    } else {
      print_session(text, res, "attached");
    }
    return 0;
  }
  if (o.command == "list") {
    api::ListSessionsRequest req;
    api::ListSessionsResponse res;
    grpc::ClientContext ctx;
    set_deadline(ctx, o.timeout_set ? o.timeout_seconds : 30);
    require_ok(stub.ListSessions(&ctx, req, &res), "ListSessions");
    if (binary) {
      binary->write(res);
    } else if (res.sessions().empty()) {
      text.line("no sessions");
    } else {
      for (const auto &s : res.sessions()) {
        print_session(text, s, "session");
      }
    }
    return 0;
  }
  const auto unary_timeout = o.timeout_set ? o.timeout_seconds : 30;
  api::Session session;
  std::optional<api::Session> event_session;
  if (o.command != "events") {
    session =
        find_session(stub, static_cast<std::uint32_t>(o.pid), unary_timeout);
  } else {
    event_session = find_attached_session(
        stub, static_cast<std::uint32_t>(o.pid), unary_timeout);
  }
  if (o.command == "resolve") {
    api::ResolveProbeRequest req;
    req.set_session_id(session.id());
    req.set_selector(o.positional_selector);
    req.set_compilation_database(o.compdb);
    req.set_module(o.module);
    req.set_debug_max_entries(o.debug_max_entries);
    req.set_debug_max_depth(o.debug_max_depth);
    for (const auto &header : o.sources)
      req.add_include_headers(std::filesystem::absolute(header).string());
    for (const auto &option : o.compile_options)
      req.add_compile_options(option);
    api::ProbeSpec res;
    grpc::ClientContext ctx;
    set_deadline(ctx, unary_timeout);
    require_ok(stub.ResolveProbe(&ctx, req, &res), "ResolveProbe");
    if (binary) {
      binary->write(res);
    } else {
      text.line("resolved selector=", res.debug_info().selector(),
                " symbol=", res.symbol(), " kind=", res.signature().kind(),
                " result=", res.signature().result(),
                " arguments=", res.signature().arguments_size());
      for (const auto &option : res.compile_options())
        text.line("compile_option=", option);
      if (res.has_header_context())
        text.line("header_context=", res.header_context().inference(),
                  " directory=", res.header_context().working_directory());
    }
    return 0;
  }
  if (o.command == "validate") {
    std::ifstream callback(o.callback);
    if (!callback)
      throw std::runtime_error("cannot open callback file: " + o.callback);
    std::string source;
    char ch;
    while (callback.get(ch)) {
      if (source.size() == 64 * 1024)
        throw std::runtime_error("callback file exceeds 64 KiB");
      source.push_back(ch);
    }
    if (source.empty() || source.find('\0') != std::string::npos)
      throw std::runtime_error("callback source must be nonempty and contain no NUL");
    api::ValidateProbeRequest req;
    req.set_session_id(session.id());
    req.set_selector(o.positional_selector);
    req.set_callback_source(source);
    req.set_compilation_database(o.compdb);
    req.set_module(o.module);
    req.set_debug_max_entries(o.debug_max_entries);
    req.set_debug_max_depth(o.debug_max_depth);
    req.set_level(o.validation_level == "syntax"
                      ? api::ValidateProbeRequest::SYNTAX
                      : api::ValidateProbeRequest::COMPILE);
    for (const auto &header : o.sources)
      req.add_include_headers(std::filesystem::absolute(header).string());
    for (const auto &option : o.compile_options)
      req.add_compile_options(option);
    api::ValidationReport res;
    grpc::ClientContext ctx;
    set_deadline(ctx, unary_timeout);
    require_ok(stub.ValidateProbe(&ctx, req, &res), "ValidateProbe");
    if (binary) {
      binary->write(res);
    } else {
      text.line("validation success=", res.success() ? "true" : "false",
                " level=", res.level() == api::ValidateProbeRequest::SYNTAX
                                ? "syntax"
                                : "compile");
      for (const auto &diagnostic : res.diagnostics())
        text.line(diagnostic.file(), diagnostic.line() ? ":" : "",
                  diagnostic.line() ? std::to_string(diagnostic.line()) : "",
                  diagnostic.column() ? ":" : "",
                  diagnostic.column() ? std::to_string(diagnostic.column()) : "",
                  diagnostic.file().empty() ? "" : ": ", diagnostic.severity(),
                  " [", diagnostic.stage(), "] ", diagnostic.message());
    }
    return res.success() ? 0 : 1;
  }
  if (o.command == "trace") {
    api::AddProbeRequest req;
    req.set_session_id(session.id());
    auto *spec = req.mutable_spec();
    if (resolved) {
      *spec = resolved->spec;
      spec->set_callback_source(callback_source);
      set_callback_package_location(*spec, o);
      for (const auto &header : o.sources)
        spec->add_include_headers(std::filesystem::absolute(header).string());
      for (const auto &option : o.compile_options)
        spec->add_compile_options(option);
      tracer::client::resolve_header_context(*spec, o.compdb);
    } else {
      const auto split = o.selector.find(' ');
      spec->set_symbol(o.selector.substr(split + 1));
      spec->set_callback_source(callback_source);
      set_callback_package_location(*spec, o);
      auto *sig = spec->mutable_signature();
      sig->set_kind(::tracer::compat::starts_with(o.selector, "--method") ? api::METHOD
                                                                          : api::FUNCTION);
      sig->set_result(parse_type(o.return_type, true));
      for (const auto &t : o.argument_types) {
        sig->add_arguments(parse_type(t, false));
      }
      for (const auto &option : o.compile_options)
        spec->add_compile_options(option);
    }
    spec->set_stack_depth(o.stack_depth);
    spec->set_include_internal_stack_frames(!o.stack_application_only);
    spec->set_print_arguments(o.callback_print_arguments);
    spec->set_print_result(o.callback_print_result);
    api::Probe res;
    grpc::ClientContext ctx;
    set_deadline(ctx, unary_timeout);
    require_ok(stub.AddProbe(&ctx, req, &res), "AddProbe");
    if (binary) {
      binary->write(res);
    } else {
      text.line("traced pid=", o.pid, " session=", res.session_id(),
                " probe=", res.id(), " symbol=", res.symbol(),
                declaration.empty() ? "" : " declaration=", declaration,
                declaration_source.empty() ? "" : " debug_image=",
                declaration_source);
      if (spec->has_header_context())
        text.line("header_context=", spec->header_context().inference(),
                  " directory=", spec->header_context().working_directory());
    }
    return 0;
  }
  if (o.command == "remove") {
    api::RemoveProbeRequest req;
    req.set_session_id(session.id());
    req.set_probe_id(o.probe_id);
    api::Probe res;
    grpc::ClientContext ctx;
    set_deadline(ctx, unary_timeout);
    require_ok(stub.RemoveProbe(&ctx, req, &res), "RemoveProbe");
    if (binary) {
      binary->write(res);
    } else {
      text.line("removed pid=", o.pid, " session=", res.session_id(),
                " probe=", res.id(),
                " restored=", res.original_bytes_restored() ? "true" : "false");
    }
    return 0;
  }
  if (o.command == "detach") {
    api::DetachRequest req;
    req.set_session_id(session.id());
    api::Session res;
    grpc::ClientContext ctx;
    set_deadline(ctx, unary_timeout);
    require_ok(stub.Detach(&ctx, req, &res), "Detach");
    if (binary) {
      binary->write(res);
    } else {
      print_detached(text, res);
    }
    return 0;
  }
  api::WatchEventsRequest req;
  if (event_session) {
    req.add_session_ids(event_session->id());
  } else {
    req.set_pid(static_cast<std::uint32_t>(o.pid));
  }
  req.set_after_sequence(o.after);
  req.set_include_replay_types(static_cast<bool>(storage));
  grpc::ClientContext ctx;
  if (o.timeout_set) {
    ctx.set_deadline(std::chrono::system_clock::now() +
                     std::chrono::seconds(o.timeout_seconds));
  } else if (o.count_set) {
    ctx.set_deadline(std::chrono::system_clock::now() +
                     std::chrono::seconds(30));
  }
  SignalGuard signals;
  auto reader = stub.WatchEvents(&ctx, req);
  StreamCanceller canceller(ctx);
  api::TraceEvent event;
  ContextEventDecoder context_decoder;
  std::uint64_t received = 0;
  while ((!o.count_set || received < o.count) && reader->Read(&event)) {
    if (storage) {
      storage->write(std::array{event});
    } else if (binary) {
      binary->write(event);
    } else {
      print_event(text, event, o, context_decoder);
    }
    ++received;
  }
  if (o.count_set && received == o.count) {
    ctx.TryCancel();
  }
  canceller.stop();
  auto status = reader->Finish();
  if (interrupted.load(std::memory_order_relaxed)) {
    return 0;
  }
  if (o.count_set && received == o.count) {
    return 0;
  }
  if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    throw std::runtime_error("events timed out after receiving " +
                             std::to_string(received) + " events");
  }
  require_ok(status, "WatchEvents");
  if (o.count_set) {
    throw std::runtime_error("events ended after receiving " +
                             std::to_string(received) + " of " +
                             std::to_string(o.count) + " events");
  }
  throw std::runtime_error("events stream ended unexpectedly");
}

int run_legacy(const Options &o, api::Tracer::Stub &stub) {
  FrameWriter out(o.output_path);
  if (o.command == "attach") {
    auto req = read_textproto<api::AttachRequest>(o.request_path);
    api::Session res;
    grpc::ClientContext c;
    set_deadline(c, o.timeout_seconds);
    require_ok(stub.Attach(&c, req, &res), "Attach");
    out.write(res);
    return 0;
  }
  if (o.command == "add") {
    auto req = read_textproto<api::AddProbeRequest>(o.request_path);
    api::Probe res;
    grpc::ClientContext c;
    set_deadline(c, o.timeout_seconds);
    require_ok(stub.AddProbe(&c, req, &res), "AddProbe");
    out.write(res);
    return 0;
  }
  if (o.command == "remove") {
    auto req = read_textproto<api::RemoveProbeRequest>(o.request_path);
    api::Probe res;
    grpc::ClientContext c;
    set_deadline(c, o.timeout_seconds);
    require_ok(stub.RemoveProbe(&c, req, &res), "RemoveProbe");
    out.write(res);
    return 0;
  }
  if (o.command == "detach") {
    auto req = read_textproto<api::DetachRequest>(o.request_path);
    api::Session res;
    grpc::ClientContext c;
    set_deadline(c, o.timeout_seconds);
    require_ok(stub.Detach(&c, req, &res), "Detach");
    out.write(res);
    return 0;
  }
  if (o.command == "list") {
    auto req = read_textproto<api::ListSessionsRequest>(o.request_path);
    api::ListSessionsResponse res;
    grpc::ClientContext c;
    set_deadline(c, o.timeout_seconds);
    require_ok(stub.ListSessions(&c, req, &res), "ListSessions");
    out.write(res);
    return 0;
  }
  auto req = read_textproto<api::WatchEventsRequest>(o.request_path);
  grpc::ClientContext c;
  set_deadline(c, o.timeout_seconds);
  auto reader = stub.WatchEvents(&c, req);
  api::TraceEvent event;
  std::uint64_t n = 0;
  while (n < o.count && reader->Read(&event)) {
    out.write(event);
    ++n;
  }
  if (n == o.count) {
    c.TryCancel();
    return 0;
  }
  auto s = reader->Finish();
  require_ok(s, "WatchEvents");
  throw std::runtime_error("WatchEvents ended early");
}

} // namespace

int main(int argc, char **argv) {
  try {
    const auto options = parse_options(argc, argv);
    grpc::ChannelArguments channel_arguments;
    channel_arguments.SetMaxReceiveMessageSize(-1);
    channel_arguments.SetMaxSendMessageSize(-1);
    auto channel = grpc::CreateCustomChannel(
        options.endpoint, grpc::InsecureChannelCredentials(), channel_arguments);
    auto stub = api::Tracer::NewStub(channel);
    return options.request_path.empty() ? run_direct(options, *stub)
                                        : run_legacy(options, *stub);
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer: " << error.what() << '\n';
    return 1;
  }
}
