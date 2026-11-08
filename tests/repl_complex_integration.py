"""Capture strings and nested C++ records from an actual Linux process."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "target", "header"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    header = json.dumps(str(Path(args.header).resolve()))
    mappings = f'''#include {header}
template<> struct tracer_repl::EventRecord<repl_fixture::Location> {{ static constexpr const char* type_name="repl_fixture::Location"; static auto fields() {{ return std::make_tuple(tracer_repl::event_member("code", &repl_fixture::Location::code),tracer_repl::event_member("name", &repl_fixture::Location::name)); }} }};
template<> struct tracer_repl::EventRecord<repl_fixture::Record> {{ static constexpr const char* type_name="repl_fixture::Record"; static auto fields() {{ return std::make_tuple(tracer_repl::event_member("id", &repl_fixture::Record::id),tracer_repl::event_member("label", &repl_fixture::Record::label),tracer_repl::event_member("location", &repl_fixture::Record::location),tracer_repl::event_member("samples", &repl_fixture::Record::samples),tracer_repl::event_member("tags", &repl_fixture::Record::tags),tracer_repl::event_member("serial", &repl_fixture::Record::serial)); }} }};
'''
    with tempfile.TemporaryDirectory(prefix="tracer-complex-") as directory:
        root = Path(directory)
        hook = root / "hook.cpp"
        hook.write_text(f'#include {header}\nstruct Interceptor {{ void pre(const Call&) {{}} void post(const Call&) {{}} }};\n')
        shared = root / "shared.cpp"
        shared.write_text(f'''#include {header}
struct SnapshotScope {{}};
struct SnapshotState : state<SnapshotScope> {{ int calls=0; }};
struct Interceptor {{ Interceptor(State<SnapshotState>); void post(const Call& call, SnapshotState& state) {{ ++state.calls; call.context().event("snapshot.shared"); }} }};
''')
        recording = root / "records.pb"
        with (root / "target.log").open("w+") as target_log:
            target = subprocess.Popen([args.target], stdout=target_log, stderr=subprocess.STDOUT)

            def run(selector, checks, *, callback=hook, extra="", record=False):
                commands = f'''%probe inject "repl_fixture::{selector}" --callback "{callback}" --include {header} {extra}
%collect --count 15 --phase POST --timeout 20 --as captured
{mappings}
auto data=tracer.batch("captured");
{checks}
'''
                if record:
                    commands += f'%record "{recording}" --count 4 --phase POST --timeout 15\n'
                commands += '%detach\nstd::cout << "complex-case-ok" << std::endl;\n%quit\n'
                result = subprocess.run([args.repl, "--server", args.server, "--attach", str(target.pid), "--timeout", "60", "--fail-fast"],
                                        input=commands, text=True, capture_output=True, timeout=180)
                assert result.returncode == 0 and "complex-case-ok" in result.stdout, (selector, result.stdout[-12000:], result.stderr[-12000:])
                assert target.poll() is None, (selector, "tracing stopped the target")
                print(f"{selector}: typed capture passed")

            record_check = '''for (const auto& e:data) { auto a=e.argument_as<repl_fixture::Record>(0); auto r=e.result_as<repl_fixture::Record>(); if(a.id!=7 || r.id!=8 || a.label!="heap-backed snapshot label longer than SSO" || a.location.name!="nested" || r.location.name!="nested-returned" || a.samples!=std::vector<int>({1,3,5}) || a.tags.at("")!=0 || a.tags.at("key")!=2 || r.tags.at("key")!=42 || !a.serial.has_value() || *a.serial!=0 || r.serial.has_value()) throw std::runtime_error("record mismatch"); }
auto owned=data.at(0).argument_as<repl_fixture::Record>(0);
'''
            try:
                run("string_argument", '''int empty=0,sso=0,heap=0,binary=0,limited=0;
for (const auto& e:data) { try { auto s=e.argument_as<std::string>(0); if(e.result_as<bool>()!=s.empty()) throw std::logic_error("string/bool mismatch"); if(s.empty()) ++empty; else if(s=="short") ++sso; else if(s==std::string("a\\0\\xffz",4)) ++binary; else if(s=="heap-backed string longer than SSO") ++heap; else throw std::logic_error("wrong string bytes"); } catch(const std::runtime_error& error) { if(std::string(error.what()).find("budget")==std::string::npos) throw; ++limited; } }
if(!empty || !sso || !heap || !binary || !limited) throw std::runtime_error("string cases missing");
''')
                run("string_result", '''for(const auto& e:data) if(e.argument_as<std::string>(0)!="return-input" || e.result_as<std::string>()!="return-input-returned") throw std::runtime_error("string result mismatch");
''')
                run("round_trip", record_check, record=True)
                run("mutate_argument", '''for(const auto& e:data) { auto a=e.argument_as<repl_fixture::Record>(0); if(a.id!=7 || a.label!="heap-backed snapshot label longer than SSO" || a.samples!=std::vector<int>({1,3,5}) || !e.result_as<bool>()) throw std::runtime_error("entry snapshot was mutated/freed"); }
''')
                run("round_trip", record_check + """
if (data.at(0).state_snapshot().field("calls").as<int>() < 1) throw std::runtime_error("marked state snapshot missing");
""", callback=shared, extra="--print-arguments --print-result")
            finally:
                target.terminate()
                target.wait(timeout=10)
        # No process or server remains: replay must reconstruct fully owned data.
        replay = subprocess.run([args.repl, "--endpoint", "127.0.0.1:1", "--fail-fast"],
            input=f'%events open "{recording}" --as captured\n{mappings}\nauto data=tracer.batch("captured");\n{record_check}\nstd::cout << "offline-complex-ok" << std::endl;\n%quit\n',
            text=True, capture_output=True, timeout=90)
        assert replay.returncode == 0 and "offline-complex-ok" in replay.stdout, (replay.stdout, replay.stderr)
        print("offline record reconstruction after target exit passed")
    print("native complex typed event integration passed")


if __name__ == "__main__":
    main()
