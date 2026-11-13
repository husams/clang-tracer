#include "help.hpp"

#include "commands.hpp"
#include "interpreter.hpp"
#include "syntax.hpp"
#include "../snapshot_limits.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>

namespace tracer_repl {
namespace {
using std::string_view;

struct CommandTopic {
  string_view name;
  string_view usage;
  string_view summary;
  string_view example{};
};

constexpr string_view stream_options =
    "--count N --timeout SECONDS --after CURSOR --probe ID\n"
    "--phase PRE|POST|EXCEPTION --name SELECTOR --session ID | --all";
constexpr string_view resolution_options =
    "--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]\n"
    "--module TEXT --compile-option FLAG\n"
    "--prepared DIRECTORY --prepared-mode off|prefer|require";
constexpr string_view deployment_options =
    "--stack-depth N|full --stack-application-only true|false\n"
    "--print-arguments --print-result --snapshot-return-pointee";

// One line per option, from the table the prepare tool and the agent validate against.
std::string limit_options_text() {
  std::ostringstream text;
  for (const auto &spec : tracer::kLimitSpecs) {
    text << "--" << spec.name << " N (default " << tracer::default_limit(spec.name);
    text << ", " << spec.minimum << "-";
    if (spec.maximum == tracer::kUnlimited) text << "unlimited";
    else text << spec.maximum;
    text << (spec.prepare_time ? ", prepare" : ", run time") << "): " << spec.meaning << "; ";
  }
  return text.str();
}
const std::string limit_summary =
    "Pointer and reference members are followed automatically; --no-automatic-members follows none. "
    "Capture limits, each visible in the event when it cuts something: " + limit_options_text() +
    "'unlimited' selects the largest value (for --follow-depth, --string-bytes and --snapshot-slice that is a fixed maximum the tracer imposes), 'off' selects 0 where 0 is allowed. "
    "Prepare-time options shape the package; %probe inject accepts all of them and its value wins, but members that were not discovered at prepare time cannot be added. "
    "Each option is also read from the environment variable TRACER_PREPARE_<NAME> by the prepare tool; docs/native-repl.md lists them. ";

const std::string prepare_summary =
    std::string(
        "Build a reusable probe package from source, an explicit signature, or the module index. "
        "Without headers or an explicit signature, preparation uses the default index. "
        "Repeat --include for extra headers. "
        "Preparation uses compile_commands.json in the command's working directory by default. "
        "It prefers the first include's recorded compile command and falls back to the first source beside it. "
        "Use --module to pin the package to a specific build. "
        "Prepared headers can declare tracer_snapshot::FollowPointers<Record> specializations listing direct raw pointer fields to follow; pointee types and polymorphic implementations are discovered from the included source inputs. This policy applies to prepared argument, receiver and result records. "
        "Use --snapshot-return-pointee when injecting a raw pointer-return probe. "
        "Use --metadata-only to skip the separate C++ adapter files. ") +
    limit_summary +
    "With --signature, supply the C++ result and explicit parameter types, for example 'const int&()'. "
    "Qualified methods receive an opaque receiver. Pass --member for an unqualified instance method.";

const CommandTopic commands[] = {
    {"server", "", "Start, connect to, inspect, or stop a tracing server."},
    {"server start", "%server start [--executable PATH] [--agent PATH] [--context-event-bytes N] "
                     "[--context-transport-memory-bytes N]",
     "Start a private local server and connect to it."},
    {"server connect", "%server connect ENDPOINT",
     "Connect to an existing server after detaching and stopping any server you own.",
     "%server connect localhost:50051"},
    {"server status", "%server status", "Show the endpoint, ownership, and readiness."},
    {"server stop", "%server stop", "Stop the server started by this REPL and restore its targets."},
    {"attach", "%attach [PID [--start-time TICKS]]", "Attach to a target. Omit PID for the local picker, or supply the observed start time with a PID.",
     "%attach 12345"},
    {"launch", "%launch EXECUTABLE [--arg VALUE] [--env NAME=VALUE] [--cwd DIR]\n"
     "  [--stdin FILE] [--stdout NEW_FILE] [--stderr NEW_FILE] [--timeout SECONDS] [--preload GATE.so] [--supervise] [--gate-children]",
     "Launch a local C++ application and attach at its startup checkpoint. Repeat --arg and --env."},
    {"children", "%children", "List paused exec descendants of owned launches with --gate-children."},
    {"resume", "%resume [PID]", "Release the startup checkpoint of an attached owned target or a gated exec child."},
    {"terminate", "%terminate [PID]", "Kill and reap a target launched by this REPL."},
    {"call", "%call NAME [--arg SCALAR] [--timeout SECONDS]",
     "Invoke one explicitly registered function on a paused target's startup thread. Repeat --arg."},
    {"call-result", "%call-result [--timeout SECONDS]",
     "Collect the result of a target call after the wait timed out or was interrupted."},
    {"syscalls", "%syscalls [--count N] [--timeout SECONDS]",
     "Capture Linux syscall entry and exit records on the selected local target's existing threads."},
    {"faults", "%faults [--count N] [--timeout SECONDS]",
     "Observe delivered native fault signals without changing the target's handlers. Requires access to Linux tracefs."},
    {"sample allocations", "%sample allocations [--count N] [--timeout SECONDS]",
     "Profile glibc malloc, calloc and realloc requests on existing target threads with Linux perf uprobes."},
    {"sample cpu", "%sample cpu [--count N] [--timeout SECONDS]",
     "Sample user-space instruction addresses on the selected local target's existing threads."},
    {"watchpoint",
     "%watchpoint --tid TID --address ADDRESS --size BYTES --mapping TOKEN --access write|read-write [--count N] [--timeout SECONDS]",
     "Collect one thread's hardware watchpoint hits on a naturally aligned 1, 2, 4 or 8 byte range."},
    {"instructions start",
     "%instructions start --tid TID --address ADDRESS --size BYTES --mapping TOKEN [--count N] [--timeout SECONDS] [--registers] [--callback-library PATH] [--memory-address ADDRESS --memory-size BYTES] [--discover]",
     "Trace one thread with optional general-purpose snapshots, a trusted native callback, or scalar memory-access filtering. --discover records executed call destinations instead. Use %memory maps for the token."},
    {"instructions replay", "%instructions replay FILE", "Replay a saved bounded instruction capture without a target."},
    {"instructions stop", "%instructions stop [--save NEW_FILE]",
     "Stop and drain the selected session's capture, then print original instruction addresses and selected details."},
    {"processes", "%processes [--filter TEXT]", "List local target processes that match the filter."},
    {"sessions", "%sessions", "List sessions known to the connected server."},
    {"use", "%use SESSION", "Select an attached session for subsequent commands.", "%use s1"},
    {"status", "%status", "Show connection details, the selected session, and retained collections."},
    {"detach", "%detach [SESSION]", "Detach the selected or named session and remove its probes."},
    {"modules", "%modules [[--filter TEXT] [--details] | --elf PATH | --watch [--count N] [--timeout SECONDS]]",
     "List application modules in the selected process. The filter matches a case-sensitive path substring. "
     "Details show executable mappings, file identity, and a mapping token. "
     "Use --elf to inspect ELF imports, exports and sections or --watch to report mapping changes.",
     "%modules --filter libdemo.so --details"},
    {"library load", "%library load PATH", "Load a trusted Linux x86-64 shared object into the selected target until it exits."},
    {"memory write", "%memory write ADDRESS --mapping TOKEN --expect HEX --replace HEX",
     "Write up to 256 verified bytes to a private writable mapping. Restore with %memory restore."},
    {"memory allocate", "%memory allocate BYTES", "Allocate up to 1 MiB of zeroed writable memory owned by the session."},
    {"memory free", "%memory free ID", "Free a session allocation after restoring its patches."},
    {"memory patch", "%memory patch --module PATH --elf-address ADDRESS --mapping TOKEN --expect HEX --replace HEX",
     "Patch verified equal-width instructions at a function entry and retain original bytes for restoration."},
    {"memory import-hook", "%memory import-hook --module PATH --symbol NAME --original ADDRESS --address ADDRESS --mapping TOKEN --abi linux-x86-64-sysv",
     "Redirect one resolved ELF import slot to an ABI-compatible function in the main executable."},
    {"memory restore", "%memory restore ID", "Restore original executable or import-slot bytes."},
    {"memory monitor-start", "%memory monitor-start --tid TID --address ADDRESS --size BYTES --mapping TOKEN [--timeout SECONDS]",
     "Record the nominated thread's first read or write to each page of a dedicated mapping."},
    {"memory monitor-stop", "%memory monitor-stop", "Restore page protections and print first accesses."},
    {"module", "", "Index ELF modules and browse or export recovered types and symbols."},
    {"module index", "%module index (--file ELF | --module PATH --mapping TOKEN) [--output INDEX] [--debug-file FILE] [--max-entries N] [--max-depth N]",
     "Create a reusable module index and select it for the current REPL. File input works offline."},
    {"module use", "%module use INDEX", "Select an existing index for module commands and probe preparation."},
    {"module types", "%module types [--index INDEX] [--filter GLOB] [--limit N] [--page TOKEN] [--format json]",
     "Find indexed types and report their sizes when available."},
    {"module symbols", "%module symbols [--index INDEX] [--filter GLOB] [--exported] [--limit N] [--page TOKEN] [--format json]",
     "Find ELF symbols and distinguish exported definitions."},
    {"module show", "%module show [--index INDEX] (--type NAME | --symbol SELECTOR | --id ID)",
     "Show detailed metadata for a type or symbol."},
    {"module emit", "%module emit [--index INDEX] [--type NAME | --symbol SELECTOR | --id ID]... --output DIR",
     "Emit recovered C++ declarations, read-only views and a manifest."},
    {"threads", "%threads", "List the selected target's thread IDs, names, and Linux start times."},
    {"symbols", "%symbols [--filter TEXT] [--module TEXT] [--limit N] [--page TOKEN]\n"
     "  [--addresses] [--mapping TOKEN] [--address PC]",
     "Find supported C++ callables. Use --addresses for verified entries or --address with the mapping token to look up a PC.",
     "%symbols --filter Calculator --limit 20 --addresses"},
    {"memory", "", "List, read, or scan mapped memory."},
    {"memory maps", "%memory maps",
     "List all mapped ranges and permissions in the selected session and issue a memory snapshot token."},
    {"memory read", "%memory read ADDRESS --mapping TOKEN (--count BYTES | --type TYPE)",
     "Read up to 65536 bytes as a hex dump, or decode one little-endian u8/u16/u32/u64, "
     "i8/i16/i32/i64, f32 or f64 value."},
    {"memory scan", "%memory scan ADDRESS --mapping TOKEN --count BYTES --pattern PATTERN [--limit N]",
     "Find masked byte patterns in a readable range up to 1 MiB. Use ? for wildcard nibbles. "
     "The default result limit is 64 and the maximum is 1024.",
     R"(%memory scan 0xADDRESS --mapping TOKEN --count 4096 --pattern "48 8? ?? ff")"},

    {"trace", "", "Preview, apply, or remove a bulk C++ trace."},
    {"trace preview", "%trace preview [--module GLOB] [--include GLOB] [--exclude GLOB] [--exact SIGNATURE]\n"
     "  or %trace preview --manifest FILE\n  [--save NEW_FILE]",
     "Preview bulk C++ matches, ABI status and retained probe capacity without installing probes.",
     R"example(%trace preview --module "libapp*" --include "app::*" --exclude "app::detail::*")example"},
    {"trace apply", "%trace apply --manifest FILE [--callback FILE]",
     "Install a typed observer for each eligible manifest match. Restore newly installed probes on partial failure.",
     "%trace apply --manifest trace.textproto"},
    {"trace remove", "%trace remove BATCH_ID",
     "Remove or retry restoration of the probes owned by a batch in the selected session."},
    {"probes", "%probes [--cwd DIRECTORY]", "List the probes in the selected session."},
    {"probe", "", "Prepare, resolve, validate, install, or remove a probe."},
    {"probe entry", "%probe entry SELECTOR --mapping TOKEN [--module MODULE] [--cwd DIRECTORY]\n"
     "  or %probe entry --module PATH --elf-address ADDRESS --abi linux-x86-64-sysv --mapping TOKEN [--cwd DIRECTORY]",
     "Verify and disassemble a function entry and show the copy-only detour window and bounded control flow."},
    {"probe prepare",
     "%probe prepare SELECTOR --include HEADER --output NEW_DIRECTORY\n"
     "  [--compdb PATH] [--translation-unit FILE] [--module ELF] [--metadata-only] [--no-automatic-members] [--cwd DIRECTORY] [EXTRA COMPILER ARGUMENTS]\n"
     "  [--follow-depth N] [--follow-members N] [--record-members N] [--nested-depth N]\n"
     "  [--string-bytes N] [--vector-elements N] [--checked-reads N] [--snapshot-reserve N] [--snapshot-arena N] [--snapshot-slice N]\n"
     "  or %probe prepare SELECTOR --signature TYPE --module ELF --output NEW_DIRECTORY [--member] [--metadata-only] [--cwd DIRECTORY]"
     "\n  or %probe prepare SELECTOR [--module-index INDEX] [--module ELF] [--id ID] [--generated DIR] --output NEW_DIRECTORY",
     prepare_summary,
     R"example(%probe prepare "HFW::Item::create()" --include item.hpp --include item-impl.hpp --include item-snapshot-policy.hpp --output prepared)example"},
    {"probe resolve", "%probe resolve SELECTOR [RESOLUTION OPTIONS] [--cwd DIRECTORY]",
     "Resolve a selector in the selected target and show the available metadata.",
     R"(%probe resolve "Calculator::multiply(int, int) const")"},
    {"probe validate", "%probe validate SELECTOR --callback FILE [RESOLUTION OPTIONS] [--cwd DIRECTORY]\n"
     "  [--allow-arg-overrides] [--allow-result-override] [--allow-replacement] [--allow-call-original] [--trace-exceptions] [--reclaimable]",
     "Check whether the callback can be deployed for the selected function."},
    {"probe inject", "%probe inject SELECTOR [--callback FILE] [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS] [--cwd DIRECTORY]\n"
     "  [--allow-arg-overrides] [--allow-result-override] [--allow-replacement] [--allow-call-original] [--trace-exceptions] [--reclaimable] [--snapshot-return-pointee]\n"
     "  [--follow-depth N] [--follow-members N] [--record-members N] [--nested-depth N]\n"
     "  [--string-bytes N] [--vector-elements N] [--checked-reads N] [--snapshot-reserve N] [--snapshot-arena N] [--snapshot-slice N]",
     "Omit --callback to capture events with the built-in observer. For prepared implementation member snapshots, a callback file can define tracer_snapshot::encode(const Implementation&) noexcept to return a custom record by value. Snapshot returned record pointees only when the live layout matches and the pointee remains stable through POST; result() gives its address and result_as<T>() copies the record.",
     R"example(%probe inject "prepared_pointee::HfwManager::createItem(int)" --prepared prepared --snapshot-return-pointee)example"},
    {"probe remove", "%probe remove ID [--cwd DIRECTORY]", "Remove a probe from the selected session and restore the original code."},
    {"probe reload", "%probe reload ID --callback FILE [--cwd DIRECTORY] [--watch [--count N] [--timeout SECONDS]]",
     "Switch an active probe to a new observer generation while existing calls finish on their original generation."},
    {"probes", "%probes [--cwd DIRECTORY]", "List the probes in the selected session."},
    {"interceptor", "", "Manage named C++ callback drafts."},
    {"interceptor load", "%interceptor load FILE --as NAME", "Load a local C++ file into a named draft."},
    {"interceptor edit", "%interceptor edit NAME", "Enter C++ source interactively. Use .end to save or .cancel to discard."},
    {"interceptor show", "%interceptor show NAME", "Print the source of a named draft."},
    {"interceptor validate", "%interceptor validate NAME --target SELECTOR [RESOLUTION OPTIONS]",
     "Compile and link the named draft for the selected target without activating a probe."},
    {"interceptor upload", "%interceptor upload NAME --target SELECTOR [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS]",
     "Install the named draft on the selected target.",
     R"example(%interceptor upload hook --target "demo::compute(int)")example"},

    {"events", "%events [STREAM OPTIONS] [--as NAME]",
     "Print live events and keep a bounded collection named live by default.",
     "%events --count 5 --phase POST"},
    {"events list", "%events list NAME [--limit N]", "List retained events with their zero-based indexes. The default limit is 20."},
    {"events show", "%events show NAME --index N", "Print one retained event and its captured values.", "%events show batch --index 0"},
    {"events stack", "%events stack NAME --index N",
     "Print all captured stack frames for one retained event.",
     "%events stack fullstack --index 0"},
    {"events calls", "%events calls NAME", "Pair PRE with POST or EXCEPTION and show call durations and missing events."},
    {"events stats", "%events stats NAME",
     "Summarize live or replayed calls per probe, with duration histograms, caller edges and loss."},
    {"events clear", "%events clear NAME", "Remove a named collection and release its buffer space."},
    {"events open", "%events open FILE --as NAME",
     "Open a Protobuf or SQLite recording offline as a bounded, named collection. Compatible C++ types stored in SQLite load automatically.",
     "%events open capture.db --as saved"},
    {"events export", "%events export NAME --format json --output NEW_FILE.json",
     "Write the retained events to a new JSON file."},
    {"collect", "%collect [STREAM OPTIONS] [--as NAME]", "Capture events into a bounded collection named batch by default.",
     "%collect --count 10 --timeout 15 --phase POST --as results"},
    {"record", "%record FILE [--sqlite] [STREAM OPTIONS]",
     "Record framed Protobuf by default. With --sqlite, create or append to a SQLite recording and save available prepared C++ types for replay.",
     "%record capture.db --sqlite --name account.adjusted --count 12"},

    {"cpp", "", "Load C++, configure the local compiler, or generate mappings for captured values."},
    {"cpp load", "%cpp load FILE", "Evaluate a local C++ source file in the current interpreter."},
    {"cpp reset", "%cpp reset", "Clear C++ variables, declarations, and background range handles. Tracing sessions stay attached."},
    {"cpp import", "%cpp import HEADER [--type TYPE] [--output FILE]",
     "Generate, cache, and load mappings for typed event decoding. Repeat --type to select several records. "
     "Nested dependencies are included automatically. Import failures preserve earlier variables.",
     "%cpp import application/types.hpp --type app::Order"},
    {"cpp generate", "%cpp generate HEADER [--type TYPE] [--output FILE]",
     "Generate mappings from the header's AST. Use --output to save a header for #include or %cpp load. "
     "Automatic mappings require supported public fields and accessible default construction.",
     "%cpp generate application/types.hpp --output mappings.hpp"},
    {"cpp options", "%cpp options", "List the effective local compiler arguments in order. "
     "Use --cpp-arg ARG at startup. Probe compilation uses separate options on the server."},
    {"cpp options add", "%cpp options add ARG...", "Add compiler arguments before the first C++ input. "
     "Quote flags with spaces. Backticks run a shell command and add flags from its output.",
     R"(%cpp options add "-I/path/to/include" "-DFEATURE=1")"},
    {"cpp options clear", "%cpp options clear", "Clear custom local compiler arguments. Use %cpp reset first after C++ input."},
    {"cpp options compdb", "%cpp options compdb DATABASE --file FILE",
     "Load flags from an exact compilation database entry or a C++ source in the same directory.",
     "%cpp options compdb build/compile_commands.json --file src/target.cpp"},
    {"shell", "%shell \"COMMAND\"", "Run a local shell command and print its output.", R"(%shell "pwd")"},
    {"complete", "%complete \"C++ PREFIX\"", "List semantic C++ completions without evaluating the expression.",
     R"(%complete "tracer.")"},
    {"type", "%type SYMBOL", "Show the type or signatures of a symbol in the current C++ session.",
     "%type tracer.events_async"},
    {"help", "%help [COMMAND | api [NAME]]", "Browse commands and C++ APIs or look up a specific topic.",
     "%help probe inject\n%help api Event"},
    {"quit", "%quit | %exit", "Exit, detach sessions owned by this REPL, and stop its private server."},
};

struct ApiTopic {
  string_view name;
  string_view summary;
  string_view usage;
  string_view notes{};
  const char* declarations = nullptr;
};

const ApiTopic apis[] = {
    {"tracer", "The session and capture API exposed as tracer, an instance of tracer_repl::Facade.",
     R"(auto target = tracer.selected_session();
auto data = tracer.batch("results");
tracer.print(data);)",
     "Use the built-in session handle after %attach. For live capture, see events and events_async.", "tracer"},
    {"SessionHandle", "Identifies an attachment on the current server connection.",
     R"(auto target = tracer.selected_session();
std::cout << target.id << ' ' << target.pid << '\n';)",
     "Get a handle from the built-in session variable or tracer.attach(pid). Use a fresh handle after reconnecting.", "SessionHandle"},
    {"Event", "One captured event with its function, phase, arguments, result, receiver, or custom state.",
     R"(auto data = tracer.batch("results");
for (const auto& e : data) tracer.print(e);
if (data.size()) {
  auto e = data.at(0);
  std::cout << e.phase() << ' ' << e.function_name() << '\n';
})",
     "Use argument_as<T>(index) or result_as<T>() when T matches the captured type. "
     "has_result() includes void returns. Use argument_snapshot(), receiver_snapshot(), or state_snapshot() "
     "to inspect captured fields without rebuilding an application type. "
     "Use stack() to iterate over the captured frames.", "Event"},
    {"Collection", "A retained set of events with capture counters and status.",
     R"(auto data = tracer.batch("results");
std::cout << data.received << " received, " << data.evicted << " evicted\n";
if (data.size()) tracer.print(data.at(0));)",
     "Use size() for the retained count and at() for checked indexing. "
     "complete, cancelled, and timed_out describe how the capture ended.", "Collection"},
    {"EventRange", "A synchronous or background capture handle that can be iterated in C++17.",
     R"(auto live = tracer.events_async(session, 10, 30);
for (const auto& e : live) tracer.print(e);
auto data = live.snapshot();
if (live.running()) live.stop();
std::cout << live.error() << '\n';)",
     "Iteration starts at the oldest retained event and waits for later ones until the count, timeout, stop() or Ctrl+C. "
     "snapshot() returns the currently retained events without waiting. "
     "stop() preserves retained events. Release all copies of the range to free its capture reservation.", "EventRange"},
    {"EventValue", "An owned snapshot of a scalar, string, object, or container from a captured event.",
     R"(auto e = tracer.batch("results").at(0);
auto order = e.argument_snapshot(0);
auto id = order.field("id").as<int>();
std::cout << order.cpp_type() << ' ' << id << '\n';)",
     "Use fields() for object members and elements() or at() for array entries. "
     "Missing fields and incompatible types throw. For conversion to an application type, see EventRecord.", "EventValue"},
    {"ScalarValue", "The variant returned by argument_value() and result_value() for captured numeric or boolean values.",
     R"(auto e = tracer.batch("results").at(0);
auto value = e.argument_value(0);
std::visit([](auto v) { std::cout << v << '\n'; }, value);)",
     "Use argument_snapshot() or result_snapshot() for strings, objects, and containers.", "ScalarValue"},
    {"EventRecord", "Maps an owned event snapshot to a default-constructible C++ record with supported fields.",
     R"(// After %cpp import application/types.hpp --type app::Order
auto e = tracer.batch("results").at(0);
auto order = e.argument_as<app::Order>(0);)",
     "For a manual mapping, specialize tracer_repl::EventRecord<T> with type_name and fields(). "
     "Return a tuple of event_member(\"field\", &T::member) entries from fields(). For factory-based decoding, see EventCodec."},
    {"EventCodec", "Customizes decoding when an application type needs a constructor or factory.",
     R"(template <> struct tracer_repl::EventCodec<Order> {
  static Order decode(const tracer_repl::EventValue& value) {
    return Order(value.field("id").as<int>());
  }
};)",
     "Define Order first. The specialization is used by EventValue::as<Order>() and typed event accessors."},
    {"Event::argument_as", "Decode the argument at a zero-based index as an owned C++ value.",
     R"(auto e = tracer.batch("results").at(0);
auto input = e.argument_as<int>(0);)",
     "Use the captured C++ type. Use argument_snapshot(index) to inspect fields without a type mapping."},
    {"Event::stack", "Return the captured stack frames in order as an owned vector.",
     R"(auto e = tracer.batch("results").at(0);
for (const auto& frame : e.stack()) { std::cout << frame.pc << ' ' << frame.function << '\n'; })",
     "An event with no stored frames returns an empty vector."},
    {"Event::result_as", "Decode a captured return value as an owned C++ value (a Reconstructed<T> for records that cannot be constructed locally, such as handles holding pointers or references; do not move, copy or destroy it or reset its unique_ptr members).",
     R"(auto e = tracer.batch("results").at(0);
if (e.phase() == "POST" && e.has_result()) {
  auto result = e.result_as<int>();
  std::cout << result << '\n';
})",
     "This example assumes the probed function returns int. has_result() can also be true for void returns."},
    {"Event::state_as", "Decode the payload at a zero-based index in a named custom event.",
     R"(// After opening a recording with the app::State type
auto e = tracer.batch("results").at(0);
auto state = e.state_as<app::State>();)",
     "The index defaults to zero. SQLite replay generates mappings for compatible recorded C++ types. "
     "Use state_snapshot(index) to inspect fields without a mapping, or %cpp import for a local type definition."},
    {"attach", "Attach to a target PID and return its session handle.", "auto target = tracer.attach(12345);"},
    {"selected_session", "Get the currently selected attachment.", "auto target = tracer.selected_session();"},
    {"modules", "List application module paths in the selected process.",
     R"(auto paths = tracer.modules("libdemo.so");)",
     "Pass an optional case-sensitive path substring. The ModuleList supports iteration, "
     "size(), empty(), and at(index)."},
    {"module_info", "Read the executable mappings for one module in the selected process.",
     R"(auto info = tracer.module_info("libdemo.so");
std::cout << info.path << ' ' << info.mapping_token << '\n';)",
     "Pass the exact path or a unique basename. Missing and ambiguous names throw std::invalid_argument. "
     "Each entry in info.mappings contains begin_address, end_address (exclusive), "
     "file_offset, device, inode, and build_id."},
    {"prepare", "Generate a reusable probe package from local headers.",
     R"example(tracer.prepare("demo::compute(int)", {"demo.hpp", "state.hpp"}, "prepared");)example",
     "Pass one header, a braced list, or a vector of header paths. Order the probe's declaration header first, "
     "followed by additional types used by the callback. The output directory must be new. "
     "See %help probe prepare for compilation database and module options."},
    {"module_index", "Build an ELF and DWARF index and select it for this REPL.",
     R"(auto index = tracer.module_index(tracer_module::ModuleSource::file("./libdemo.so"));)"},
    {"open_module_index", "Open a saved module index and select it for this REPL.",
     R"(auto index = tracer.open_module_index("libdemo.so.tracer.sqlite");)",
     "Use index.find_type, index.find_symbol, index.emit_headers and index.prepare_probe."},
    {"inject", "Install a callback from a local file and return the probe ID.",
     R"example(auto probe = tracer.inject(session, "demo::compute(int)", "hook.cpp");)example",
     "Pass a prepared package directory as the fourth argument to require prepared injection."},
    {"detach", "Detach the given session and remove its probes.", "tracer.detach(session);"},
    {"collect", "Wait for a bounded capture and return a Collection.",
     "auto data = tracer.collect(session, 10, 15);\ntracer.print(data);",
     "Pass the event count and optionally a timeout in seconds. The default timeout is 30 seconds."},
    {"events", "Wait for a synchronous event capture and return an EventRange.",
     "auto captured = tracer.events(session, 10, 15);\nfor (const auto& e : captured) tracer.print(e);",
     "Pass a positive count and timeout in seconds. The default timeout is 30 seconds. "
     "Check captured.error() for stream failures and captured.snapshot() for partial results."},
    {"events_async", "Start a background event capture and return an EventRange immediately.",
     "auto live = tracer.events_async(session, 10, 15);\nauto data = live.snapshot();\nlive.stop();",
     "The optional count and timeout default to zero, which leaves the corresponding limit open. "
     "Check running() and error() as capture proceeds."},
    {"record", "Record events and available prepared C++ types to SQLite.",
     R"(tracer_repl::RecordOptions options;
options.names = {"account.adjusted"};
options.count = 12;
options.timeout_seconds = 15;
auto saved = tracer.record("capture.db", options);
std::cout << saved.received << ' ' << saved.complete << '\n';)",
     "Use the selected session, set options.session for another attachment, or set all_sessions. "
     "Filter with names, phase, probe_id, and after_sequence. Zero count streams until cancelled or timed out. "
     "Zero timeout uses the console default for counted recordings. Committed events remain available "
     "after cancellation or timeout. The result reports received, cursor, complete, cancelled, and timed_out."},
    {"replay", "Query a Protobuf or SQLite recording offline and return a Collection.",
     R"(tracer_repl::EventQuery query;
query.names = {"account.adjusted"};
query.count = 20;
auto data = tracer.replay("capture.db", query);
if (data.size()) tracer.print(data.at(0));)",
     "Set names, phase, pid, session_id, or count to narrow the read. Names match exact function "
     "selectors or custom event names. Count zero reads all matches and retains the latest that fit. "
     "SQLite replay loads compatible recorded C++ types and generates their mappings before the next input."},
    {"batch", "Get a Collection from a named capture or replay.",
     R"(auto data = tracer.batch("results");
if (data.size()) tracer.print(data.at(0));)",
     "First create the collection with %collect, %events, or %events open."},
    {"print", "Print a captured Event or an entire Collection.",
     R"(auto data = tracer.batch("results");
tracer.print(data);
if (data.size()) tracer.print(data.at(0));)"},
    {"adaptors", "Filter, slice, and transform Collection and EventRange values with C++17 pipelines.",
     R"(auto data = tracer.batch("results");
auto is_post = [](const auto& e) { return e.phase() == "POST"; };
auto first_post = data | filter(is_post) | first();
if (first_post) tracer.print(*first_post);
auto ids = data | take(10) | transform([](const auto& e) {
  return e.sequence();
}) | to_vector();)",
     "Use %help api NAME for each adaptor's inputs, behavior, and example. Try %help api drop.\n"
     "Topics: filter, take, drop, take_while, drop_while, transform, stride, "
     "enumerate, reverse, first, to_vector.\n"
     "Views over live ranges inspect a fresh snapshot each time a loop starts. "
     "They leave the background capture running."},
    {"filter", "Keep every item that satisfies a predicate.",
     R"(auto is_post = [](const auto& e) { return e.phase() == "POST"; };
auto post = tracer.batch("results") | filter(is_post);
for (const auto& e : post) tracer.print(e);)",
     "filter(predicate) returns a lazy view in input order. The predicate takes one item "
     "and returns a truth value. Put filter before take to count matches, or after take "
     "to test only a prefix of the input."},
    {"take", "Keep up to the first count items.",
     R"(auto preview = tracer.batch("results") | take(5);
for (const auto& e : preview) tracer.print(e);)",
     "take(count) returns a lazy view and stops when the count or input is exhausted. "
     "Zero yields an empty view without evaluating upstream predicates. "
     "A negative count throws std::invalid_argument. On a live range, take limits the "
     "snapshot being inspected. Use the count argument to events_async to limit capture."},
    {"drop", "Skip a fixed number of items at the start and keep the rest in input order.",
     R"(auto page = tracer.batch("results") | drop(5) | take(10);
for (const auto& e : page) tracer.print(e);)",
     "drop(count) returns a lazy view and skips the prefix when iteration starts. "
     "Zero skips nothing. A count at least as large as the input yields an empty view. "
     "A negative count throws std::invalid_argument. The source collection is preserved. "
     "Put drop after filter to skip matching items."},
    {"take_while", "Keep the leading items that satisfy a predicate.",
     R"(auto early = [](const auto& e) { return e.sequence() < 50; };
auto prefix = tracer.batch("results") | take_while(early);
for (const auto& e : prefix) tracer.print(e);)",
     "take_while(predicate) returns a lazy view and stops at the first false result. "
     "A false result on the first item yields an empty view. Use filter to keep matches "
     "that occur later in the input."},
    {"drop_while", "Skip the leading items that satisfy a predicate.",
     R"(auto early = [](const auto& e) { return e.sequence() < 50; };
auto tail = tracer.batch("results") | drop_while(early);
for (const auto& e : tail) tracer.print(e);)",
     "drop_while(predicate) returns a lazy view starting at the first false result. "
     "It keeps that item and every item after it. If all items match, the view is empty."},
    {"transform", "Convert each item into a new value.",
     R"(auto name = [](const auto& e) { return e.function_name(); };
auto names = tracer.batch("results") | transform(name) | to_vector();)",
     "transform(function) returns a lazy view that calls the function when an item is read. "
     "The function takes one item and returns the value to emit, such as a string or sequence number. "
     "Use to_vector to evaluate and store the results."},
    {"stride", "Select items at a fixed interval, starting with the first.",
     R"(auto sampled = tracer.batch("results") | stride(3);
for (const auto& e : sampled) tracer.print(e);)",
     "stride(step) returns a lazy view. A step of 3 selects positions 0, 3, 6, and so on. "
     "A step of 1 keeps every item. Zero and negative steps throw std::invalid_argument."},
    {"enumerate", "Pair each item with its zero-based index in the current pipeline.",
     R"(auto numbered = tracer.batch("results") | drop(5) | enumerate();
for (const auto& [index, e] : numbered) {
  std::cout << index << ' ' << e.function_name() << '\n';
})",
     "enumerate() returns a lazy view of std::pair values. Read the pair with a structured "
     "binding or its first and second members. After drop(5), the sixth input item gets index 0. "
     "Enumerate before filtering to preserve the original positions."},
    {"reverse", "Materialize the input in reverse iteration order.",
     R"(auto backwards = tracer.batch("results") | reverse();
if (!backwards.empty()) tracer.print(backwards.front());)",
     "reverse() evaluates the entire input immediately and returns an owned std::vector. "
     "It preserves the source order in the original collection. Put take before reverse "
     "to reverse only a prefix and bound the number of items stored."},
    {"first", "Return the first available item as an owned optional value.",
     R"(auto is_post = [](const auto& e) { return e.phase() == "POST"; };
auto event = tracer.batch("results") | filter(is_post) | first();
if (event) tracer.print(*event);)",
     "first() evaluates enough of the input to find one item. It returns std::optional<T>, "
     "where T is the type produced by the pipeline. The optional is empty if the input yields no items."},
    {"to_vector", "Evaluate the pipeline and store its items in an owned vector.",
     R"(auto sequence = [](const auto& e) { return e.sequence(); };
auto ids = tracer.batch("results") | take(10) | transform(sequence) | to_vector();)",
     "to_vector() returns a std::vector<T> in iteration order. It reads the entire input "
     "immediately and returns an empty vector for empty input. Put take before to_vector "
     "to cap the number of stored items."},
};

