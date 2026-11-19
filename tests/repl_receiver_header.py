"""Receiver headers must coexist with typed self() and field-name inference."""

from contextlib import ExitStack
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


RECEIVER_CALLBACK = """class Interceptor {
public:
  void pre(const Call &call) {
    call.field(call.self()->total_cents);
    call.field(call.self()->charges);
    call.field(call.self()->rate);
    call.field(call.self()->frozen);
    call.field("cents_arg", call.arg<long>(0));
  }
  void post(const Call &call) {
    call.field("charges_after", call.self()->charges);
    call.field("total_after", call.self()->total_cents);
    call.field("returned", call.result<long>());
  }
};
"""

RECORD_CALLBACK = """class Interceptor {
public:
  void pre(const Call &call) {
    call.field("requested", call.argument<0>().cents);
  }
  void post(const Call &call) {
    call.field("quoted", call.result().cents);
  }
};
"""

METHOD_CALLBACK = """class Interceptor {
public:
  void pre(const Call &call) {
    call.field("getter", call.self()->total());
    auto getter = &receiver_header::Meter::total;
    call.field("member_getter", (call.self()->*getter)());
    call.field("overload", call.self()->charge());
    auto overload = static_cast<long (receiver_header::Meter::*)() const>(
        &receiver_header::Meter::charge);
    call.field("member_overload", (call.self()->*overload)());
    call.field("nested_result", call.self()->bump(1));
  }
  void post(const Call &call) {
    call.field("charges_after", call.self()->charges);
    call.field("total_after", call.self()->total());
    call.field("returned", call.result<long>());
  }
};
"""


