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


def main(repl, server, agent, target, source, compdb):
    source = Path(source).resolve()
    cases = ("dwarf", "header", "header-compdb", "header-named", "records", "records-compdb")
    for case in cases:
        with tempfile.TemporaryDirectory(prefix=f"ct-receiver-{case}-") as directory, ExitStack() as stack:
            root = Path(directory)
            records = case.startswith("records")
            env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"))
            env.pop("CLANG_TRACER_ENDPOINT", None)
            callback = root / "observer.cpp"
            code = RECORD_CALLBACK if records else RECEIVER_CALLBACK
            if case == "header-compdb":
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
            # GCC spells the scalar parameter "long int", Clang spells it
            # "long". These unique names exercise the same DWARF-selected ABI.
            selector = "receiver_header::quote" if records else "receiver_header::Meter::charge"
            options = " --prepared-mode off"
            if case != "dwarf":
                header = source.parent / ("types.hpp" if records else "meter.hpp")
                options += f' --include "{header}"'
            if case in ("header-compdb", "records-compdb"):
                options += f' --compdb "{compdb}"'
            send(f'%probe inject "{selector}" --callback "{callback}"{options}')
            probe = output.expect(r"^installed probe=(\d+)", timeout=60)[1]
            trigger("q" if records else "c",
                    r"^quoted cents=47$" if records else r"^charged total=40 charges=1 result=40$")
            send(f"%collect --probe {probe} --count 2 --timeout 10 --as captured")
            output.expect(r"^received=2 .*complete=true")
            checks = """auto captured = tracer.batch("captured");
if (captured.size() != 2 || captured.at(0).phase() != "PRE" || captured.at(1).phase() != "POST" || captured.at(0).call_id() != captured.at(1).call_id()) throw std::runtime_error("missing matching PRE/POST events");
if (captured.at(0).callback_error() || captured.at(1).callback_error()) throw std::runtime_error("callback failed");
"""
            if not records:
                checks += """if (captured.at(0).argument_as<long>(0) != 40 || captured.at(1).result_as<long>() != 40) throw std::runtime_error("wrong argument/result");
"""
            for line in checks.splitlines():
                send(line)
            send("for (const auto& event : captured) tracer.print(event);")
            send('std::cout << "receiver-checks-passed" << std::endl;')
            output.expect(r"^receiver-checks-passed$")
            transcript = "".join(output.seen)
            fields = {"requested": 40, "quoted": 47} if records else {
                "cents_arg": 40, "charges_after": 1, "total_after": 40, "returned": 40}
            for name, value in fields.items():
                assert re.search(r'object_fields\s*\{\s*name: "' + name +
                                 r'"\s*value\s*\{\s*type: \w+\s+(?:un)?signed_value: ' +
                                 str(value) + r'\b', transcript), transcript
            if not records:
                for name in ("total_cents", "charges", "rate", "frozen"):
                    assert f'name: "{name}"' in transcript, transcript
                for name in ("total_cents", "charges", "frozen"):
                    value = re.search(r'object_fields\s*\{\s*name: "' + name +
                                      r'"\s*value\s*\{([^}]*)\}', transcript)
                    assert value and not re.search(r'(?:un)?signed_value: [1-9]|bool_value: true',
                                                   value[1]), transcript
                assert re.search(r'name: "rate"\s*value\s*\{\s*type: FLOAT64\s+real_value: 0.25',
                                 transcript), transcript
            send(f"%probe remove {probe}")
            output.expect(rf"^removed probe={probe} restored=true$")
            trigger("q" if records else "c",
                    r"^quoted cents=47$" if records else r"^charged total=80 charges=2 result=80$")
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
                guard_options = f' --prepared-mode off --include "{source.parent / "meter.hpp"}"'
                for label, body, expected in (
                    ("private", "(void)call.self()->hidden;", r"hidden.*private|private.*hidden"),
                    ("mutate", "call.self()->total_cents = 1;", r"read.only|const"),
                    ("invoke", "(void)call.self()->total();", r"cannot call.*receiver"),
                    ("copy", "auto copy = *call.self(); (void)copy;", r"cannot construct.*receiver"),
                ):
                    invalid = root / f"{label}.cpp"
                    invalid.write_text("class Interceptor { public: void pre(const Call &call) { "
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
                assert "redefinition" not in diagnostics, diagnostics
                trigger("c", r"^charged total=120 charges=3 result=120$")
            print(f"receiver header case {case}: injection, fields, events and restoration passed")


if __name__ == "__main__":
    main(*sys.argv[1:])