constexpr string_view reset = "\033[0m";
constexpr string_view bold = "\033[1m";
constexpr string_view cyan = "\033[1;36m";

void styled(std::ostream& out, string_view text, string_view style, bool color) {
  if (color) out << style;
  out << text;
  if (color) out << reset;
}

void block(std::ostream& out, string_view text, Syntax syntax, bool color) {
  while (!text.empty()) {
    const auto newline = text.find('\n');
    out << "  ";
    print_code(out, text.substr(0, newline), syntax, color);
    out << '\n';
    if (newline == string_view::npos) break;
    text.remove_prefix(newline + 1);
  }
}

void heading(std::ostream& out, string_view title, bool color) {
  styled(out, title, bold, color);
  out << '\n';
}

bool child_topic(string_view name, string_view parent) {
  return name.size() > parent.size() && name.substr(0, parent.size()) == parent &&
         name[parent.size()] == ' ';
}

const ApiTopic* find_api(string_view name) {
  for (const string_view prefix : {"tracer_repl::", "tracer.", "Facade::"}) {
    if (name.substr(0, prefix.size()) == prefix)
      name.remove_prefix(prefix.size());
  }
  if (name == "Facade") name = "tracer";
  for (const auto& entry : apis) {
    if (entry.name == name) return &entry;
  }
  return nullptr;
}