def main(repl, server, agent, target, source, compdb):
    source = Path(source).resolve()
    cases = ("dwarf", "header", "header-compdb", "header-named", "header-methods",
             "header-methods-compdb", "records", "records-compdb")
    for case in cases:
        with tempfile.TemporaryDirectory(prefix=f"ct-receiver-{case}-") as directory, ExitStack() as stack:
            root = Path(directory)
            records = case.startswith("records")
            methods = case.startswith("header-methods")
            total, charges = (41, 2) if methods else (40, 1)
            env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"))
            env.pop("CLANG_TRACER_ENDPOINT", None)
            callback = root / "observer.cpp"
            code = RECORD_CALLBACK if records else RECEIVER_CALLBACK
            if methods:
                code = METHOD_CALLBACK
            if case in ("header-compdb", "header-methods-compdb"):
                code = "#if RECEIVER_HEADER_FLAG != 73\n#error missing donor flags\n#endif\n" + code
            if case == "header-named":
                for field in ("total_cents", "charges", "rate", "frozen"):
                    code = code.replace(f"call.field(call.self()->{field})",
                                        f'call.field("{field}", call.self()->{field})')
            callback.write_text(code)
            child = subprocess.Popen([target], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, env=env)
            stack.callback(stop, child)
            target_output = Transcript(child.stdout)
            target_output.expect(r"^ready$")
            console = subprocess.Popen(
                [repl, "--server", server, "--agent", agent, "--attach", str(child.pid),
                 "--fail-fast"], cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, env=env)
            stack.callback(stop, console)
            output = Transcript(console.stdout)

            def send(command):
                console.stdin.write(command + "\n")
                console.stdin.flush()

            def trigger(letter, pattern):
                child.stdin.write(letter + "\n")
                child.stdin.flush()
                target_output.expect(pattern)

            output.expect(rf"^attached session=\S+ pid={child.pid}$", timeout=60)
            # Select the one-argument overload by its Itanium symbol; GCC and
            # Clang spell its scalar parameter differently in DWARF.
            selector = "receiver_header::quote" if records else "_ZN15receiver_header5Meter6chargeEl"
            options = " --prepared-mode off"
            if case != "dwarf":
                header = source.parent / ("types.hpp" if records else "meter.hpp")
                options += f' --include "{header}"'
            if case in ("header-compdb", "header-methods-compdb", "records-compdb"):
                options += f' --compdb "{compdb}"'
            send(f'%probe inject "{selector}" --callback "{callback}"{options}')
            probe = output.expect(r"^installed probe=(\d+)", timeout=60)[1]
            trigger("q" if records else "c",
                    r"^quoted cents=47$" if records else
                    rf"^charged total={total} charges={charges} result={total}$")
            send(f"%collect --probe {probe} --count 2 --timeout 10 --as captured")
            cursor = output.expect(r"^received=2 cursor=(\d+) .*complete=true")[1]
            checks = """auto captured = tracer.batch("captured");
if (captured.size() != 2 || captured.at(0).phase() != "PRE" || captured.at(1).phase() != "POST" || captured.at(0).call_id() != captured.at(1).call_id()) throw std::runtime_error("missing matching PRE/POST events");
if (captured.at(0).callback_error() || captured.at(1).callback_error()) throw std::runtime_error("callback failed");
"""
            if not records:
                checks += f'if (captured.at(0).argument_as<long>(0) != 40 || captured.at(1).result_as<long>() != {total}) throw std::runtime_error("wrong argument/result");\n'
            for line in checks.splitlines():
                send(line)
            send("for (const auto& event : captured) tracer.print(event);")
            send('std::cout << "receiver-checks-passed" << std::endl;')
            output.expect(r"^receiver-checks-passed$")
            transcript = "".join(output.seen)
            fields = {"requested": 40, "quoted": 47} if records else {
                "cents_arg": 40, "charges_after": 1, "total_after": 40, "returned": 40}
            if methods:
                fields = {"getter": 0, "member_getter": 0, "overload": 0,
                          "member_overload": 0, "nested_result": 2, "charges_after": charges,
                          "total_after": total, "returned": total}
            for name, value in fields.items():
                encoded = r'\s+(?:un)?signed_value: ' + str(value) + r'\b'
                assert re.search(r'object_fields\s*\{\s*name: "' + name +
                                 r'"\s*value\s*\{\s*type: \w+' + encoded,
                                 transcript), transcript
            if not records and not methods:
                for name in ("total_cents", "charges", "rate", "frozen"):
                    assert f'name: "{name}"' in transcript, transcript
                for name in ("total_cents", "charges", "frozen"):
                    value = re.search(r'object_fields\s*\{\s*name: "' + name +
                                      r'"\s*value\s*\{([^}]*)\}', transcript)
                    assert value and not re.search(r'(?:un)?signed_value: [1-9]|bool_value: true',
                                                   value[1]), transcript
                assert re.search(r'name: "rate"\s*value\s*\{\s*type: FLOAT64\s+real_value: 0.25',
                                 transcript), transcript
            if methods:
                # bump() executes charge() once while the PRE hook is active.
                # Existing callback-depth suppression must prevent nested events.
                send(f"%collect --probe {probe} --after {cursor} --count 1 --timeout 1 --as nested")
                output.expect(r"^received=0 .*complete=false")
            send(f"%probe remove {probe}")
            output.expect(rf"^removed probe={probe} restored=true$")
            trigger("q" if records else "c",
                    r"^quoted cents=47$" if records else
                    rf"^charged total={total + 40} charges={charges + 1} result={total + 40}$")
            send("%detach")
            output.expect(r"^detached session=")
            send("%quit")
            assert console.wait(timeout=30) == 0, "".join(output.seen)
            assert child.poll() is None, "target stopped after probe removal"
            if case == "header-compdb":
                # Header definitions must preserve the same read-only access
                # and target-method guards as imported DWARF declarations.
                commands = []
                expected_guards = {}
                helper = root / "helper.hpp"
                helper.write_text('#include "' + str(source.parent / "meter.hpp") +
                                  '"\ninline void invoke_target(const receiver_header::Meter *self) { '
                                  'const_cast<receiver_header::Meter *>(self)->charge(1); }\n')
                guard_options = f' --prepared-mode off --include "{source.parent / "meter.hpp"}"'
                for label, body, expected in (
                    ("private", "(void)call.self()->hidden;", r"hidden.*private|private.*hidden"),
                    ("mutate", "call.self()->total_cents = 1;", r"read.only|const"),
                    ("invoke", "(void)const_cast<receiver_header::Meter *>(call.self())->charge(1);",
                     r"cannot call.*probed method"),
                    ("invoke_without_self", "auto *receiver = reinterpret_cast<receiver_header::Meter *>"
                     "(call.object_address()); (void)receiver->charge(1);", r"cannot call.*probed method"),
                    ("address", "auto method = static_cast<long (receiver_header::Meter::*)(long)>"
                     "(&receiver_header::Meter::charge); (void)method;", r"cannot call.*probed method"),
                    ("legacy_address", "auto method = static_cast<long (receiver_header::Meter::*)(long)>"
                     "(&receiver_header::Meter::charge); (void)method;", r"cannot call.*probed method"),
                    ("helper", "invoke_target(call.self());", r"cannot call.*probed method"),
                    ("copy", "auto copy = *call.self(); (void)copy;", r"cannot construct.*receiver"),
                    ("construct", "receiver_header::Meter value; (void)value;", r"cannot construct.*receiver"),
                    ("new", "(void)new receiver_header::Meter;", r"cannot (allocate|construct).*receiver"),
                    ("delete", "delete call.self();", r"cannot destroy.*receiver"),
                    ("static", "(void)receiver_header::Meter::static_total;", r"cannot odr-use.*static"),
                ):
                    invalid = root / f"{label}.cpp"
                    prefix = f'#include "{helper}"\n' if label == "helper" else ""
                    if label == "legacy_address":
                        invalid.write_text('extern "C" long long trace_callback(const TraceContext&) { '
                                           + body + ' return 0; }\n')
                    else:
                        if label in ("construct", "new", "static"):
                            body = "(void)call.self(); " + body
                        invalid.write_text(prefix + "class Interceptor { public: void pre(const Call &call) { "
                                           + body + " } };\n")
                    commands.append(f'std::cout << "GUARD_{label}" << std::endl;')
                    commands.append(f'%probe validate "{selector}" --callback "{invalid}"{guard_options}')
                    expected_guards[label] = expected
                mismatch = root / "mismatched.hpp"
                mismatch.write_text((source.parent / "meter.hpp").read_text().replace(
                    "std::int64_t total_cents", "std::int32_t total_cents"))
                commands.append('std::cout << "GUARD_layout" << std::endl;')
                commands.append(f'%probe validate "{selector}" --callback "{invalid}"'
                                f' --prepared-mode off --include "{mismatch}"')
                expected_guards["layout"] = r"does not match DWARF"
                accepted_labels = ("getter_address", "legacy_getter_address")
                getter_address = ("auto getter = &receiver_header::Meter::total; (void)getter; "
                                  "auto overload = static_cast<long (receiver_header::Meter::*)() const>"
                                  "(&receiver_header::Meter::charge); (void)overload;")
                for label in accepted_labels:
                    valid = root / f"{label}.cpp"
                    if label.startswith("legacy"):
                        code = ('extern "C" long long trace_callback(const TraceContext&) { '
                                + getter_address + ' return 0; }\n')
                    else:
                        code = ('class Interceptor { public: void pre(const Call&) { '
                                + getter_address + ' } };\n')
                    valid.write_text(code)
                    commands.append(f'std::cout << "GUARD_{label}" << std::endl;')
                    commands.append(f'%probe validate "{selector}" --callback "{valid}"{guard_options}')
                commands.append("%quit")
                rejected = subprocess.run(
                    [repl, "--server", server, "--agent", agent, "--attach", str(child.pid)],
                    input="\n".join(commands) + "\n", cwd=root, env=env,
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
                diagnostics = rejected.stdout
                assert rejected.returncode != 0, diagnostics
                for label, expected in expected_guards.items():
                    section = re.search(rf'^GUARD_{label}\n(.*?)(?=^GUARD_\w+\n|\Z)',
                                        diagnostics, re.M | re.S)
                    assert section and "error:" in section[1], diagnostics
                    assert "success: true" not in section[1], diagnostics
                    assert re.search(expected, section[1], re.I), diagnostics
                for label in accepted_labels:
                    section = re.search(rf'^GUARD_{label}\n(.*?)(?=^GUARD_\w+\n|\Z)',
                                        diagnostics, re.M | re.S)
                    assert section and "success: true" in section[1], diagnostics
                    assert "error:" not in section[1], diagnostics
                assert "redefinition" not in diagnostics, diagnostics
                trigger("c", r"^charged total=120 charges=3 result=120$")
            print(f"receiver header case {case}: injection, fields, events and restoration passed")


if __name__ == "__main__":
    main(*sys.argv[1:])
