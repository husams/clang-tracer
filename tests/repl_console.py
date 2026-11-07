"""Executable-level native console checks; no Python tracing dependency."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time


def main():
    console, fixture = map(os.path.abspath, sys.argv[1:3])
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
    server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert server.stdout.readline().strip() == "ready"
        with tempfile.TemporaryDirectory(prefix="tracer-repl-console-") as directory:
            root = Path(directory)
            callback = root / "client only callback.cpp"
            callback.write_text("// client-only interceptor\n")
            capture = root / "capture.pb"
            filtered = root / "filtered.db"
            timed_recording = root / "timed.db"
            export = root / "events.json"
            commands = f'''%attach 7
%interceptor load "{callback}" --as hook
%interceptor upload hook --target "multiply(int, int)"
%collect --count 4 --as batch
%events show batch --index 1
%events calls batch
auto data = tracer.batch("batch");
std::cout << "event-print-start" << std::endl;
tracer.print(data.at(1));
std::cout << "event-print-end" << std::endl;
std::cout << "exact-print-start" << std::endl;
tracer.print(data.at(0))
std::cout << "exact-print-end" << std::endl;
std::cout << "cpp-size=" << data.size() << " result=" << data.at(1).result() << std::endl;
int typed_sum = data.at(0).argument_as<int>(0) + data.at(1).result_as<int>();
std::cout << "typed-sum=" << typed_sum << std::endl;
try {{ data.at(1).result_as<bool>(); }} catch (const std::invalid_argument&) {{ std::cout << "typed-mismatch-rejected" << std::endl; }}
auto captured = session;
auto another = tracer.collect(captured, 4);
std::cout << "collected-cpp=" << another.size() << std::endl;
%cpp reset
std::cout << "retained-session=" << session.pid << std::endl;
auto live_range = tracer.events_async(session, 4, 5);
std::cout << "background-prompt-ready" << std::endl;
%help EventRange
%cpp reset
auto sync_range = tracer.events(session, 4, 5);
int range_count = 0;
for (const auto &event : sync_range) {{ ++range_count; }}
std::cout << "sync-range-count=" << range_count << std::endl;
auto selected_events = sync_range | filter([](const auto& e) {{ return e.phase() == "POST"; }}) | take(1);
int selected_count = 0;
for (const auto& e : selected_events) {{ if (e.sequence() != 2) throw std::runtime_error("wrong filtered event"); ++selected_count; }}
auto first_event = sync_range | drop(1) | first();
if (!first_event || first_event->sequence() != 2) throw std::runtime_error("first failed");
int prefix_count = 0;
for (const auto& e : sync_range.snapshot() | take_while([](const auto& e) {{ return e.sequence() < 3; }})) {{ ++prefix_count; }}
std::cout << "adaptors=" << selected_count << ":" << prefix_count << std::endl;
auto sequences = sync_range | filter([](const auto& e) {{ return e.phase() == "POST"; }}) | transform([](const auto& e) {{ return e.sequence(); }}) | to_vector();
if (sequences.size() != 2 || sequences.at(1) != 4) throw std::runtime_error("transform failed");
auto tail = sequences | drop_while([](auto n) {{ return n < 4; }}) | to_vector();
if (tail.size() != 1 || tail.at(0) != 4) throw std::runtime_error("drop_while failed");
auto is_post = [](const auto& e) {{ return e.phase() == "POST"; }};
for (const auto& e : sync_range | filter(is_post) | take(1)) {{ if (e.sequence() != 2) throw std::runtime_error("inline filter failed"); }}
auto backwards = sync_range | reverse();
if (backwards.at(0).sequence() != 4) throw std::runtime_error("reverse failed");
int numbered = 0;
for (const auto& [index, e] : sync_range | stride(2) | enumerate()) {{ if (e.sequence() != index * 2 + 1) throw std::runtime_error("enumerate failed"); ++numbered; }}
if (numbered != 2) throw std::runtime_error("stride failed");
%cpp reset
%record "{capture}" --count 4
%events open "{capture}" --as saved
%events export saved --format json --output "{export}"
%events stats saved
%record "{filtered}" --sqlite --phase POST --count 2
tracer_repl::RecordOptions options;
options.phase = "POST";
options.count = 2;
auto recorded = tracer.record("{filtered}", options);
if (recorded.received != 2 || recorded.cursor != 4 || !recorded.complete) throw std::runtime_error("wrong recording result");
auto filtered_recording = tracer.replay("{filtered}");
if (filtered_recording.size() != 2 || filtered_recording.at(0).sequence() != 2 || filtered_recording.at(1).sequence() != 4) throw std::runtime_error("wrong recorded phases");
ReplayMarker replay_marker{{42}};
if (replay_marker.value != 42) throw std::runtime_error("filtered adapter was lost");
options.after_sequence = 900;
options.timeout_seconds = 1;
auto timed_recording = tracer.record("{timed_recording}", options);
if (timed_recording.received || timed_recording.cursor != 900 || timed_recording.complete || !timed_recording.timed_out || timed_recording.cancelled) throw std::runtime_error("wrong recording timeout result");
%help api tracer.record
tracer.detach(session)
%quit
'''
            run = subprocess.run([console, "--endpoint", endpoint, "--fail-fast"], input=commands, text=True, capture_output=True, timeout=45)
            assert run.returncode == 0, (run.stdout, run.stderr)
            for expected in ["installed probe=5", "cpp-size=4 result=42", "typed-sum=63", "typed-mismatch-rejected", "collected-cpp=4", "retained-session=7", "duration_ns=10", "paired=2", "detached session=s1", "background-prompt-ready", "sync-range-count=4", "EventRange", "adaptors=1:2"]:
                assert expected in run.stdout, (expected, run.stdout, run.stderr)
            assert len(json.loads(export.read_text())) == 4
            assert "Record events and available prepared C++ types to SQLite." in run.stdout
            database = root / "capture with spaces.db"
            subprocess.run([fixture, "--write-replay-db", str(database)], check=True, timeout=10)
            before_replay = database.read_bytes()
            replay = subprocess.run(
                [console, "--fail-fast"],
                input=f'''%events open "{database}" --as saved
%events stats saved
auto all_saved = tracer.batch("saved");
if (all_saved.size() != 4 || all_saved.at(2).sequence() != 10) throw std::runtime_error("wrong replay order");
tracer_repl::EventQuery query;
query.names = {{"missing.event", "multiply(int, int)"}};
query.phase = "POST";
query.count = 1;
auto earliest = tracer.replay("{database}", query);
if (earliest.size() != 1 || earliest.received != 1 || earliest.at(0).sequence() != 2) throw std::runtime_error("wrong replay count");
query.pid = 7;
query.session_id = "s1";
auto matches = tracer.replay("{database}", query);
if (matches.size() != 1 || matches.at(0).sequence() != 2 || matches.at(0).result_as<int>() != 42) throw std::runtime_error("wrong filtered result");
auto framed = tracer.replay("{capture}", query);
if (framed.size() != 1 || framed.at(0).sequence() != 2) throw std::runtime_error("wrong framed result");
query.names = {{"custom.event"}};
query.pid = 8;
query.session_id = "s2";
auto custom = tracer.replay("{database}", query);
if (custom.size() != 1 || custom.at(0).sequence() != 11 || custom.at(0).state_snapshot().field("label").as<std::string>() != "saved") throw std::runtime_error("wrong custom event state");
query.session_id = "s1";
if (tracer.replay("{database}", query).size()) throw std::runtime_error("replay filter ignored session");
%help api tracer.replay
%quit
''', text=True, capture_output=True, timeout=30)
            assert replay.returncode == 0, (replay.stdout, replay.stderr)
            assert "opened events=4 retained=4 evicted=0" in replay.stdout, replay.stdout
            assert "Query a Protobuf or SQLite recording offline" in replay.stdout, replay.stdout
            assert database.read_bytes() == before_replay
            unsafe = root / "unsafe.db"
            subprocess.run([fixture, "--write-unsafe-replay-db", str(unsafe)], check=True, timeout=10)
            rejected = subprocess.run(
                [console, "--fail-fast"],
                input=f'''%events open "{unsafe}" --as saved
auto saved = tracer.batch("saved");
if (saved.at(3).state_snapshot().field("label").as<std::string>() != "saved") throw std::runtime_error("snapshot lost");
%quit
''', text=True, capture_output=True, timeout=30)
            assert rejected.returncode == 0, (rejected.stdout, rejected.stderr)
            assert "recorded adapter must contain declarations only" in rejected.stderr, rejected.stderr
            printed = run.stdout.split("event-print-start\n", 1)[1].split("event-print-end", 1)[0]
            assert "phase: POST" in printed and "signed_value: 42" in printed, printed
            printed = run.stdout.split("exact-print-start\n", 1)[1].split("exact-print-end", 1)[0]
            assert "phase: PRE" in printed and "signed_value: 21" in printed, printed
            discovery = subprocess.run([console, "--fail-fast"], input='''%help
%help %probe inject
%help api
%help api tracer_repl::Event
%help api tracer.events_async
%help Event
%complete "tracer.pr"
tracer_repl::Collection empty;
%complete "empty.rec"
tracer.print(empty)
%status
%quit
''', text=True, capture_output=True, timeout=20)
            assert discovery.returncode == 0, (discovery.stdout, discovery.stderr)
            for expected in ("Connection and targets", "Capture and replay", "Resolution options",
                             "Types and objects", "tracer methods", "One captured event", "Example",
                             "T result_as() const", "auto live = tracer.events_async(session, 10, 15);",
                             "  print", "  received", "Collection retained=0", "ready=unconnected"):
                assert expected in discovery.stdout, (expected, discovery.stdout, discovery.stderr)
            assert "\x1b[" not in discovery.stdout
            assert "Collection {" not in discovery.stdout
            types = subprocess.run([console, "--fail-fast"], input='''std::string text = "hello";
auto title = text;
std::basic_string<char> basic;
std::map<std::string, int> counts;
std::map<int, int, std::greater<int>> descending;
std::vector<std::basic_string<char>> nested;
decltype(text) same_type;
const decltype(text) immutable = text;
namespace sample { int choose(int value); }
namespace sample { double choose(double value); }
using sample::choose;
%type text
%type title
%type basic
%type counts
%type descending
%type nested
%type same_type
%type immutable
%type std::string
%type std::map
%type sample::choose
%type choose
%type tracer.events_async
%type session.pid
%quit
''', text=True, capture_output=True, timeout=20)
            assert types.returncode == 0, (types.stdout, types.stderr)
            for expected in ("std::string text;", "std::string title;", "std::string basic;",
                             "std::map<std::string, int> counts;",
                             "std::map<int, int, std::greater<int>> descending;",
                             "std::vector<std::string> nested;", "std::string same_type;",
                             "const std::string immutable;",
                             "\nstd::string;\n", "\nstd::map;\n",
                             "events_async(", "pid;"):
                assert expected in types.stdout, (expected, types.stdout)
            assert types.stdout.count("choose(int value)") == 2, types.stdout
            assert types.stdout.count("choose(double value)") == 2, types.stdout
            assert "\x1b[" not in types.stdout and "__cxx11" not in types.stdout, types.stdout
            adaptors = ("filter", "take", "drop", "take_while", "drop_while", "transform",
                        "stride", "enumerate", "reverse", "first", "to_vector")
            topics = subprocess.run(
                [console, "--fail-fast"],
                input="".join(f"%help api {name}\n" for name in adaptors) + "%quit\n",
                text=True, capture_output=True, timeout=10)
            assert topics.returncode == 0, (topics.stdout, topics.stderr)
            pages = topics.stdout.split("\nExample\n")
            assert len(pages) == len(adaptors) + 1, topics.stdout
            for name, page in zip(adaptors, pages):
                assert name in page.splitlines(), (name, page)
            drop_page = pages[adaptors.index("drop") + 1]
            assert "| drop(5) | take(10)" in drop_page, drop_page
            assert "std::invalid_argument" in drop_page and "empty view" in drop_page, drop_page
            for command, message in [("%help missing", "Use %help for commands"),
                                     ("%help api Missing", "Use %help api to list topics"),
                                     ("%type missing", "unknown C++ symbol"),
                                     ("%type tracer.events_async()", "expects a C++ symbol")]:
                unknown = subprocess.run([console, "--fail-fast"], input=command + "\n",
                                         text=True, capture_output=True, timeout=10)
                assert unknown.returncode == 1 and message in unknown.stderr, unknown.stderr
            temporary = subprocess.run([console, "--fail-fast"], input='''tracer_repl::Collection{}.size()
std::cout << "temporary-collection-ok" << std::endl;
%quit
''', text=True, capture_output=True, timeout=20)
            assert temporary.returncode == 0 and "temporary-collection-ok" in temporary.stdout, (temporary.stdout, temporary.stderr)
            # Disk recording must remain available when named memory collections are full.
            disk_only = root / "full-buffer-recording.pb"
            full = subprocess.run([console, "--endpoint", endpoint, "--buffer-events", "4", "--fail-fast"],
                input=f'%attach 7\n%collect --count 4 --as full\n%record "{disk_only}" --count 4\n%quit\n',
                text=True, capture_output=True, timeout=15)
            assert full.returncode == 0 and disk_only.stat().st_size > 0, (full.stdout, full.stderr)
            # A partial C++ input must leave commands responsive.
            rejected = subprocess.run([console], input='''int kept = 42;
int broken = ;
for (int e : {1,2}) { int unfinished = 0;
std::cout << "recovered=" << kept << std::endl;
%complete "kep"
%status
%quit
''', text=True, capture_output=True, timeout=20)
            assert rejected.returncode == 1 and "ready=unconnected" in rejected.stdout
            assert "recovered=42" in rejected.stdout and "  kept" in rejected.stdout, (rejected.stdout, rejected.stderr)
            assert rejected.stderr.count("error:") == 2 and "%cpp reset" not in rejected.stderr, rejected.stderr
            malformed_loop = subprocess.run([console], input='''std::vector<int> values; for (const auto& value : values) (void)value
int recovered_after_loop = 42;
std::cout << "loop-recovered=" << recovered_after_loop << std::endl;
std::vector<int> values; for (const auto& value : values) (void)value;
%status
%quit
''', text=True, capture_output=True, timeout=20)
            assert malformed_loop.returncode == 1, (malformed_loop.stdout, malformed_loop.stderr)
            assert "expected ';'" in malformed_loop.stderr and "loop-recovered=42" in malformed_loop.stdout, (malformed_loop.stdout, malformed_loop.stderr)
            assert "redefinition" not in malformed_loop.stderr and "reset" not in malformed_loop.stderr, malformed_loop.stderr
            # Explicit unreachable endpoint never starts a local replacement.
            unavailable = subprocess.run([console, "--endpoint", "unix:///nonexistent/tracer.sock", "--fail-fast"], input="%sessions\n", text=True, capture_output=True, timeout=10)
            assert unavailable.returncode == 1 and "server ready" not in unavailable.stdout
            # SIGINT cancels a truly blocked stream, then permits another command.
            waiting = subprocess.Popen([console, "--endpoint", endpoint], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            waiting.stdin.write("%attach 7\n%collect --after 900 --as stopped\n")
            waiting.stdin.flush()
            while "attached session=" not in waiting.stdout.readline():
                assert waiting.poll() is None
            time.sleep(0.2)
            waiting.send_signal(signal.SIGINT)
            waiting.stdin.write("%status\n%quit\n")
            waiting.stdin.flush()
            stdout, stderr = waiting.communicate(timeout=10)
            assert waiting.returncode == 0, (stdout, stderr)
            assert "cancelled=true" in stdout and "ready=true" in stdout, (stdout, stderr)
            assert server.poll() is None, "console stopped an external service"
    finally:
        server.terminate()
        server.communicate(timeout=10)
    # A live stream remains open while later C++ cells and commands execute.
    live_server = subprocess.Popen([fixture, "--listen-live", endpoint],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True)
    try:
        assert live_server.stdout.readline().strip() == "ready"
        commands = '''%attach 7
auto live = tracer.events_async(session);
live.snapshot().size()
if (!live.running()) throw std::runtime_error("background stopped");
std::cout << "prompt-usable-during-capture" << std::endl;
%sessions
live.stop();
if (live.running()) throw std::runtime_error("stop failed");
%cpp reset
auto live_again = tracer.events_async(session);
%cpp reset
auto sync = tracer.events(session, 4, 5);
if (!sync.snapshot().complete) throw std::runtime_error("incomplete sync range");
int visited = 0;
for (const auto &event : sync) { ++visited; }
std::cout << "live-sync-visited=" << visited << std::endl;
%quit
'''
        result = subprocess.run([console, "--endpoint", endpoint, "--fail-fast"],
                                input=commands, text=True, capture_output=True,
                                timeout=45)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert "prompt-usable-during-capture" in result.stdout, result.stdout
        assert "live-sync-visited=4" in result.stdout, result.stdout
    finally:
        live_server.terminate()
        live_server.communicate(timeout=10)
    print("native console executable checks passed")


if __name__ == "__main__":
    main()