void command_index(std::ostream& out, bool color) {
  heading(out, "Clang Tracer commands", color);
  out << "Use %help COMMAND for usage, %help api for the C++ API index,\n"
         "or %help api NAME for an API example. Tab completes help topics.\n";
  for (const auto& entry : commands) {
    if (entry.name.find(' ') != string_view::npos) continue;
    if (entry.name == "server" || entry.name == "probe" ||
        entry.name == "events" || entry.name == "cpp") {
      out << '\n';
      heading(out, entry.name == "server" ? "Connection and targets" :
                   entry.name == "probe" ? "Probes and interceptors" :
                   entry.name == "events" ? "Capture and replay" : "C++ and utilities", color);
    }
    out << "  ";
    const auto name = "%" + std::string(entry.name);
    styled(out, name, cyan, color);
    out << std::setw(std::max(1, 16 - static_cast<int>(name.size()))) << ' '
        << entry.summary << '\n';
  }
}

void api_index(std::ostream& out, bool color) {
  heading(out, "C++ API", color);
  out << "Use %help api NAME for examples and details.\n"
         "The examples that use a collection named results start with\n";
  block(out, "%collect --count 10 --as results\n"
             "%events open capture.pb --as results",
        Syntax::command, color);
  out << "Capture live events with the first command or open a recording with the second.\n";
  for (const auto& entry : apis) {
    if (entry.name == "tracer" || entry.name == "Event::argument_as" ||
        entry.name == "attach" || entry.name == "adaptors") {
      out << '\n';
      heading(out, entry.name == "tracer" ? "Types and objects" :
                   entry.name == "Event::argument_as" ? "Typed event access" :
                   entry.name == "attach" ? "tracer methods" : "Range pipelines", color);
    }
    out << "  ";
    styled(out, entry.name, cyan, color);
    out << std::setw(std::max(1, 21 - static_cast<int>(entry.name.size()))) << ' '
        << entry.summary << '\n';
  }
}

