#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <replxx.hxx>
#include <unistd.h>

#include "completion.hpp"
#include "controller.hpp"
#include "interpreter.hpp"

namespace tr = tracer_repl;

namespace {
void interrupt(int) { tr::interrupted.store(true, std::memory_order_relaxed); }

struct Signals {
  struct sigaction old{};

  Signals() {
    struct sigaction action{};
    action.sa_handler = interrupt;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, &old);
  }

  ~Signals() { sigaction(SIGINT, &old, nullptr); }
};

class Input {
  bool terminal_ = ::isatty(STDIN_FILENO);
  replxx::Replxx editor_;
  tr::CppEngine *cpp_ = nullptr;

public:
  Input() {
    editor_.set_max_history_size(1000);
    editor_.set_double_tab_completion(false);
    editor_.bind_key(replxx::Replxx::KEY::ENTER, [this](char32_t key) {
      const std::string source = editor_.get_state().text();
      const auto first = source.find_first_not_of(" \t\r\n");
      const bool more = first != std::string::npos && source[first] != '%' &&
                        tr::incomplete_cpp(source);
      return editor_.invoke(more ? replxx::Replxx::ACTION::NEW_LINE
                                 : replxx::Replxx::ACTION::COMMIT_LINE,
                            key);
    });
    editor_.set_completion_callback(
        [this](const std::string &input, int &length) {
          replxx::Replxx::completions_t completions;
          const auto first = input.find_first_not_of(" \t\r\n");
          if (first == std::string::npos)
            return completions;
          const auto entered = input.substr(first);
          if (entered.front() == '%') {
            // Command completions never perform RPCs on the editor's input
            // thread.
            for (const auto &name : tr::command_names()) {
              if (name.rfind(entered, 0) == 0) {
                completions.emplace_back(name);
              }
            }
            // Matching command prefixes are ASCII; preserve replxx's code-point
            // context length for arbitrary UTF-8 argument text.
            if (!completions.empty()) {
              length = static_cast<int>(entered.size());
            }
          } else if (cpp_) {
            try {
              const auto candidates = cpp_->complete(input);
              length = static_cast<int>(std::count_if(
                  candidates.prefix.begin(), candidates.prefix.end(),
                  [](unsigned char byte) { return (byte & 0xc0) != 0x80; }));
              for (const auto &candidate : candidates.items)
                completions.emplace_back(candidate);
            } catch (const std::exception &error) {
              editor_.print("C++ completion: %s\n", error.what());
            }
          }
          return completions;
        });
    editor_.set_highlighter_callback(
        [](const std::string &input, auto &colors) {
          if (!input.empty() && input.front() == '%') {
            std::fill(colors.begin(), colors.end(),
                      replxx::Replxx::Color::BRIGHTCYAN);
          }
        });
  }

  bool terminal() const { return terminal_; }
  void use_cpp(tr::CppEngine &cpp) { cpp_ = &cpp; }

  std::optional<std::string> read(const std::string &prompt) {
    if (!terminal_) {
      std::string line;
      if (!std::getline(std::cin, line)) {
        return {};
      }
      return line;
    }
    for (;;) {
      errno = 0;
      const auto *line = editor_.input(prompt);
      if (!line && (errno == EAGAIN || tr::interrupted.load())) {
        tr::interrupted = false;
        continue;
      }
      if (!line) {
        return {};
      }
      std::string result(line);
      if (!result.empty()) {
        editor_.history_add(result);
      }
      return result;
    }
  }
};
} // namespace

