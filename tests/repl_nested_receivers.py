"""Methods of classes nested in classes are DWARF receivers, with and without real headers.

The fixture (tests/fixtures/repl_nested_receivers.*) calls one method of each of: a class nested
in a class, a sibling nested in the same class, a class nested two levels deep, and a plain
class (the control). Each probe publishes the receiver's member and the call result.
"""

import argparse
from contextlib import ExitStack
from pathlib import Path
import re
import subprocess
import tempfile

from repl_browser_integration import Transcript, stop

FIXTURES = Path(__file__).resolve().parent / "fixtures"
HEADER = FIXTURES / "repl_nested_receivers.hpp"
NS = "nested_receivers"

# selector, receiver member, member value, call result
CASES = (
    (f"{NS}::Outer::Nested::f(int) const", "v", 3, 10),
    (f"{NS}::Outer::Sibling::g(int) const", "w", 5, 10),
    (f"{NS}::Outer2::Mid::Deep::f(int) const", "v", 1, 2),
    (f"{NS}::Plain::get() const", "a", 1, 3),
)


def callback(member):
    return ("class Interceptor { public:\n"
            f'  void pre(const Call &call) {{ call.field("m", call.self()->{member}); }}\n'
            '  void post(const Call &call) { call.field("r", call.result<int>()); }\n};\n')


def field(transcript, name):
    found = re.search(r'name: "' + name + r'"\s*value\s*\{\s*type: \w+\s+(?:un)?signed_value: (-?\d+)',
                      transcript)
    return int(found[1]) if found else None


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ct-nested-") as directory, ExitStack() as stack:
        root = Path(directory)
        target = root / "nested-target"
        subprocess.run([args.compiler, "-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer",
                        "-fcf-protection=none", "-fPIE", "-pie", "-I", str(FIXTURES),
                        str(FIXTURES / "repl_nested_receivers.cpp"), "-o", str(target)],
                       check=True, timeout=120)
        child = subprocess.Popen([str(target)], stdout=subprocess.PIPE, text=True)
        stack.callback(stop, child)
        Transcript(child.stdout).expect(r"^NESTED_FIXTURE pid=")
        console = subprocess.Popen(
            [args.repl, "--server", args.server, "--agent", args.agent, "--attach", str(child.pid)],
            cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True)
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={child.pid}$", timeout=60)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        first = len(output.seen)
        send(f'%trace preview --include "{NS}::*"')
        output.expect(r"^dry-run matched=")
        rows = [line for line in output.seen[first:] if line.startswith(("DWARF_SUPPORTED ", "UNSUPPORTED "))]
        for selector, *_ in CASES:
            assert any(row.startswith("DWARF_SUPPORTED ") and f'"{selector}"' in row for row in rows), \
                (selector, rows)
        assert not any("nested debug receiver" in row for row in rows), rows

        # Real enclosing classes, supplied by the header, must be reused and not redeclared;
        # without it every nested receiver is rebuilt from DWARF alone.
        for header in (False, True):
            for index, (selector, member, value, result) in enumerate(CASES):
                observer = root / f"observer{index}{'h' if header else ''}.cpp"
                observer.write_text(callback(member))
                option = f' --include "{HEADER}" --prepared-mode off' if header else ""
                send(f'%probe inject "{selector}" --callback "{observer}"{option}')
                probe = output.expect(r"^installed probe=(\d+)|^error: .*", timeout=90)
                assert probe[0].startswith("installed"), ("".join(output.seen[-5:]), selector, header)
                send(f"%collect --probe {probe[1]} --count 2 --timeout 20 --as nested_events")
                output.expect(r"^received=2 ")
                mark = len(output.seen)
                send('for (const auto& event : tracer.batch("nested_events")) tracer.print(event);')
                send('std::cout << "nested-events-printed" << std::endl;')
                output.expect(r"^nested-events-printed$")
                text = "".join(output.seen[mark:])
                assert field(text, "m") == value, (selector, header, text[-1500:])
                assert field(text, "r") == result, (selector, header, text[-1500:])
                send(f"%probe remove {probe[1]}")
                output.expect(rf"^removed probe={probe[1]} restored=true$")
        send("%detach")
        output.expect(r"^detached session=")
        send("%quit")
        assert console.wait(timeout=30) == 0, "".join(output.seen)
        assert child.poll() is None
    print("Nested receivers observed from DWARF alone and with real headers.")


if __name__ == "__main__":
    main()