void command_details(std::ostream& out, const CommandTopic& entry, bool color) {
  heading(out, "%" + std::string(entry.name), color);
  out << entry.summary << '\n';
  if (!entry.usage.empty()) {
    out << '\n';
    heading(out, "Usage", color);
    block(out, entry.usage, Syntax::command, color);
  }
  if (std::any_of(std::begin(commands), std::end(commands),
                  [&](const auto& sub) { return child_topic(sub.name, entry.name); })) {
    out << '\n';
    heading(out, "Subcommands", color);
    for (const auto& sub : commands) {
      if (child_topic(sub.name, entry.name)) {
        out << "  ";
        styled(out, "%" + std::string(sub.name), cyan, color);
        out << '\n' << "    " << sub.summary << '\n';
      }
    }
  }
  if (entry.name == "probe" || entry.usage.find("--cwd DIRECTORY") != string_view::npos) {
    out << "\n--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.\n"
           "Server-side inputs must be available at the resulting paths on the server host.\n";
  }
  if (entry.usage.find("STREAM OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Stream options", color);
    block(out, stream_options, Syntax::command, color);
    out << "The selected session is the default. Without a count or timeout, press Ctrl+C to stop.\n";
  }
  if (entry.usage.find("RESOLUTION OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Resolution options", color);
    block(out, resolution_options, Syntax::command, color);
    block(out, "[EXTRA COMPILER ARGUMENTS]", Syntax::command, color);
    out << "The default mode, prefer, uses an available prepared package and falls back to DWARF.\n";
    out << "With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.\n"
           "Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.\n";
  }
  if (entry.usage.find("EXTRA COMPILER ARGUMENTS") != string_view::npos ||
      entry.usage.find("RESOLUTION OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Extra compiler arguments", color);
    block(out, R"(--extra-arg ARG | --extra-args "-I dir -DNAME=1" | --extra-args `my-conf --cxxflags`)",
          Syntax::command, color);
    out << "Repeat these options to add fallback include paths and new macro definitions. "
           "Compilation database settings take priority.\n"
           "Set startup defaults with --extra-arg=ARG or --extra-args \"$(my-conf --cxxflags)\".\n";
  }
  if (entry.usage.find("DEPLOYMENT OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Deployment options", color);
    block(out, deployment_options, Syntax::command, color);
  }
  if (!entry.example.empty()) {
    out << '\n';
    heading(out, "Example", color);
    block(out, entry.example, Syntax::command, color);
  }
}