int main(int argc, char **argv) {
  std::cout.setf(std::ios::unitbuf);
  try {
    tr::Settings settings;
    std::uint32_t pid = 0;
    std::string process;
    std::vector<std::string> cpp_args;
    bool fail_fast = false;
    if (const auto *endpoint = std::getenv("CLANG_TRACER_ENDPOINT")) {
      settings.endpoint = endpoint;
    }
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        std::cout << "Usage: clang-tracer-repl [--attach PID | --process "
                     "FILTER] [--endpoint ENDPOINT]\n"
                     "  --server PATH --agent PATH --timeout SECONDS "
                     "--buffer-events N --buffer-bytes N\n"
                     "  --cpp-arg ARG (repeatable) --fail-fast\n"
                     "C++17; replxx 0.0.4; ANTLR generator/runtime 4.13.2; "
                     "LLVM 21 or 22.\n"
                     "Use %help for tracing commands and %help api for C++ "
                     "APIs. Tab completes C++ members. Without --endpoint, "
                     "startup is local and owned.\n";
        return 0;
      }
      if (option == "--fail-fast") {
        fail_fast = true;
        continue;
      }
      if (++i == argc) {
        throw std::invalid_argument("missing value for " + option);
      }
      const std::string value = argv[i];
      if (option == "--attach") {
        pid = static_cast<std::uint32_t>(
            tr::unsigned_number(value, "PID", INT32_MAX));
        if (!pid) {
          throw std::invalid_argument("PID must be positive");
        }
      } else if (option == "--process") {
        process = value;
      } else if (option == "--endpoint") {
        if (value.empty()) {
          throw std::invalid_argument("endpoint must not be empty");
        }
        settings.endpoint = value;
      } else if (option == "--server") {
        settings.server = value;
      } else if (option == "--agent") {
        settings.agent = value;
      } else if (option == "--timeout") {
        settings.timeout_seconds =
            static_cast<unsigned>(tr::unsigned_number(value, "timeout", 300));
      } else if (option == "--buffer-events") {
        settings.max_events =
            tr::unsigned_number(value, "buffer events", 1000000);
      } else if (option == "--buffer-bytes") {
        settings.max_bytes =
            tr::unsigned_number(value, "buffer bytes", 1024ULL * 1024 * 1024);
      } else if (option == "--cpp-arg") {
        cpp_args.push_back(value);
      } else {
        throw std::invalid_argument("unknown option: " + option);
      }
    }
    if (!settings.timeout_seconds || !settings.max_events ||
        !settings.max_bytes) {
      throw std::invalid_argument(
          "timeouts and buffer limits must be positive");
    }
    if (pid && !process.empty()) {
      throw std::invalid_argument(
          "--attach and --process are mutually exclusive");
    }
    Signals signals;
    Input input;
    tr::Controller controller(settings, std::cout);
    tr::Facade facade(controller);
    tr::CppEngine cpp(facade, std::move(cpp_args));
    input.use_cpp(cpp);
    auto choose = [&](const std::string &prompt) {
      return input.read(prompt).value_or("");
    };
    if (pid) {
      controller.attach(pid);
    } else if (!process.empty()) {
      controller.select_process(process, choose);
    }
    if (input.terminal()) {
      std::cout << "Clang Tracer native C++17 REPL. %help lists commands; "
                   "%help api describes C++ APIs; Tab completes members.\n";
    }
    bool failed = false;
    while (const auto line = input.read(controller.prompt())) {
      const auto first = line->find_first_not_of(" \t\r\n");
      if (first == std::string::npos) {
        continue;
      }
      tr::interrupted = false;
      try {
        if ((*line)[first] != '%') {
          if (!cpp.evaluate(*line)) {
            failed = true;
            if (fail_fast) {
              break;
            }
          }
          continue;
        }
        const auto command = tr::parse_command(*line);
        const bool api_help =
            command.kind == tr::CommandKind::help &&
            !command.arguments.empty() &&
            std::find(tr::command_names().begin(), tr::command_names().end(),
                      "%help " + command.arguments[0]) !=
                tr::command_names().end();
        if (api_help) {
          std::cout
              << "C++ APIs from the loaded header (namespace tracer_repl; "
                 "tracer is a Facade).\n"
                 "After collecting: auto data = tracer.batch(\"batch\"); "
                 "tracer.print(data);\n"
                 "Print one event: tracer.print(data.at(0)); — a trailing "
                 "semicolon is optional.\n"
                 "Typed access: event.argument_as<int>(0), "
                 "event.result_as<bool>(); types must match.\n"
                 "Typed values support scalars, std::string, containers, and "
                 "class/struct snapshots. Strings preserve NUL/binary bytes.\n"
                 "Inspect objects: "
                 "event.argument_snapshot(0).field(\"name\").as<std::string>()."
                 "\n"
                 "For argument_as<MyType>(), specialize EventRecord<MyType>: "
                 "type_name plus fields() returning a tuple of "
                 "event_member(\"name\", &MyType::member).\n"
                 "Use EventCodec<T>::decode(const EventValue&) for "
                 "constructor/factory mapping. "
                 "All converted values own their data; missing/over-budget "
                 "captures throw.\n"
                 "Use event.has_result() before reading a result (normally "
                 "POST); private implementation members are hidden.\n"
              << cpp.describe_api(command.arguments[0]) << std::flush;
        } else if (command.kind == tr::CommandKind::complete) {
          const auto candidates = cpp.complete(command.arguments.at(0));
          std::cout << "C++ completions (prefix=\"" << candidates.prefix
                    << "\"):\n";
          for (const auto &candidate : candidates.items)
            std::cout << "  " << candidate << '\n';
          if (candidates.items.empty())
            std::cout << "  (none)\n";
        } else if (command.kind == tr::CommandKind::cpp_reset) {
          cpp.reset();
          std::cout << "C++ analysis reset; tracing sessions retained\n";
        } else if (command.kind == tr::CommandKind::cpp_load) {
          if (!cpp.evaluate(tr::read_source(command.arguments.at(0)))) {
            failed = true;
            if (fail_fast) {
              break;
            }
          }
        } else if (command.kind == tr::CommandKind::draft_edit) {
          std::cout
              << "Enter interceptor C++; .end finishes, .cancel discards.\n";
          std::string source;
          bool save = false;
          while (const auto part = input.read("interceptor> ")) {
            if (*part == ".cancel") {
              break;
            }
            if (*part == ".end") {
              save = true;
              break;
            }
            source += *part + '\n';
            if (source.size() > 4 * 1024 * 1024) {
              throw std::runtime_error("interceptor exceeds 4 MiB");
            }
          }
          if (save) {
            controller.draft(command.arguments.at(0), std::move(source));
          }
        } else if (command.kind == tr::CommandKind::attach &&
                   command.arguments.empty()) {
          controller.select_process("", choose);
        } else if (!controller.execute(command)) {
          break;
        }
      } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        failed = true;
        if (fail_fast) {
          break;
        }
      }
    }
    // Destroy JIT values before their facade/controller and before detaching.
    cpp.reset();
    controller.shutdown();
    return (fail_fast || !input.terminal()) && failed ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << "clang-tracer-repl: " << error.what() << '\n';
    return 1;
  }
}
