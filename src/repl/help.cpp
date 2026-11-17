#include "help.hpp"

#include "commands.hpp"
#include "interpreter.hpp"
#include "syntax.hpp"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tracer_repl {
namespace {
using std::string_view;

struct CommandTopic {
  string_view name;
  string_view usage;
  string_view summary;
  string_view example{};
  string_view options{};
};

constexpr string_view stream_options =
    "--count N --timeout SECONDS --after CURSOR --probe ID\n"
    "--phase PRE|POST --name SELECTOR --session ID | --all";
constexpr string_view resolution_options =
    "--include SERVER_HEADER --compdb SERVER_PATH [--translation-unit SERVER_FILE]\n"
    "--module TEXT --compile-option FLAG\n"
    "--prepared DIRECTORY --prepared-mode off|prefer|require";
constexpr string_view deployment_options =
    "--stack-depth N|full --stack-application-only true|false\n"
    "--print-arguments --print-result --snapshot-return-pointee";

// Per-option descriptions for the shared groups, as "LABEL<TAB>DESCRIPTION" lines.
constexpr string_view stream_option_rows =
    "--count N\tStop after N matching events, N > 0. Without --count or --timeout the stream runs until Ctrl+C.\n"
    "--timeout SECONDS\tStop after this many seconds, 1 to 86400. With --count it defaults to the REPL's --timeout, 30 seconds unless changed. Without --count there is no limit.\n"
    "--after CURSOR\tStart strictly after this sequence cursor, for example the cursor= value of an earlier run, to resume.\n"
    "--probe ID\tOnly events from this probe ID. It is filtered in the REPL, so --count counts matching events.\n"
    "--phase PRE|POST\tOnly events of this phase. It is filtered in the REPL.\n"
    "--name SELECTOR\tOnly events whose function selector, linkage name or custom event name matches exactly. Repeat for several, which match any of them.\n"
    "--session ID\tReceive from this session instead of the selected one.\n"
    "--all\tReceive from every session. Conflicts with --session.\n";
constexpr string_view resolution_option_rows =
    "--include SERVER_HEADER\tHeader on the server host to use when resolving types and compiling the callback. Repeat for several.\n"
    "--compdb SERVER_PATH\tCompilation database, a file or directory on the server host, that supplies the compile flags for the target and its headers.\n"
    "--translation-unit SERVER_FILE\tSource file on the server host whose compilation database entry to use. Requires --compdb.\n"
    "--module TEXT\tNarrow ELF resolution to modules whose path matches TEXT. A plain module name works as a filter.\n"
    "--compile-option FLAG\tAdd one compiler argument to the target compilation. Repeat to add several, in order.\n"
    "--prepared DIRECTORY\tPrepared package created by %probe prepare. Without it, a package prepared in this REPL for the same selector is used.\n"
    "--prepared-mode off|prefer|require\toff resolves from DWARF only. prefer uses the package and falls back to DWARF. require fails if the package cannot be used. The default is prefer when a package is given or was prepared in this REPL, otherwise off.\n"
    "[EXTRA COMPILER ARGUMENTS]\tSupplemental include paths and macros. See Extra compiler arguments below.\n";
constexpr string_view deployment_option_rows =
    "--stack-depth N|full\tCapture the entry stack with each event, up to N frames. The default 0 disables it. full follows the supported unwind chain.\n"
    "--stack-application-only true|false\tWith true, the default, stacks show only application frames. false also includes runtime and JIT frames.\n"
    "--print-arguments\tInclude the automatically captured arguments in events. Explicit callback fields stay available either way.\n"
    "--print-result\tInclude the automatically captured result in events.\n"
    "--snapshot-return-pointee\tFor a raw pointer return, let the callback snapshot the returned record's fields. The pointee needs a matching record layout and must stay live and stable right after return.\n"
    "--snapshot-receiver-at-return\tAlso snapshot the receiver at return, for methods whose receiver is stable at return.\n";
constexpr string_view extra_argument_rows =
    "--extra-arg ARG\tAdd one compiler argument, for example -I/dir or -DNAME=1. Repeat for several.\n"
    "--extra-args FLAGS\tAdd a quoted list such as \"-I dir -DNAME=1\", or the output of a backtick command such as `my-conf --cxxflags`. Repeat for several.\n";

// Option rows are "LABEL<TAB>DESCRIPTION" lines. A line without a tab is a sub-heading.
// Examples are command lines; lines starting with # are explanatory comments.
#define OBJECT_PROBE_START_OPTIONS \
    "--package FILE\tPackage file created by %object-probe prepare.\n" \
    "--module MAPPING\tMapped name or path of the module that contains the field writer, for a writer in an attached shared library.\n" \
    "--method-module MAPPING\tMapped module that contains the method images. Use it when the method is not uniquely mapped.\n" \
    "--follow-calls\tFollow the native callees of the writer. Available for write-only packages.\n" \
    "--no-values\tRecord each completed write without reading field values.\n" \
    "--first-seen\tScope a write-only package to one verified invocation of a standalone executable writer.\n" \
    "--union-tag FIELD\tDiscriminator field of the watched union, relative to the watched record.\n" \
    "--union-case TAG=MEMBER\tMap one tag value to the union member it selects. Repeat for each admitted tag.\n" \
    "--synchronization POLICY\tDeclare how the sampled state is protected: thread-confined or caller-lock.\n"

const CommandTopic commands[] = {
    {"server", "", "Start, connect to, inspect, or stop a tracing server."},
    {"server start", "%server start [--executable PATH] [--agent PATH] [--context-event-bytes N] "
                     "[--context-transport-memory-bytes N]",
     "Start a private local server and connect to it.",
     "# Start a private server with an explicit agent library\n"
     "%server start --agent /opt/tracer/lib64/libclang_tracer_agent.so\n"
     "# Allow 24 MiB events, for example for deep stack captures\n"
     "%server start --context-event-bytes 25165824 --context-transport-memory-bytes 234881024",
     "--executable PATH\tServer executable to start. Defaults to the REPL's --server value, otherwise an adjacent clang-tracer-server and then PATH.\n"
     "--agent PATH\tInjected agent library the server uses. Defaults to the REPL's --agent value, otherwise the server finds the installed or adjacent library.\n"
     "--context-event-bytes N\tShared event byte budget of the managed server, a positive 32-bit value. Raise it for large events such as deep stacks. The server default is 256 KiB.\n"
     "--context-transport-memory-bytes N\tTotal shared transport memory budget of the managed server, a positive value. The server default is 64 MiB.\n"},
    {"server connect", "%server connect ENDPOINT",
     "Connect to an existing server after detaching and stopping any server you own.",
     "# Connect to a server on this host\n"
     "%server connect localhost:50051\n"
     "# Connect through a Unix socket\n"
     "%server connect unix:///tmp/clang-tracer.sock",
     "ENDPOINT\tAddress of the running server: HOST:PORT or unix:///PATH. The REPL cleans up its current connection first.\n"},
    {"server status", "%server status", "Show the endpoint, ownership, and readiness.",
     "%server status", ""},
    {"server stop", "%server stop", "Stop the server started by this REPL and restore its targets.",
     "%server stop", ""},
    {"attach", "%attach [PID [--start-time TICKS]]", "Attach to a target. Omit PID for the local picker, or supply the observed start time with a PID.",
     "# Pick a local process from a list\n"
     "%attach\n"
     "# Attach by PID\n"
     "%attach 12345\n"
     "# Attach only if the PID still has the start time printed by %processes\n"
     "%attach 12345 --start-time 98765",
     "PID\tProcess ID of the target on the server host. Omit it to choose from the local process picker, which needs a server owned by this REPL.\n"
     "--start-time TICKS\tLinux start time of the process, as shown by %processes. The attach is refused when the PID no longer has it, which guards against PID reuse. Must be positive.\n"},
    {"processes", "%processes [--filter TEXT]", "List local target processes that match the filter.",
     "# List every local candidate\n"
     "%processes\n"
     "# Only processes whose executable path contains the text\n"
     "%processes --filter calculator",
     "--filter TEXT\tKeep only processes whose executable path contains TEXT. Each row shows the PID, start time, executable and command line.\n"},
    {"sessions", "%sessions",
     "List sessions known to the connected server, one per line with ID, PID and state. "
     "The selected session is marked with *.",
     "%sessions", ""},
    {"use", "%use SESSION",
     "Select an attached session for subsequent commands. SESSION is an ID from %sessions or a unique prefix of one.",
     "# Select the session listed as s1\n"
     "%use s1",
     "SESSION\tSession ID from %sessions, or a unique prefix of one. Only an attached session can be selected.\n"},
    {"status", "%status", "Show connection details, the selected session, and retained collections.",
     "%status", ""},
    {"detach", "%detach [SESSION]",
     "Detach the selected or named session and remove its probes. SESSION may be a unique ID prefix.",
     "# Detach the selected session\n"
     "%detach\n"
     "# Detach a specific session\n"
     "%detach s1",
     "SESSION\tSession ID from %sessions, or a unique prefix of one. Defaults to the selected session.\n"},
    {"modules", "%modules [--details]", "List application modules. Details show executable mappings, file identity, and a mapping token.",
     "# List module paths\n"
     "%modules\n"
     "# Show executable mappings, ELF build IDs and the mapping token\n"
     "%modules --details",
     "--details\tAlso print each executable mapping with its address range, file offset, device, inode and ELF build ID, and the mapping= token. Use the path and token with %module index --module and %symbols --address.\n"},
    {"module", "", "Index ELF modules and browse or export recovered types and symbols."},
    {"module index", "%module index (--file ELF | --module PATH --mapping TOKEN) [--output INDEX] [--debug-file FILE] [--max-entries N] [--max-depth N]",
     "Create a reusable module index and select it for the current REPL. File input works offline.",
     "# Index a library offline; writes ./libdemo.so.tracer.sqlite\n"
     "%module index --file ./libdemo.so\n"
     "# Name the index and supply a separate debug file\n"
     "%module index --file ./libdemo.so --debug-file ./libdemo.so.debug --output demo.index.sqlite\n"
     "# Index a module mapped in the selected process\n"
     "%modules --details\n"
     "%module index --module /opt/app/libdemo.so --mapping TOKEN",
     "--file ELF\tLocal ELF executable or shared library to index. Works without a target. Use either --file or --module, not both.\n"
     "--module PATH\tExact path of a module mapped in the selected process, from %modules --details. Requires --mapping and the REPL's own local server.\n"
     "--mapping TOKEN\tMapping token printed by %modules --details. It is rejected if the mappings changed since then.\n"
     "--output INDEX\tIndex file to create. The default is the ELF path with .tracer.sqlite appended.\n"
     "--debug-file FILE\tMatching separate debug file to read when the ELF has no embedded DWARF.\n"
     "--max-entries N\tStop reading DWARF after N entries. The default 0 means no limit.\n"
     "--max-depth N\tLimit the DWARF nesting depth that is traversed. The default 0 means no limit.\n"},
    {"module use", "%module use INDEX", "Select an existing index for module commands and probe preparation.",
     "%module use ./libdemo.so.tracer.sqlite",
     "INDEX\tIndex file created by %module index. It becomes the default for module commands and indexed %probe prepare.\n"},
    {"module types", "%module types [--index INDEX] [--filter GLOB] [--limit N] [--page TOKEN] [--format json]",
     "Find indexed types and report their sizes when available.",
     "# Types in a namespace\n"
     "%module types --filter \"demo::*\"\n"
     "# First 20 types as JSON\n"
     "%module types --limit 20 --format json",
     "--index INDEX\tIndex to query. Defaults to the index selected by %module index or %module use, then module-index.sqlite in the working directory.\n"
     "--filter GLOB\tKeep only types whose name matches the glob, for example \"demo::*\". Quote the pattern.\n"
     "--limit N\tMaximum number of entries in one page.\n"
     "--page TOKEN\tContinue from the next_page token printed by the previous page.\n"
     "--format json\tPrint JSON instead of text lines. json is the supported format.\n"},
    {"module symbols", "%module symbols [--index INDEX] [--filter GLOB] [--exported] [--limit N] [--page TOKEN] [--format json]",
     "Find ELF symbols and distinguish exported definitions.",
     "# Exported symbols in a namespace\n"
     "%module symbols --exported --filter \"demo::*\"\n"
     "# Page through all symbols\n"
     "%module symbols --limit 50\n"
     "%module symbols --limit 50 --page TOKEN",
     "--index INDEX\tIndex to query. Defaults to the index selected by %module index or %module use, then module-index.sqlite in the working directory.\n"
     "--filter GLOB\tKeep only symbols whose name matches the glob, for example \"demo::*\". Quote the pattern.\n"
     "--exported\tList only exported definitions.\n"
     "--limit N\tMaximum number of entries in one page.\n"
     "--page TOKEN\tContinue from the next_page token printed by the previous page.\n"
     "--format json\tPrint JSON instead of text lines. json is the supported format.\n"},
    {"module show", "%module show [--index INDEX] (--type NAME | --symbol SELECTOR | --id ID)",
     "Show detailed metadata for a type or symbol.",
     "# A type by name\n"
     "%module show --type demo::Packet\n"
     "# One overload, selected by its full signature\n"
     "%module show --symbol \"demo::score(demo::Packet const&)\"",
     "--index INDEX\tIndex to query. Defaults to the selected index.\n"
     "--type NAME\tQualified C++ type name to show.\n"
     "--symbol SELECTOR\tSymbol to show. Use the full C++ signature to select an overload.\n"
     "--id ID\tType or symbol ID from %module types or %module symbols. Use it when a name is ambiguous.\n"
     "Give exactly one of --type, --symbol and --id.\n"},
    {"module emit", "%module emit [--index INDEX] [--type NAME | --symbol SELECTOR | --id ID]... --output DIR",
     "Emit recovered C++ declarations, read-only views and a manifest.",
     "# Emit the declarations needed by one function\n"
     "%module emit --symbol \"demo::score(demo::Packet const&)\" --output recovered\n"
     "# Emit a type and a function together\n"
     "%module emit --type demo::Packet --symbol \"demo::score(demo::Packet const&)\" --output recovered-all",
     "--index INDEX\tIndex to read. Defaults to the selected index.\n"
     "--type NAME\tQualified C++ type to emit. Repeat to emit several.\n"
     "--symbol SELECTOR\tSymbol to emit, by full C++ signature. Repeat to emit several.\n"
     "--id ID\tType or symbol ID from the index. Repeat to emit several.\n"
     "--output DIR\tDirectory that receives the declarations, the read-only views (views.hpp) and the manifest listing omitted fields.\n"},
    {"threads", "%threads", "List the selected target's thread IDs, names, and Linux start times.",
     "%threads", ""},
    {"symbols", "%symbols [--filter TEXT] [--module TEXT] [--limit N] [--page TOKEN]\n"
     "  [--addresses] [--mapping TOKEN] [--address PC]",
     "Find supported C++ callables. Use --addresses for verified entries or --address with the mapping token to look up a PC.",
     "# Find callables and show their verified live addresses\n"
     "%symbols --filter Calculator --limit 20 --addresses\n"
     "# Look up the callable that contains a runtime address\n"
     "%symbols --address 0xADDRESS --mapping TOKEN",
     "--filter TEXT\tKeep only callables whose selector contains TEXT.\n"
     "--module TEXT\tKeep only callables from modules whose path contains TEXT.\n"
     "--limit N\tPage size. The default 0 returns everything in one page.\n"
     "--page TOKEN\tContinue from the next_page token printed by the previous page.\n"
     "--addresses\tAlso show the verified runtime and ELF entry address of each callable, and print a mapping= token.\n"
     "--mapping TOKEN\tMapping snapshot token from an earlier %symbols --addresses or %modules --details run. Required with --address.\n"
     "--address PC\tShow the callable that contains this runtime address, decimal or 0x hex. Requires --mapping and does not support paging.\n"},
    {"live", "", "Preview, apply, inspect, and revert verified native code replacements."},
    {"live update", "%live update FILE... [--compdb FILE] [--build MANIFEST] [--retention-limit N] [--payload-limit BYTES]\n"
     "  [--managed true | --after-file FILE --after-function NAME --for-each TYPE]",
     "Publish a verified -O0 group. Local entries need a unique ELF file marker and a patchable entry.",
     "# Preview and apply saved edits as one group\n"
     "%live update source/checkout.cpp source/pricer.cpp\n"
     "# Choose the compilation database and build manifest explicitly\n"
     "%live update source/pricer.cpp --compdb build/compile_commands.json --build clang-tracer-live.textproto\n"
     "# Managed migration: recipe plus a hook that runs for each Cache\n"
     "%live update update.hpp --after-file after.cpp --after-function fixture::after --for-each Cache\n"
     "# Managed recipe without a hook\n"
     "%live update update.hpp --managed true",
     "FILE...\tSaved source or header files to publish together as one group. For a managed update, the recipe and any new layout headers or source changes.\n"
     "--compdb FILE\tCompilation database for the files. Defaults to compile_commands.json in the working directory.\n"
     "--build MANIFEST\tBuild manifest that records the original files and hashes. Defaults to clang-tracer-live.textproto in the working directory.\n"
     "--retention-limit N\tMaximum number of retained JIT generations, including reverted ones. Must be positive.\n"
     "--payload-limit BYTES\tMaximum serialized source and dependency snapshot sent to the target. The default is 16 MiB.\n"
     "--managed true\tPublish a managed layout recipe that has no after hook. Needs the application's managed registry.\n"
     "--after-file FILE\tFile that defines the after hook. Give it together with --after-function and --for-each.\n"
     "--after-function NAME\tHook function to run against the migrated instances while access is still gated.\n"
     "--for-each TYPE\tManaged type whose instances the hook visits.\n"},
    {"live preview", "%live preview FILE... [--compdb FILE] [--build MANIFEST]",
     "Show the exact native entries and blockers for a saved source snapshot.",
     "# Check what an update would replace, without applying it\n"
     "%live preview source/checkout.cpp\n"
     "%live preview source/checkout.cpp --compdb build/compile_commands.json --build clang-tracer-live.textproto",
     "FILE...\tSaved source or header files to evaluate as one group.\n"
     "--compdb FILE\tCompilation database for the files. Defaults to compile_commands.json in the working directory.\n"
     "--build MANIFEST\tBuild manifest that records the original files and hashes. Defaults to clang-tracer-live.textproto in the working directory.\n"},
    {"live status", "%live status", "List the active revision and retained JIT generation count.",
     "%live status", ""},
    {"live watch", "%live watch FILE [FILE ...] [--build FILE] [--compdb FILE]",
     "Watch saved files and automatically preview and apply eligible changes. Press Ctrl-C to stop watching.",
     "# Re-preview and apply whenever either file is saved\n"
     "%live watch source/checkout.cpp source/pricer.cpp",
     "FILE\tSource or header file to watch. Name each file once.\n"
     "--build FILE\tBuild manifest. Defaults to clang-tracer-live.textproto in the working directory.\n"
     "--compdb FILE\tCompilation database. Defaults to compile_commands.json in the working directory.\n"},
    {"live mock", "%live mock SIGNATURE --file FILE --function FUNCTION [--fixture FILE] [--bind ALIAS=SIGNATURE] [--recursion original]",
     "Compile a typed replacement into the target and activate it as a live revision.",
     "# Replace a method with a function from mock.cpp, using fixture data\n"
     "%live mock \"ProductStore::load(int)\" --file product_fixture.cpp --function load_fixture --fixture products.txt\n"
     "# Send recursive calls to the original implementation\n"
     "%live mock \"demo::fib(int)\" --file mock.cpp --function fib_mock --recursion original",
     "SIGNATURE\tExact signature of the callable to replace, with source-style or demangled qualifiers.\n"
     "--file FILE\tC++ source file that defines the replacement.\n"
     "--function FUNCTION\tName of the replacement function in FILE, optionally qualified.\n"
     "--fixture FILE\tOptional fixture file. It is snapshotted with the request for the replacement to use.\n"
     "--bind ALIAS=SIGNATURE\tMake a verified application callable available to the replacement under ALIAS. Repeat for several.\n"
     "--recursion original\tRoute recursive calls made by the original implementation back to the original code.\n"},
    {"live update-data", "%live update-data FILE [--discover-file FILE --discover-function NAME] [--module MODULE] [--address ADDRESS] [--from-scan ID ...]",
     "Run a C++ callback once per selected object with verified ownership or borrowing.",
     "# Update every object the discovery function reports\n"
     "%live update-data shared.cpp --discover-file discover.cpp --discover-function discover\n"
     "# Update the candidates found by an earlier heap scan\n"
     "%live update-data shared.cpp --from-scan SCAN_ID",
     "FILE\tC++ source file whose callback runs once for each selected object.\n"
     "--discover-file FILE\tC++ file with the discovery function. Give it together with --discover-function.\n"
     "--discover-function NAME\tFunction that reports objects through clang_tracer::ObjectSink, with their owner or borrow.\n"
     "--module MODULE\tOnly use objects from this verified module of the build manifest. A different mapped module is rejected before the callback runs.\n"
     "--address ADDRESS\tOnly update the object at this nonzero address, decimal or 0x hex.\n"
     "--from-scan ID ...\tUpdate candidates reported by %live scan-heap. The original owner of each is reacquired. Accepts several IDs.\n"},
    {"live scan-heap", "%live scan-heap --type TYPE [--type TYPE] [--module MODULE]",
     "Find candidate objects in the target heap and report the evidence for each candidate.",
     "# Scan for two types\n"
     "%live scan-heap --type example::Cache --type example::Account\n"
     "# Restrict to one module\n"
     "%live scan-heap --type example::Cache --module libcache.so",
     "--type TYPE\tQualified C++ type to search for. Repeat for several types. At least one is required.\n"
     "--module MODULE\tOnly search objects of this module.\n"
     "--discover-file FILE\tOptional C++ file with an explicit discovery function, for owners that automatic discovery does not recognize. Give it together with --discover-function.\n"
     "--discover-function NAME\tDiscovery function in --discover-file.\n"},
    {"live revert", "%live revert [REVISION]", "Restore the parent or specified revision. Revision 0 is the original build.",
     "# Go back one revision\n"
     "%live revert\n"
     "# Restore the original build\n"
     "%live revert 0",
     "REVISION\tRevision number to restore. Revision 0 is the original build. Without it, the parent of the active revision is restored.\n"},
    {"live recover", "%live recover", "Recover a managed publication in the current server session.",
     "# After a managed publication reports RESTORATION_REQUIRED\n"
     "%live recover", ""},
    {"memory", "", "List, read, or scan mapped memory."},
    {"memory maps", "%memory maps",
     "List all mapped ranges and permissions in the selected session and issue a memory snapshot token.",
     "%memory maps", ""},
    {"memory read", "%memory read ADDRESS --mapping TOKEN (--count BYTES | --type TYPE)",
     "Read up to 65536 bytes as a hex dump, or decode one little-endian u8/u16/u32/u64, "
     "i8/i16/i32/i64, f32 or f64 value.",
     "# Hex and ASCII dump of 32 bytes\n"
     "%memory read 0xADDRESS --mapping TOKEN --count 32\n"
     "# Decode one 64-bit signed value\n"
     "%memory read 0xADDRESS --mapping TOKEN --type i64",
     "ADDRESS\tNonzero start address, decimal or 0x hex, taken from a readable range in the snapshot.\n"
     "--mapping TOKEN\tToken printed after \"memory mapping=\" by %memory maps. Run %memory maps again after any mapping change.\n"
     "--count BYTES\tNumber of bytes to dump as hex and ASCII, 1 to 65536. Use this or --type.\n"
     "--type TYPE\tDecode one little-endian value instead of dumping bytes: u8, u16, u32, u64, i8, i16, i32, i64, f32 or f64.\n"},
    {"memory scan", "%memory scan ADDRESS --mapping TOKEN --count BYTES --pattern PATTERN [--limit N]",
     "Find masked byte patterns in a readable range up to 1 MiB. Use ? for wildcard nibbles. "
     "The default result limit is 64 and the maximum is 1024.",
     "# Find a pattern with wildcard nibbles in 4 KiB\n"
     "%memory scan 0xADDRESS --mapping TOKEN --count 4096 --pattern \"48 8? ?? ff\"\n"
     "# Use an explicit mask after a colon and cap the matches\n"
     "%memory scan 0xADDRESS --mapping TOKEN --count 4096 --pattern \"13 37 : 1f ff\" --limit 16",
     "ADDRESS\tNonzero start of the range, decimal or 0x hex. The whole range must be readable in the snapshot.\n"
     "--mapping TOKEN\tToken printed after \"memory mapping=\" by %memory maps.\n"
     "--count BYTES\tLength of the range to scan, 1 to 1048576 bytes.\n"
     "--pattern PATTERN\t1 to 256 hex bytes separated by spaces. ?? matches any byte, and ? matches one nibble, as in 8? or ?f. Append \": MASK\" with the same number of hex bytes to select individual bits.\n"
     "--limit N\tMaximum matches to report. The default is 64 and the maximum is 1024. truncated=true means more exist.\n"},
    {"object", "", "Watch object lifetimes, ownership, method snapshots and stores on Linux."},
    {"object prepare", "%object prepare TYPE [--executable APP] --module ELF --field FIELD --code SELECTOR --output DIRECTORY",
     "Save a reusable object definition and verified ELF bindings without attaching. "
     "For a shared-library writer, add --executable APP and keep --module LIB; a lone shared object "
     "has no process interpreter. Use --on METHOD, --include HEADER, and --synchronization for a typed method watch.",
     "# Save a write watch for a field of an executable\n"
     "%object prepare demo::PlanRecord --module ./build/repl-object-plan-target --field value --code \"demo::write_value(demo::PlanRecord &)\" --backend code-cache --output ./record-watch\n"
     "# The writer is in a shared library\n"
     "%object prepare demo::Record --executable ./build/app --module ./build/libdemo.so --field value --code \"demo::write_value(demo::Record &)\" --backend code-cache --follow-calls --output ./record-watch\n"
     "# Method snapshots for a typed receiver\n"
     "%object prepare demo::Portfolio --module ./portfolio --include ./portfolio.hpp --compdb ./compile_commands.json --translation-unit ./portfolio.cpp --on \"demo::Portfolio::rebalance()\" --field legs --key legs=id --synchronization thread-confined --output ./portfolio-watch",
     "TYPE\tQualified C++ type to watch. --type TYPE is accepted instead.\n"
     "--executable APP\tProcess executable. Needed when --module names a shared library that holds the writer.\n"
     "--module ELF\tELF file that contains the watched code and layout: the executable, or the shared library when --executable is also given.\n"
     "--field FIELD\tField to watch. A nested path such as summary.caption is allowed, and a path into a union selects the whole union. Repeat it with --on to choose the reported members.\n"
     "--code SELECTOR\tFunction that writes the field, for example \"demo::write_value(demo::Record &)\". Repeat to select more writers for a standalone main-image field.\n"
     "--output DIRECTORY\tNew directory that receives the plan (object-plan.pb) and its text definition. It must not exist.\n"
     "Also accepted:\n"
     "--on METHOD\tMethod to snapshot at verified entry and return, for a method watch. Repeat for several, up to 16. Needs --include.\n"
     "--include HEADER\tOriginal header that declares TYPE, for a method watch. Repeat for more headers.\n"
     "--policy HEADER\tHeader with a snapshot policy, for example FollowPointers specializations, added to the preparation.\n"
     "--compdb FILE\tOriginal compilation database, used for --on methods and for --discover-writers.\n"
     "--translation-unit FILE\tSource whose compile command to use. Give one for all methods or one per --on method in order.\n"
     "--key FIELD=MEMBER\tIdentity member that pairs elements of a container field between samples. Repeat as needed.\n"
     "--synchronization POLICY\tHow the sampled state is protected: thread-confined, caller-lock, or stable-at-boundaries for method snapshots.\n"
     "--discover-writers\tFind direct source writers of the field using --compdb and --translation-unit, and save them with the plan.\n"
     "--emit-source DIRECTORY\tAlso write the generated snapshot adapters (method-N.hpp) to this new directory.\n"
     "--max-depth N\tMethod snapshots: maximum nesting depth to capture. The default is 32.\n"
     "--max-snapshot-bytes N\tMethod snapshots: maximum encoded bytes per captured value, up to 65536. The default is 4096.\n"
     "--max-instances N\tMethod snapshots: receivers with an outstanding sample at once, up to 65536. The default is 1024.\n"
     "The other watch options of %object start, such as --backend, --follow-calls, --first-seen, --union-tag, --union-case, --stack-depth and --no-values, are saved into the plan.\n"},
    {"object preview", "%object preview --plan DIRECTORY",
     "Resolve the plan against the selected process and check live code bytes without installing patches.",
     "# Check the binding and the admitted store sites\n"
     "%object preview --plan ./record-watch",
     "--plan DIRECTORY\tPlan directory created by %object prepare. Preview reports the binding status and verified sites and changes nothing in the target.\n"
     "Run-time options such as --module, --stack-depth, --max-events and --no-values can be given with --plan. Other definition options cannot.\n"},
    {"object start", "%object start --type TYPE --field FIELD --code SELECTOR "
     "[--base BASE --owner OWNER --collection FIELD] [--snapshot-field FIELD] "
     "[--module MODULE] [--caller SELECTOR] [--rendezvous SYMBOL] "
     "[--backend ptrace|code-cache] [--no-values] [--stack-depth N] [--max-elements N] [--max-string-bytes N]",
     "Start a native watch with matching DWARF. Ptrace is the diagnostic default. "
     "Code cache translates supported writers and lifecycle fragments. "
     "Repeat --code to select multiple writers for a standalone main-image field. "
     "For an eligible standalone write scope, use --follow-calls to resolve callees at runtime, with --runtime-limit N to bound translations. "
     "A non-leaf shared writer in that scope requires --follow-calls. If its writer is not in the main executable, name its ELF with --module. "
     "--follow-calls is limited to standalone writes; it cannot be combined with --owner, collection or caller scopes, "
     "--writer-entry, or --first-seen unless the watched value has a supported structured layout. "
     "Use --shared-file PATH to correlate shared storage across live processes. "
     "Use --pointee PATH with a verified --mutation to follow a DWARF-resolved pointer path. "
     "On AArch64, --rendezvous selects an existing single-threaded userspace checkpoint. "
     "On x86-64, use --module for a shared writer and "
     "--caller to verify an Image::get borrow followed by an indirect call. "
     "Use --writer-entry for all entries into an admitted standalone leaf. "
     "Use --vector with --element-key FIELD=UINT32 or --map with --map-key KEY to track collection storage generations. "
     "Select a verified --mutation SYMBOL and declare --synchronization for native collection capture. "
     "Use --mode logical-writes for keyed collection changes and --mutation destroy=SYMBOL for destruction. "
     "For libstdc++ string, variant and any storage writes, use --first-seen and declare --synchronization. "
     "Select a nested union with --field PATH and supply --union-tag PATH and --union-case TAG=MEMBER for typed capture. "
     "Use --owner-add, --owner-get, --owner-remove and --owner-snapshot to select ownership boundaries. "
     "Use --no-values to record every completed write and omit field samples. "
     "Record observations with %record FILE --sqlite --name object.trace.",
     "# Trace writes to one field with the diagnostic backend and 4 stack frames\n"
     "%object start --type demo::Person --field age --code \"demo::update(demo::Person &)\" --stack-depth 4\n"
     "# Translate the writer in the native code cache\n"
     "%object start --backend code-cache --type demo::Person --field age --code person_write_many\n"
     "# Start a plan saved by %object prepare\n"
     "%object start --plan ./record-watch --stack-depth 4\n"
     "# Follow a vector element by key while the writer runs\n"
     "%object start --type demo::Portfolio --field quantity --vector legs --element-key id=12 --code \"demo::Portfolio::rebalance()\" --mutation _ZNSt6vectorIN4demo3LegESaIS1_EE7reserveEm\n"
     "# Then record the observations\n"
     "%record object-writes.db --sqlite --name object.trace --probe WATCH_ID --count 2 --timeout 30",
     "--type TYPE\tQualified C++ type whose instances are watched.\n"
     "--field FIELD\tField to watch. A nested path such as envelope.state.payload is allowed, and a path into a union selects the whole union. A write watch takes exactly one --field.\n"
     "--code SELECTOR\tFunction that writes the field, for example \"demo::update(demo::Person &)\". Repeat to select more writers for a standalone main-image field.\n"
     "--base BASE\tBase type of TYPE that the owner stores, for example demo::Shape for demo::Circle. Use with --owner and --collection.\n"
     "--owner OWNER\tType that owns the watched objects. It enables lifetime and ownership tracking through its collection.\n"
     "--collection FIELD\tOwner field that holds the watched objects, such as a vector of unique pointers.\n"
     "--snapshot-field FIELD\tOwner-managed field, for example a string, to report from its verified setter boundary.\n"
     "--module MODULE\tELF basename or absolute path of the mapped module that contains the writer, when it is not in the main executable.\n"
     "--caller SELECTOR\tMain-executable function that borrows the object from its owner and then calls the writer. On x86-64 the watch verifies the borrow and the indirect call.\n"
     "--rendezvous SYMBOL\tAArch64 code-cache only: existing main-image function where a thread can safely enter the loader.\n"
     "--backend ptrace|code-cache\tptrace (default) single-steps verified stores for diagnostics. code-cache translates supported writers in the target without a stop per write.\n"
     "--no-values\tRecord FIELD_WRITTEN for every completed store, including same-value stores, and skip value samples.\n"
     "--stack-depth N\tCapture up to N stack frames with each write, 0 to 64. The default is 0.\n"
     "--max-elements N\tElement budget for captured containers, up to 65536. The default is 4096.\n"
     "--max-string-bytes N\tByte budget for captured strings, up to 65536. The default is 4096.\n"
     "Also accepted:\n"
     "--plan DIRECTORY\tStart a plan saved by %object prepare instead of defining the watch inline. Only run-time options may accompany it.\n"
     "--mode writes|logical-writes|snapshots\tCapture mode. logical-writes records the logical effect of each selected --mutation on a --vector or --map. snapshots needs a prepared plan.\n"
     "--follow-calls\tCode-cache write watches: resolve and translate callees at runtime. A non-leaf shared writer needs it. Standalone writes only.\n"
     "--runtime-limit N\tWith --follow-calls, bound nested scope discovery and retained translations. The default is 1024, the maximum 65536.\n"
     "--writer-entry\tRequire an entry redirect at every selected standalone leaf writer, covering calls through pointers and vtables.\n"
     "--first-seen\tScope the watch to one verified writer invocation when construction is inlined, absent or ran before attach. Needed for string, variant and any writes.\n"
     "--shared-file PATH\tCorrelate writes through a writable shared file mapping across processes. Code-cache write watches.\n"
     "--pointee PATH\tFollow a pointer path such as routes.head->node to the watched --field. Needs --mutation and a declared --synchronization.\n"
     "--mutation SYMBOL\tFunction that changes the watched container or pointer path and takes it as its first argument. Repeat for several. destroy=SYMBOL declares a destroying function.\n"
     "--synchronization POLICY\tHow the sampled state is protected: thread-confined or caller-lock. Required for collection, pointee, union, first-seen and complex writes.\n"
     "--vector FIELD\tTrack a std::vector field of the type. Use --element-key to bind one element.\n"
     "--element-key FIELD=UINT32\tElement of the --vector to follow, by its 32-bit unsigned key field and value.\n"
     "--map FIELD\tTrack a std::map or std::unordered_map field. Requires --map-key.\n"
     "--map-key KEY\tKey of the --map entry to follow.\n"
     "--union-tag PATH\tDiscriminator field of the watched union. Use with --union-case.\n"
     "--union-case TAG=MEMBER\tMap a tag value to the union member it selects. Repeat for each known alternative.\n"
     "--owner-add NAME, --owner-get NAME, --owner-remove NAME, --owner-snapshot NAME\tOwner methods that act as ownership boundaries. The defaults are add, get, remove and set_description.\n"
     "--tid TID\tReport only stores by this thread.\n"
     "--max-events N\tStop after N observations. The diagnostic backend then restores its patches.\n"
     "--timeout SECONDS\tStop after this many seconds. The diagnostic backend then restores its patches.\n"
     "--event-buffer N\tSize of the code-cache event queue, at least 2. The default is 4096.\n"
     "--overflow drop|stop\tWhen the code-cache queue fills, drop new observations or end capture. Application stores continue either way.\n"
     "--stack-cfi\tUnwind stacks with CFI from the saved registers. Needs code-cache writes and --stack-depth.\n"
     "--serialize-wait-us N\tSerialize observed stores per field address for up to N microseconds. Needs code-cache writes from all threads.\n"
     "--async-signal N\tSignal number that has an application handler. Repeat for several.\n"
     "--syscall-output\tObserve read and pread when the destination is the exact 32-bit field. Diagnostic scalar watches only.\n"},
    {"object status", "%object status [WATCH_ID]", "Show admitted store sites, observations and restoration status.",
     "%object status\n"
     "%object status 3",
     "WATCH_ID\tWatch ID printed by %object start. Without it, the session's current watch is shown.\n"},
    {"object stop", "%object stop [WATCH_ID]", "Restore the watch's patches and release execution control.",
     "# Stop and confirm restoration before detaching\n"
     "%object stop 3\n"
     "%object status 3",
     "WATCH_ID\tWatch ID printed by %object start. Without it, the session's current watch is stopped. If restoration is pending, retry with the same ID.\n"},
    {"object-probe prepare", "%object-probe prepare TYPE --method METHOD --include HEADER --compdb FILE --translation-unit TU --callbacks SOURCE --output FILE",
      "Prepare an offline type package. A field write package uses --field FIELD --code WRITER.",
      "# Methods of a type, with callbacks embedded in the package\n"
      "%object-probe prepare demo::Counter --include ./counter.hpp --compdb ./compile_commands.json --translation-unit ./counter.cpp --method \"demo::Counter::add(int)\" --callbacks ./callbacks.cpp --output ./counter.object\n"
      "# Writes to one field\n"
      "%object-probe prepare demo::Counter --field value --code \"demo::Counter::set(int)\" --dwarf-writers --output ./writes.object",
      "TYPE\tQualified C++ type to register.\n"
      "--method METHOD\tExact method selector to observe, for example \"demo::Counter::add(int)\". Repeat for several.\n"
      "--include HEADER\tOriginal header that declares TYPE. Repeat for more headers.\n"
      "--compdb FILE\tOriginal compilation database. Also used to discover field writers.\n"
      "--translation-unit TU\tSource whose compile command is used. Also used to discover field writers.\n"
      "--callbacks SOURCE\tC++ source with the interceptor callbacks. It is embedded in the package.\n"
      "--output FILE\tNew package file to write. It must not exist.\n"
      "Also accepted:\n"
      "--field FIELD\tField to watch for individual writes. Select one field per package.\n"
      "--code WRITER\tFunction that writes the field. Repeat for several. Omit it to discover writers from --compdb and --translation-unit.\n"
      "--dwarf-writers\tAllow DWARF writer lookup for a write-only package prepared without a compilation database.\n"},
    {"object-probe preview", "%object-probe preview --package FILE [--module MAPPING] [start options]",
      "Bind an attached target and check patch windows. Use the same options for preview and start.",
      "%object-probe preview --package ./counter.object\n"
      "%object-probe preview --package ./writes.object --module libdemo.so --follow-calls",
      OBJECT_PROBE_START_OPTIONS},
    {"object-probe start", "%object-probe start --package FILE [--module MAPPING] [--method-module MAPPING] "
        "[--follow-calls] [--no-values] [--first-seen] [--union-tag FIELD --union-case TAG=MEMBER "
        "--synchronization thread-confined|caller-lock]",
      "Activate the verified object probe. Follow calls for write-only registrations. "
      "Use first-seen with a standalone writer and a write-only package. Repeat union-case for each admitted tag.",
      "# Start a package and note the registration ID it prints\n"
      "%object-probe start --package ./counter.object\n"
      "# Record its events\n"
      "%record counter.db --sqlite --name object.trace --probe REGISTRATION_ID --count 4 --timeout 30\n"
      "# Tagged union write watch\n"
      "%object-probe start --package ./writes.object --first-seen --union-tag kind --union-case 1=number --synchronization thread-confined",
      OBJECT_PROBE_START_OPTIONS},
    {"object-probe status", "%object-probe status [ID]", "Report group coverage and restoration.",
      "%object-probe status\n"
      "%object-probe status 2",
      "ID\tRegistration ID printed by %object-probe start. Without it, the session's current registration is reported.\n"},
    {"object-probe stop", "%object-probe stop [ID]", "Close callback admission and restore the group's patches.",
      "%object-probe stop 2",
      "ID\tRegistration ID printed by %object-probe start. Without it, the session's current registration is stopped. If restoration is pending, retry with the same ID.\n"},

    {"trace", "", "Preview, apply, or remove a bulk C++ trace."},
    {"trace preview", "%trace preview [--module GLOB] [--include GLOB] [--exclude GLOB] [--exact SIGNATURE]\n"
     "  or %trace preview --manifest FILE\n  [--save NEW_FILE]",
     "Preview bulk C++ matches, ABI status and retained probe capacity without installing probes.",
     "# Select by module and name globs, then save the rules\n"
     "%trace preview --module \"libapp*\" --include \"app::*\" --exclude \"app::detail::*\" --save trace.textproto\n"
     "# Select one overload by its full signature\n"
     "%trace preview --exact \"Calculator::multiply(int, int) const\"\n"
     "# Re-resolve saved rules against the current session\n"
     "%trace preview --manifest trace.textproto",
     "--module GLOB\tMatch executable modules by basename or path. Repeat to match any of several. Narrows duplicate signatures.\n"
     "--include GLOB\tSelect functions by qualified name or full signature. Case-sensitive, with *, ? and bracket patterns. Repeat for several. Quote the pattern.\n"
     "--exclude GLOB\tRemove matches from the --include and --exact selection. Repeat for several.\n"
     "--exact SIGNATURE\tSelect one overload by its full C++ signature. Repeat for several. Combines with --include.\n"
     "--manifest FILE\tLoad the rules from a saved manifest instead of the options above. It cannot be combined with --module, --include, --exclude or --exact.\n"
     "--save NEW_FILE\tWrite the selection rules to this new file, which must not exist. Reuse it with %trace apply.\n"
     "A preview needs at least one --include or --exact rule, or a --manifest.\n"},
    {"trace apply", "%trace apply --manifest FILE [--callback FILE]",
     "Install a typed observer for each eligible manifest match. Restore newly installed probes on partial failure.",
     "# Install the default PRE and POST observer for every match\n"
     "%trace apply --manifest trace.textproto\n"
     "# Compile a custom observer for each match\n"
     "%trace apply --manifest trace.textproto --callback observer.cpp",
     "--manifest FILE\tRules saved by %trace preview --save. They are resolved again against the selected session.\n"
     "--callback FILE\tLocal C++ observer to compile for each matched function. Without it, the default typed PRE and POST observer is used.\n"},
    {"trace remove", "%trace remove BATCH_ID",
     "Remove or retry restoration of the probes owned by a batch in the selected session.",
     "%trace remove 3f9a0c1d5b7e4a2c8d6e1f0a9b8c7d6e",
     "BATCH_ID\tBatch ID printed by %trace apply. Repeat the command to retry entries reported as RESTORE_PENDING.\n"},
    {"probes", "%probes [--cwd DIRECTORY]", "List the probes in the selected session.",
     "%probes\n"
     "%probes --cwd /work/demo",
     ""},
    {"probe", "", "Prepare, resolve, validate, install, or remove a probe."},
    {"probe entry", "%probe entry SELECTOR --mapping TOKEN [--module MODULE] [--cwd DIRECTORY]\n"
     "  or %probe entry --module PATH --elf-address ADDRESS --abi linux-x86-64-sysv --mapping TOKEN [--cwd DIRECTORY]",
     "Verify and disassemble a function entry and show the copy-only detour window and bounded control flow.",
     "# A function chosen by selector\n"
     "%memory maps\n"
     "%probe entry \"Calculator::multiply(int, int) const\" --mapping TOKEN\n"
     "# An explicit entry in a stripped module\n"
     "%probe entry --module /absolute/path/to/app --elf-address 0x401000 --abi linux-x86-64-sysv --mapping TOKEN",
     "SELECTOR\tC++ function selector, resolved through DWARF.\n"
     "--mapping TOKEN\tToken printed by %memory maps for this session. It is checked against the live mappings.\n"
     "--module MODULE\tModule to use. With a selector it disambiguates the function. With --elf-address it must be the exact mapped module path.\n"
     "--elf-address ADDRESS\tLink-time ELF virtual address, before ASLR, of an exact function symbol start or an exact .eh_frame range start.\n"
     "--abi linux-x86-64-sysv\tABI label for the explicit entry. The caller supplies it. Required with --elf-address.\n"},
    {"probe prepare",
     "%probe prepare SELECTOR --include HEADER --output NEW_DIRECTORY\n"
     "  [--compdb PATH] [--translation-unit FILE] [--module ELF] [--metadata-only] [--cwd DIRECTORY] [EXTRA COMPILER ARGUMENTS]\n"
     "  or %probe prepare SELECTOR --signature TYPE --module ELF --output NEW_DIRECTORY [--member] [--metadata-only] [--cwd DIRECTORY]"
     "\n  or %probe prepare SELECTOR [--module-index INDEX] [--module ELF] [--id ID] [--generated DIR] --output NEW_DIRECTORY",
     "Build a reusable probe package from source, an explicit signature, or the module index. "
     "Without headers or an explicit signature, preparation uses the default index. "
     "Repeat --include for extra headers. "
     "Preparation uses compile_commands.json in the command's working directory by default. "
     "It prefers the first include's recorded compile command and falls back to the first source beside it. "
     "Use --module to pin the package to a specific build. "
     "Prepared headers can declare tracer_snapshot::FollowPointers<Record> specializations listing direct raw pointer fields to follow; pointee types and polymorphic implementations are discovered from the included source inputs. This policy applies to prepared argument, receiver and result records. "
     "Use --snapshot-return-pointee when injecting a raw pointer-return probe. "
     "Use --metadata-only to skip the separate C++ adapter files. "
     "With --signature, supply the C++ result and explicit parameter types, for example 'const int&()'. "
     "Qualified methods receive an opaque receiver. Pass --member for an unqualified instance method.",
     "# From source: the first include declares the function, later ones supply types\n"
     "%probe prepare \"HFW::Item::create()\" --include item.hpp --include item-impl.hpp --include item-snapshot-policy.hpp --output prepared\n"
     "# With an explicit compilation database and translation unit\n"
     "%probe prepare \"demo::compute(int)\" --include src/compute.hpp --compdb build/compile_commands.json --translation-unit src/compute.cpp --output prepared\n"
     "# From an explicit signature and the ELF, without headers\n"
     "%probe prepare \"demo::Counter::value\" --signature \"const int&()\" --module ./app --output prepared-value\n"
     "# From a module index\n"
     "%probe prepare \"demo::score(demo::Packet const&)\" --generated recovered --output prepared-score",
     "SELECTOR\tC++ function to prepare, for example \"demo::compute(int)\". Use the full signature to select an overload.\n"
     "--include HEADER\tHeader that declares the function. The first one supplies the declaration. Repeat it for headers that define the types a callback uses. Order matters.\n"
     "--output NEW_DIRECTORY\tNew directory for the package. It must not exist. The package is saved as prepared.pb with its adapter files.\n"
     "--compdb PATH\tCompilation database file or directory for the headers. Defaults to compile_commands.json in the working directory.\n"
     "--translation-unit FILE\tSelect an exact compile command when the first include's command is not unique or not recorded.\n"
     "--module ELF\tPin the package to this ELF build. In signature mode, the ELF to read; in index mode, overrides the ELF path recorded in the index.\n"
     "--metadata-only\tSkip the separate C++ adapter files. The package still embeds its adapter.\n"
     "[EXTRA COMPILER ARGUMENTS]\tSupplemental include paths and macros for the preparation. See Extra compiler arguments below.\n"
     "--signature TYPE\tC++ function type that gives the result and explicit parameter types, for example 'const int&()'. Requires --module and no source options.\n"
     "--member\tTreat an unqualified selector as an instance method. Only with --signature.\n"
     "--module-index INDEX\tModule index to prepare from. Defaults to the sidecar of --module, then the selected index, then module-index.sqlite in the working directory.\n"
     "--id ID\tSelect an indexed symbol by ID when the name is ambiguous.\n"
     "--generated DIR\tDirectory of C++ emitted by %module emit. Optional, the preparer can regenerate it from the index.\n"},
    {"probe resolve", "%probe resolve SELECTOR [RESOLUTION OPTIONS] [--cwd DIRECTORY]",
     "Resolve a selector in the selected target and show the available metadata.",
     "# Resolve by selector\n"
     "%probe resolve \"Calculator::multiply(int, int) const\"\n"
     "# Resolve with a prepared package\n"
     "%probe resolve \"demo::compute(int)\" --prepared prepared --prepared-mode require",
     "SELECTOR\tC++ function selector to resolve. Use the full signature to select an overload.\n"
     "[RESOLUTION OPTIONS]\tHow to find the function and its types. See Resolution options below.\n"},
    {"probe validate", "%probe validate SELECTOR --callback FILE [RESOLUTION OPTIONS] [--cwd DIRECTORY]",
     "Check whether the callback can be deployed for the selected function.",
     "# Compile and link the callback without installing it\n"
     "%probe validate \"Calculator::multiply(int, int) const\" --callback hook.cpp\n"
     "# Validate against a prepared package\n"
     "%probe validate \"demo::score(demo::Packet const&)\" --callback observer.cpp --prepared prepared-score --prepared-mode require",
     "SELECTOR\tC++ function selector the callback would observe.\n"
     "--callback FILE\tLocal C++ source of the callback, up to 4 MiB.\n"
     "[RESOLUTION OPTIONS]\tHow to find the function and its types. See Resolution options below.\n"},
    {"probe inject", "%probe inject SELECTOR [--callback FILE] [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS] [--cwd DIRECTORY]",
     "Install a probe on the selected target. Omit --callback to capture events with the built-in observer. "
     "The auto backend tries the verified entry first, then a verified --caller or an entry trap for supported short getters. "
     "Use --backend entry to require an entry jump. "
     "For prepared implementation member snapshots, a callback file can define tracer_snapshot::encode(const Implementation&) noexcept to return a custom record by value. "
     "Use --snapshot-return-pointee with a matching record layout and live, stable pointees immediately after return. "
     "result() gives the address and result_as<T>() decodes an owned copy.",
     "# Capture events with the built-in observer\n"
     "%probe inject \"Calculator::multiply(int, int) const\"\n"
     "# Custom callback and a 16-frame entry stack\n"
     "%probe inject \"Calculator::multiply(int, int) const\" --callback method.cpp --stack-depth 16\n"
     "# Require the prepared package\n"
     "%probe inject \"demo::compute(int)\" --callback hook.cpp --prepared prepared --prepared-mode require\n"
     "# Short getter through a verified caller\n"
     "%probe inject \"demo::Circle::get_radius() const\" --backend auto --caller \"demo::call_radius(const demo::Circle &)\" --stack-depth 6\n"
     "# Capture the pointee of a returned raw pointer\n"
     "%probe inject \"prepared_pointee::HfwManager::createItem(int)\" --prepared prepared --snapshot-return-pointee",
     "SELECTOR\tC++ function selector to probe. Use the full signature to select an overload.\n"
     "--callback FILE\tLocal C++ source of the observer, up to 4 MiB. When omitted, the built-in observer records events, including receiver snapshots when the class layout is known.\n"
     "[RESOLUTION OPTIONS]\tHow to find the function and its types. See Resolution options below.\n"
     "[DEPLOYMENT OPTIONS]\tHow much to capture with each event. See Deployment options below.\n"
     "Also accepted:\n"
     "--backend auto|entry\tauto (default) tries the verified entry jump, then a verified --caller or an entry trap for supported short getters. entry requires an entry jump.\n"
     "--caller SELECTOR\tNamed function in the same module that calls a short getter, for the call-site backend. Needs the auto backend.\n"},
    {"probe remove", "%probe remove ID [--cwd DIRECTORY]", "Remove a probe from the selected session and restore the original code.",
     "%probes\n"
     "%probe remove 1",
     "ID\tProbe ID printed by %probe inject or listed by %probes. It must be positive.\n"},
    {"probe reload", "%probe reload ID --callback FILE [--cwd DIRECTORY]",
     "Switch an active probe to a new observer generation while existing calls finish on their original generation.",
     "# Replace the observer of probe 1; its binding and deployment options stay\n"
     "%probe reload 1 --callback observer-v2.cpp",
     "ID\tProbe ID of the active probe, from %probes. Use the new ID the command prints for later filtering and removal.\n"
     "--callback FILE\tLocal C++ source of the new observer. A rejected callback leaves the existing observer active.\n"},
    {"probes", "%probes [--cwd DIRECTORY]", "List the probes in the selected session.",
     "%probes\n"
     "%probes --cwd /work/demo",
     ""},
    {"interceptor", "", "Manage named C++ callback drafts."},
    {"interceptor load", "%interceptor load FILE --as NAME", "Load a local C++ file into a named draft.",
     "%interceptor load method.cpp --as multiply_hook",
     "FILE\tLocal C++ source file, up to 4 MiB.\n"
     "--as NAME\tName of the draft to create or replace.\n"},
    {"interceptor edit", "%interceptor edit NAME", "Enter C++ source interactively. Use .end to save or .cancel to discard.",
     "%interceptor edit multiply_hook",
     "NAME\tName of the draft to create or replace. Type the source, then .end to save or .cancel to discard.\n"},
    {"interceptor show", "%interceptor show NAME", "Print the source of a named draft.",
     "%interceptor show multiply_hook",
     "NAME\tName of an existing draft.\n"},
    {"interceptor validate", "%interceptor validate NAME --target SELECTOR [RESOLUTION OPTIONS]",
     "Compile and link the named draft for the selected target without activating a probe.",
     "%interceptor validate multiply_hook --target \"Calculator::multiply(int, int) const\"",
     "NAME\tName of an existing draft.\n"
     "--target SELECTOR\tC++ function selector the draft would observe.\n"
     "[RESOLUTION OPTIONS]\tHow to find the function and its types. See Resolution options below.\n"},
    {"interceptor upload", "%interceptor upload NAME --target SELECTOR [RESOLUTION OPTIONS] [DEPLOYMENT OPTIONS]",
     "Install the named draft on the selected target.",
     "# Install a draft\n"
     "%interceptor upload hook --target \"demo::compute(int)\"\n"
     "# Capture a 16-frame stack with each event\n"
     "%interceptor upload multiply_hook --target \"Calculator::multiply(int, int) const\" --stack-depth 16",
     "NAME\tName of an existing draft. The draft stays available after a deployment error.\n"
     "--target SELECTOR\tC++ function selector to probe.\n"
     "[RESOLUTION OPTIONS]\tHow to find the function and its types. See Resolution options below.\n"
     "[DEPLOYMENT OPTIONS]\tHow much to capture with each event. See Deployment options below.\n"
     "Also accepted:\n"
     "--backend auto|entry\tSame as for %probe inject.\n"
     "--caller SELECTOR\tSame as for %probe inject.\n"},

    {"events", "%events [STREAM OPTIONS] [--as NAME]",
     "Print live events and keep a bounded collection named live by default. Use --callback-errors exclude or only to select callback outcomes.",
     "# Print and retain five POST events that reported a callback error\n"
     "%events --count 5 --phase POST --callback-errors only\n"
     "# Stream until Ctrl+C, keeping the events in a named collection\n"
     "%events --as watch",
     "[STREAM OPTIONS]\tWhich events to receive and when to stop. See Stream options below.\n"
     "--as NAME\tName of the retained collection. The default is live. A collection of the same name is replaced.\n"
     "--callback-errors include|exclude|only\tinclude (default) keeps every event, exclude drops callback failures, only keeps just the callback failures.\n"},
    {"events list", "%events list NAME [--limit N]", "List retained events with their zero-based indexes. The default limit is 20.",
     "%events list batch\n"
     "%events list batch --limit 100",
     "NAME\tCollection created by %events, %collect or %events open.\n"
     "--limit N\tMaximum number of events to list. The default is 20, and 0 lists all of them.\n"},
    {"events show", "%events show NAME --index N", "Print one retained event and its captured values.",
     "%events show batch --index 0",
     "NAME\tCollection created by %events, %collect or %events open.\n"
     "--index N\tZero-based position of the event, as shown by %events list.\n"},
    {"events diff", "%events diff NAME --pre N --post N [--key PATH=MEMBER]...",
     "Compare owned receiver snapshots for one call. Use --key for a verified element identity.",
     "# Compare the receiver before and after one call\n"
     "%events diff saved --pre 0 --post 1\n"
     "# Pair container elements by an identity member\n"
     "%events diff saved --pre 0 --post 1 --key legs=id",
     "NAME\tCollection that holds both events.\n"
     "--pre N\tZero-based index of the PRE event of the call.\n"
     "--post N\tZero-based index of the POST event of the same call.\n"
     "--key PATH=MEMBER\tIdentify the elements of the container at PATH by their member MEMBER, so elements are paired by identity rather than position. Repeat for several containers.\n"},
    {"events stack", "%events stack NAME --index N",
     "Print all captured stack frames for one retained event.",
     "%events stack fullstack --index 0",
     "NAME\tCollection created by %events, %collect or %events open.\n"
     "--index N\tZero-based position of the event, as shown by %events list.\n"},
    {"events calls", "%events calls NAME", "Pair PRE and POST events and show call durations and missing halves.",
     "%events calls batch",
     "NAME\tCollection created by %events, %collect or %events open.\n"},
    {"events stats", "%events stats NAME",
     "Summarize live or replayed calls per probe, with duration histograms, caller edges and loss.",
     "# Caller edges need stacks captured at injection, for example --stack-depth 8\n"
     "%events stats batch",
     "NAME\tCollection created by %events, %collect or %events open.\n"},
    {"events clear", "%events clear NAME", "Remove a named collection and release its buffer space.",
     "%events clear batch",
     "NAME\tCollection to remove. Its capacity becomes available to other collections.\n"},
    {"events open", "%events open FILE --as NAME [--callback-errors include|exclude|only]",
     "Open a Protobuf or SQLite recording offline as a bounded, named collection. Compatible C++ types stored in SQLite load automatically.",
     "# Open a SQLite recording\n"
     "%events open capture.db --as saved --callback-errors only\n"
     "# Open a framed Protobuf recording\n"
     "%events open capture.pb --as results",
     "FILE\tFramed Protobuf recording or SQLite database.\n"
     "--as NAME\tName for the new collection. It must not already exist. Use %events clear NAME first to reuse a name.\n"
     "--callback-errors include|exclude|only\tinclude (default) loads every event, exclude drops callback failures, only loads just the callback failures.\n"},
    {"events export", "%events export NAME --format json --output NEW_FILE.json",
     "Write the retained events to a new JSON file.",
     "%events export saved --format json --output events.json",
     "NAME\tCollection to export.\n"
     "--format json\tExport format. json is the only supported format.\n"
     "--output NEW_FILE.json\tFile to create. It must not exist.\n"},
    {"collect", "%collect [STREAM OPTIONS] [--as NAME]", "Capture events into a bounded collection named batch by default. Use --callback-errors exclude or only to select successful callbacks or callback failures.",
     "# Collect ten POST events, then inspect them\n"
     "%collect --count 10 --timeout 15 --phase POST --callback-errors only --as failures\n"
     "%events list failures",
     "[STREAM OPTIONS]\tWhich events to receive and when to stop. See Stream options below.\n"
     "--as NAME\tName of the retained collection. The default is batch. A collection of the same name is replaced.\n"
     "--callback-errors include|exclude|only\tinclude (default) keeps every event, exclude keeps only successful callbacks, only keeps just the callback failures.\n"},
    {"record", "%record FILE [--sqlite] [STREAM OPTIONS]",
     "Record framed Protobuf by default. With --sqlite, create or append to a SQLite recording and save available prepared C++ types for replay. Use --callback-errors exclude or only to select callback outcomes.",
     "# Record 12 events of one name to SQLite\n"
     "%record capture.db --sqlite --name account.adjusted --callback-errors only --count 12\n"
     "# Record framed Protobuf from one probe\n"
     "%record capture.pb --probe 1 --count 100",
     "FILE\tRecording to write. A framed Protobuf file must not exist. A SQLite database is created or appended to.\n"
     "--sqlite\tWrite SQLite instead of framed Protobuf. Events are deduplicated by server instance and sequence, and available prepared C++ types are saved for replay.\n"
     "[STREAM OPTIONS]\tWhich events to record and when to stop. See Stream options below. --as does not apply to a recording.\n"
     "--callback-errors include|exclude|only\tinclude (default) records every event, exclude skips callback failures, only records just the callback failures.\n"},

    {"cpp", "", "Load C++, configure the local compiler, or generate mappings for captured values."},
    {"cpp load", "%cpp load FILE", "Evaluate a local C++ source file in the current interpreter.",
     "%cpp load analysis.cpp",
     "FILE\tLocal C++ source file. It is evaluated in the current interpreter, and its declarations stay available.\n"},
    {"cpp reset", "%cpp reset", "Clear C++ variables, declarations, and background range handles. Tracing sessions stay attached.",
     "%cpp reset", ""},
    {"cpp import", "%cpp import HEADER [--type TYPE] [--output FILE]",
     "Generate, cache, and load mappings for typed event decoding. Repeat --type to select several records. "
     "Nested dependencies are included automatically. Import failures preserve earlier variables.",
     "# Import one record type\n"
     "%cpp import application/types.hpp --type app::Order\n"
     "# Import several and keep the generated header\n"
     "%cpp import application/types.hpp --type app::Order --type app::Line --output order-mappings.hpp",
     "HEADER\tHeader that defines the application types.\n"
     "--type TYPE\tQualified record type to map. Repeat for several. Without it the header's own types are mapped. For a header with unsupported types, select the roots with --type.\n"
     "--output FILE\tAlso save the generated mapping header.\n"},
    {"cpp generate", "%cpp generate HEADER [--type TYPE] [--output FILE]",
     "Generate mappings from the header's AST. Use --output to save a header for #include or %cpp load. "
     "Automatic mappings require supported public fields and accessible default construction.",
     "# Save mappings for reuse\n"
     "%cpp generate application/types.hpp --output mappings.hpp\n"
     "# Only selected records\n"
     "%cpp generate application/types.hpp --type app::Order --output order-mappings.hpp",
     "HEADER\tHeader that defines the application types.\n"
     "--type TYPE\tQualified record type to map. Repeat for several. Without it the header's own types are mapped. For a header with unsupported types, select the roots with --type.\n"
     "--output FILE\tSave the generated header, to use with #include or %cpp load.\n"},
    {"cpp options", "%cpp options", "List the effective local compiler arguments in order. "
     "Use --cpp-arg ARG at startup. Probe compilation uses separate options on the server.",
     "%cpp options", ""},
    {"cpp options add", "%cpp options add ARG...", "Add compiler arguments before the first C++ input. "
     "Quote flags with spaces. Backticks run a shell command and add flags from its output.",
     "# Add include and macro flags\n"
     "%cpp options add \"-I/path/to/include\" \"-DFEATURE=1\"\n"
     "# Take flags from a command\n"
     "%cpp options add `pkg-config --cflags mylib`",
     "ARG...\tOne or more compiler arguments, each as its own token. Quote an argument that contains spaces. A backtick command contributes the flags it prints.\n"},
    {"cpp options clear", "%cpp options clear", "Clear custom local compiler arguments. Use %cpp reset first after C++ input.",
     "%cpp reset\n"
     "%cpp options clear", ""},
    {"cpp options compdb", "%cpp options compdb DATABASE --file FILE",
     "Load flags from an exact compilation database entry or a C++ source in the same directory.",
     "%cpp options compdb build/compile_commands.json --file src/target.cpp",
     "DATABASE\tPath of a compile_commands.json file.\n"
     "--file FILE\tSource file whose entry supplies the flags. If it has no entry, a C++ source in the same directory is used.\n"},
    {"plugin", "", "Load and run trusted local C++ plugins in the REPL."},
    {"plugin load", "%plugin load FILE --as NAME", "Compile or reuse a cached object and load it in its own JIT scope.",
     "%plugin load ./my_plugin.cpp --as my_plugin",
     "FILE\tC++ plugin source file.\n"
     "--as NAME\tName to load the plugin under. Use it with %plugin run and %plugin reload.\n"},
    {"plugin enable", "%plugin enable FILE --as NAME", "Load a trusted C++ plugin and remember it for future REPL sessions.",
     "%plugin enable ./my_plugin.cpp --as my_plugin",
     "FILE\tC++ plugin source file. Its absolute path is remembered.\n"
     "--as NAME\tName to load the plugin under, now and at each startup.\n"},
    {"plugin disable", "%plugin disable NAME", "Stop loading a plugin automatically in future REPL sessions.",
     "%plugin disable my_plugin",
     "NAME\tName given to %plugin enable. A plugin that is loaded now stays loaded until the REPL exits.\n"},
    {"plugin reload", "%plugin reload NAME", "Stage a fresh generation for a loaded plugin and replace it if loading succeeds.",
     "%plugin reload my_plugin",
     "NAME\tName of a loaded plugin. The previous generation is kept when the new one fails to load.\n"},
    {"plugin list", "%plugin list", "List loaded plugins and their active generations.",
     "%plugin list", ""},
    {"plugin run", "%plugin run NAME COMMAND", "Run a command registered by a loaded plugin.",
     "%plugin run my_plugin version",
     "NAME\tName of a loaded plugin.\n"
     "COMMAND\tCommand the plugin registered. %plugin commands NAME lists them. The result is printed as NAME.COMMAND = value.\n"},
    {"plugin commands", "%plugin commands NAME", "Show a plugin's commands and their help.",
     "%plugin commands my_plugin",
     "NAME\tName of a loaded plugin.\n"},
    {"plugin agent", "", "Load and run trusted C++ plugins inside the selected target."},
    {"plugin agent load", "%plugin agent load FILE --as NAME", "Load C++ source in the selected target's injected agent.",
     "%plugin agent load ./agent_plugin.cpp --as agent_plugin",
     "FILE\tC++ plugin source file on the REPL host.\n"
     "--as NAME\tName to load the plugin under in the target.\n"},
    {"plugin agent reload", "%plugin agent reload NAME [FILE]", "Reload an installed agent plugin from its manifest. Provide FILE to select a source or reload a standalone plugin.",
     "# Reload an installed plugin package\n"
     "%plugin agent reload agent_plugin\n"
     "# Reload from a changed source file\n"
     "%plugin agent reload agent_plugin ./agent_plugin.cpp",
     "NAME\tName of a plugin loaded in the target.\n"
     "FILE\tSource file to reload from. Required for a plugin that has no installed package.\n"},
    {"plugin agent run", "%plugin agent run NAME COMMAND", "Invoke a plugin command inside the selected target.",
     "%plugin agent run agent_plugin status",
     "NAME\tName of a plugin loaded in the target.\n"
     "COMMAND\tCommand that the plugin registered.\n"},
    {"shell", "%shell \"COMMAND\"", "Run a local shell command and print its output.",
     "%shell \"pwd\"\n"
     "%shell \"ls -l *.db\"",
     "\"COMMAND\"\tShell command line to run on the REPL host. Quote it. Its output is printed, and a nonzero exit status is reported as an error.\n"},
    {"complete", "%complete \"C++ PREFIX\"", "List semantic C++ completions without evaluating the expression.",
     "%complete \"tracer.\"\n"
     "%complete \"tracer.pr\"",
     "\"C++ PREFIX\"\tC++ text to complete, quoted. Nothing is evaluated.\n"},
    {"type", "%type SYMBOL", "Show the type or signatures of a symbol in the current C++ session.",
     "%type tracer.events_async\n"
     "%type session.pid",
     "SYMBOL\tName of a variable, function or member in the current C++ session. Overloads are all shown. Nothing is evaluated.\n"},
    {"help", "%help [COMMAND | api [NAME]]", "Browse commands and C++ APIs or look up a specific topic.",
     "# The command index\n"
     "%help\n"
     "# One command or subcommand\n"
     "%help probe inject\n"
     "# The C++ API index, then one topic\n"
     "%help api\n"
     "%help api Event",
     "COMMAND\tCommand or subcommand topic, with or without a leading %, for example probe inject or events open.\n"
     "api\tShow the C++ API index instead of a command.\n"
     "NAME\tAPI topic after api, for example Event, tracer.record or filter.\n"},
    {"quit", "%quit | %exit", "Exit, detach sessions owned by this REPL, and stop its private server.",
     "%quit", ""},
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
     "Use callback_error() and callback_error_message() to inspect callback failures. "
     "Use argument_as<T>(index) or result_as<T>() when T matches the captured type. "
     "has_result() includes void returns. Use argument_snapshot(), receiver_snapshot(), or state_snapshot() "
     "to inspect captured fields without rebuilding an application type.", "Event"},
    {"Collection", "A retained set of events with capture counters and status.",
     R"(auto data = tracer.batch("results");
std::cout << data.received << " received, " << data.evicted << " evicted\n";
if (data.size()) tracer.print(data.at(0));)",
     "Use size() for the retained count and at() for checked indexing. "
     "complete, cancelled, and timed_out describe how the capture ended.", "Collection"},
    {"EventRange", "A synchronous or background capture handle with snapshot iteration.",
     R"(auto live = tracer.events_async(session, 10, 30);
auto data = live.snapshot();
for (const auto& e : data) tracer.print(e);
if (live.running()) live.stop();
std::cout << live.error() << '\n';)",
     "Iteration reads a stable snapshot of currently retained events. Later snapshots may include new arrivals. "
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
    {"Event::result_as", "Decode a captured return value as an owned C++ value.",
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
     "Set names, phase, pid, session_id, callback_errors, or count to narrow the read. Names match exact function "
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
    {"adaptors", "Filter, slice, and transform Collection and EventRange values with pipelines.",
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

constexpr std::size_t help_width = 100;
constexpr std::size_t label_limit = 30;
constexpr string_view dim = "\033[2m";

// Writes text starting at the current column, wrapping at help_width and
// indenting continuation lines.
void wrapped(std::ostream& out, string_view text, std::size_t indent, std::size_t column) {
  bool first_word = true;
  while (true) {
    text.remove_prefix(std::min(text.find_first_not_of(' '), text.size()));
    if (text.empty()) break;
    const auto end = text.find(' ');
    const auto word = text.substr(0, end);
    if (!first_word && column + 1 + word.size() > help_width) {
      out << '\n' << std::string(indent, ' ');
      column = indent;
      first_word = true;
    }
    if (!first_word) {
      out << ' ';
      ++column;
    }
    out << word;
    column += word.size();
    first_word = false;
    if (end == string_view::npos) break;
    text.remove_prefix(end);
  }
  out << '\n';
}

struct OptionRow {
  string_view label;
  string_view text;
};

// Rows are "LABEL<TAB>DESCRIPTION" lines. A line without a tab is a note.
std::vector<OptionRow> option_rows(string_view rows) {
  std::vector<OptionRow> result;
  while (!rows.empty()) {
    const auto end = rows.find('\n');
    const auto line = rows.substr(0, end);
    const auto tab = line.find('\t');
    if (!line.empty())
      result.push_back(tab == string_view::npos ? OptionRow{{}, line}
                                                : OptionRow{line.substr(0, tab), line.substr(tab + 1)});
    if (end == string_view::npos) break;
    rows.remove_prefix(end + 1);
  }
  return result;
}

void option_block(std::ostream& out, std::vector<OptionRow> rows, bool color) {
  std::size_t width = 0;
  for (const auto& row : rows)
    if (row.label.size() <= label_limit) width = std::max(width, row.label.size());
  const auto indent = 2 + width + 2;
  bool first = true;
  for (const auto& row : rows) {
    if (row.label.empty()) {
      if (!first) out << '\n';
      out << "  ";
      wrapped(out, row.text, 2, 2);
    } else if (row.label.size() <= label_limit) {
      out << "  ";
      styled(out, row.label, cyan, color);
      out << std::string(width - row.label.size() + 2, ' ');
      wrapped(out, row.text, indent, indent);
    } else {
      out << "  ";
      styled(out, row.label, cyan, color);
      out << '\n' << std::string(indent, ' ');
      wrapped(out, row.text, indent, indent);
    }
    first = false;
  }
}

void examples_block(std::ostream& out, string_view text, bool color) {
  while (!text.empty()) {
    const auto newline = text.find('\n');
    const auto line = text.substr(0, newline);
    if (!line.empty() && line.front() == '#') {
      out << "  ";
      styled(out, line, dim, color);
    } else {
      out << "  ";
      print_code(out, line, Syntax::command, color);
    }
    out << '\n';
    if (newline == string_view::npos) break;
    text.remove_prefix(newline + 1);
  }
}

void command_details(std::ostream& out, const CommandTopic& entry, bool color) {
  heading(out, "%" + std::string(entry.name), color);
  out << entry.summary << '\n';
  const bool has_cwd = entry.usage.find("--cwd DIRECTORY") != string_view::npos;
  if (!entry.usage.empty()) {
    out << '\n';
    heading(out, "Usage", color);
    block(out, entry.usage, Syntax::command, color);
    out << '\n';
    heading(out, "Options", color);
    auto rows = option_rows(entry.options);
    if (has_cwd)
      rows.push_back({"--cwd DIRECTORY",
                      "Directory on the REPL host that is the base for relative paths and backtick "
                      "shell expansions in this command."});
    if (rows.empty())
      out << "  None. This command takes no arguments.\n";
    else
      option_block(out, std::move(rows), color);
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
  if (entry.name == "probe" || has_cwd) {
    out << "\n--cwd DIRECTORY sets the base for relative paths and shell expansions for this command.\n"
           "Server-side inputs must be available at the resulting paths on the server host.\n";
  }
  if (entry.usage.find("STREAM OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Stream options", color);
    option_block(out, option_rows(stream_option_rows), color);
    out << "The selected session is the default. Without a count or timeout, press Ctrl+C to stop.\n";
  }
  if (entry.usage.find("RESOLUTION OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Resolution options", color);
    option_block(out, option_rows(resolution_option_rows), color);
    out << "The default mode, prefer, uses an available prepared package and falls back to DWARF.\n";
    out << "With a typed prepared package, --compdb supplies include paths, forced includes and macros to the callback.\n"
           "Extra headers must coexist with the prepared declarations. Use --translation-unit to override the donor.\n";
  }
  if (entry.usage.find("EXTRA COMPILER ARGUMENTS") != string_view::npos ||
      entry.usage.find("RESOLUTION OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Extra compiler arguments", color);
    option_block(out, option_rows(extra_argument_rows), color);
    out << "Repeat these options to add fallback include paths and new macro definitions. "
           "Compilation database settings take priority.\n"
           "Set startup defaults with --extra-arg=ARG or --extra-args \"$(my-conf --cxxflags)\".\n";
  }
  if (entry.usage.find("DEPLOYMENT OPTIONS") != string_view::npos) {
    out << '\n';
    heading(out, "Deployment options", color);
    option_block(out, option_rows(deployment_option_rows), color);
  }
  if (!entry.example.empty()) {
    out << '\n';
    heading(out, "Examples", color);
    examples_block(out, entry.example, color);
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