void api_details(std::ostream& out, const ApiTopic& entry, CppEngine& cpp, bool color) {
  heading(out, entry.name, color);
  out << entry.summary << "\n\n";
  heading(out, "Example", color);
  block(out, entry.usage, Syntax::cpp, color);
  if (!entry.notes.empty()) out << '\n' << entry.notes << '\n';
  if (entry.declarations) {
    out << '\n';
    heading(out, "Public signatures", color);
    const auto signatures = cpp.describe_api(entry.declarations);
    const auto first = signatures.find_first_not_of('\n');
    if (first != std::string::npos)
      block(out, string_view(signatures).substr(first), Syntax::cpp, color);
  }
}
} // namespace

const std::vector<std::string>& command_completions() {
  static const auto names = [] {
    auto result = command_names();
    for (const auto& entry : commands) {
      result.push_back("%" + std::string(entry.name));
      result.push_back("%help " + std::string(entry.name));
    }
    result.emplace_back("%help exit");
    result.emplace_back("%help api");
    result.emplace_back("%help api Facade");
    for (const auto& entry : apis) {
      result.push_back("%help api " + std::string(entry.name));
      if (entry.declarations)
        result.push_back("%help " + std::string(entry.name));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }();
  return names;
}

std::string command_usage(std::string_view name) {
  for (const auto& entry : commands) {
    if (entry.name != name) continue;
    std::string usage(entry.usage);
    const auto append_options = [&](string_view group, string_view options) {
      if (entry.usage.find(group) != string_view::npos) {
        usage += '\n';
        usage += options;
      }
    };
    append_options("STREAM OPTIONS", stream_options);
    append_options("RESOLUTION OPTIONS", resolution_options);
    append_options("DEPLOYMENT OPTIONS", deployment_options);
    if (entry.usage.find("EXTRA COMPILER ARGUMENTS") != string_view::npos ||
        entry.usage.find("RESOLUTION OPTIONS") != string_view::npos)
      usage += " --extra-arg ARG --extra-args ARG";
    return usage;
  }
  return {};
}

void print_help(std::ostream& out, const std::vector<std::string>& arguments,
                CppEngine& cpp, bool color) {
  if (arguments.empty()) {
    command_index(out, color);
    return;
  }
  if (arguments.front() == "api") {
    if (arguments.size() == 1) {
      api_index(out, color);
      return;
    }
    if (arguments.size() == 2) {
      if (const auto* entry = find_api(arguments[1])) {
        api_details(out, *entry, cpp, color);
        return;
      }
    }
    throw std::invalid_argument("unknown C++ API topic. Use %help api to list topics.");
  }

  std::string name = arguments.front();
  for (std::size_t i = 1; i < arguments.size(); ++i)
    name += " " + arguments[i];
  if (!name.empty() && name.front() == '%') name.erase(0, 1);
  if (name == "exit") name = "quit";
  for (const auto& entry : commands) {
    if (entry.name == name) {
      command_details(out, entry, color);
      return;
    }
  }
  if (const auto* entry = find_api(name)) {
    api_details(out, *entry, cpp, color);
    return;
  }
  throw std::invalid_argument("unknown help topic. Use %help for commands or %help api for C++ APIs.");
}
} // namespace tracer_repl
