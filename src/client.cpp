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
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <google/protobuf/text_format.h>
#include <grpcpp/grpcpp.h>

#include "debug_resolver.hpp"
#include "header_context.hpp"
#include "tracer.grpc.pb.h"

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
  std::string return_type;
  std::string callback;
  std::vector<std::string> argument_types;
  std::vector<std::string> sources;
  std::uint64_t pid = 0;
  std::uint64_t probe_id = 0;
  std::uint64_t count = 0;
  std::uint64_t after = 0;
  std::uint64_t timeout_seconds = 0;
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
             "[--include HEADER ...] [--compdb FILE|DIR] [--stack DEPTH]\n"
             "    --stack-application-only true|false (default: true); false shows all frames.\n"
             "    --callback-print-arguments true|false (default: false)\n"
             "    --callback-print-result true|false (default: false)\n"
             "    Header flags are auto-detected or inferred by Clang from "
             "the compilation database; target .cpp files are not read.\n"
             "  trace PID (--function NAME|--method NAME) [--return "
             "TYPE [--arg TYPE ...]] --callback FILE\n"
             "  events PID [--count N] [--after N] [--timeout SEC]\n"
             "  detach PID | remove PID PROBE_ID | list\n"
             "  CALLBACK FILE may define class Interceptor with optional\n"
             "  pre(const Call&) and post(const Call&) methods; each may\n"
             "  return void or long long. Legacy trace_callback is "
             "entry-only.\n"
             "Legacy: --request FILE [--output FILE] "
             "attach|add|remove|detach|list|watch\n";
      std::exit(0);
    }
    if (a == "--endpoint" || a == "--request" || a == "--output" ||
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
               a == "--compdb" || a == "--stack" || a == "--stack-application-only" ||
               a == "--callback-print-arguments" || a == "--callback-print-result") {
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
        o.stack_depth = static_cast<unsigned>(parse_number(v, a, 64));
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
      } else {
        o.sources.push_back(v);
      }
    } else if (a.starts_with("--")) {
      throw std::runtime_error("unknown option: " + a);
    } else if (o.command.empty()) {
      o.command = a;
    } else if ((o.command == "attach" || o.command == "trace" ||
                o.command == "events" || o.command == "detach" ||
                o.command == "remove") &&
               o.pid == 0) {
      o.direct = true;
      o.pid = parse_number(a, "PID", std::numeric_limits<std::uint32_t>::max());
    } else if (o.command == "trace" && o.pid != 0 &&
               o.positional_selector.empty()) {
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
  } else if (o.command == "attach" || o.command == "events" ||
             o.command == "detach") {
    if (o.pid == 0) {
      throw std::runtime_error(o.command + " requires PID");
    }
  } else if (o.command == "remove") {
    if (o.pid == 0 || o.probe_id == 0) {
      throw std::runtime_error("remove requires PID and PROBE_ID");
    }
  } else if (o.command != "list") {
    throw std::runtime_error("unknown command: " + o.command);
  }
  if (o.command != "events" && (o.count_set || o.after_set)) {
    throw std::runtime_error("--count and --after are only valid for events");
  }
  const bool has_trace_args =
      !o.selector.empty() || !o.return_type.empty() ||
      !o.argument_types.empty() || !o.callback.empty() ||
      !o.positional_selector.empty() || o.compdb_set || !o.sources.empty() || o.stack_depth ||
      o.stack_application_only_set || o.callback_print_set;
  if (o.command != "trace" && has_trace_args) {
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
      "' (expected int32, int64, uint64, float32, float64, pointer" +
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

api::Session find_session(api::Tracer::Stub &stub, std::uint32_t pid,
                          std::uint64_t timeout_seconds) {
  api::ListSessionsRequest req;
  api::ListSessionsResponse res;
  grpc::ClientContext ctx;
  set_deadline(ctx, timeout_seconds);
  require_ok(stub.ListSessions(&ctx, req, &res), "ListSessions");
  const api::Session *found = nullptr;
  for (const auto &s : res.sessions()) {
    if (s.pid() == pid && s.state() == api::Session::ATTACHED) {
      if (found) {
        throw std::runtime_error("multiple attached sessions match PID " +
                                 std::to_string(pid));
      }
      found = &s;
    }
  }
  if (!found) {
    throw std::runtime_error("no attached session found for PID " +
                             std::to_string(pid));
  }
  return *found;
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

void print_event(TextWriter &out, const api::TraceEvent &e) {
  const char *phase = "unspecified";
  if (e.phase() == api::PRE) {
    phase = "pre";
  } else if (e.phase() == api::POST) {
    phase = "post";
  }
  out.line(
      "cursor=", e.sequence(), " pid=", e.pid(), " tid=", e.tid(),
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
        if (e.object_fields().empty())
          return std::string{};
        std::string s = " fields=[";
        for (const auto &field : e.object_fields()) {
          if (s.back() != '[')
            s += ", ";
          s += field.name() + "=" + value_text(field.value());
        }
        return s + "]";
      }(),
      e.named_arguments().empty() && e.object_fields().empty()
          ? " callback=" + std::to_string(e.callback_value())
          : "",
      " phase=", phase, " call=", e.call_id(),
      e.phase() == api::POST && e.has_result()
          ? " result=" + value_text(e.result())
          : "",
      " lost(agent=", e.agent_dropped_events(),
      ",server=", e.server_dropped_events(), ")");
  if (e.has_stack()) {
    const auto &stack = e.stack();
    out.line("  entry-stack ", api::StackTrace::Status_Name(stack.status()),
             " depth=", stack.frames_size(), "/", stack.requested_depth(),
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

int run_direct(const Options &o, api::Tracer::Stub &stub) {
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
      resolved = std::make_unique<tracer::client::ResolvedCallable>(
          tracer::client::resolve_debug_callable(
              static_cast<std::uint32_t>(o.pid), name));
      if (!o.positional_selector.empty()) {
        declaration = resolved->declaration;
      } else {
        const auto expected =
            o.selector.starts_with("--method") ? api::METHOD : api::FUNCTION;
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
  const auto session =
      find_session(stub, static_cast<std::uint32_t>(o.pid), unary_timeout);
  if (o.command == "trace") {
    api::AddProbeRequest req;
    req.set_session_id(session.id());
    auto *spec = req.mutable_spec();
    if (resolved) {
      *spec = resolved->spec;
      spec->set_callback_source(callback_source);
      for (const auto &header : o.sources)
        spec->add_include_headers(std::filesystem::absolute(header).string());
      tracer::client::resolve_header_context(*spec, o.compdb);
    } else {
      const auto split = o.selector.find(' ');
      spec->set_symbol(o.selector.substr(split + 1));
      spec->set_callback_source(callback_source);
      auto *sig = spec->mutable_signature();
      sig->set_kind(o.selector.starts_with("--method") ? api::METHOD
                                                       : api::FUNCTION);
      sig->set_result(parse_type(o.return_type, true));
      for (const auto &t : o.argument_types) {
        sig->add_arguments(parse_type(t, false));
      }
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
  req.add_session_ids(session.id());
  req.set_after_sequence(o.after);
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
  std::uint64_t received = 0;
  while ((!o.count_set || received < o.count) && reader->Read(&event)) {
    if (binary) {
      binary->write(event);
    } else {
      print_event(text, event);
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
    auto channel = grpc::CreateChannel(options.endpoint,
                                       grpc::InsecureChannelCredentials());
    auto stub = api::Tracer::NewStub(channel);
    return options.request_path.empty() ? run_direct(options, *stub)
                                        : run_legacy(options, *stub);
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer: " << error.what() << '\n';
    return 1;
  }
}
