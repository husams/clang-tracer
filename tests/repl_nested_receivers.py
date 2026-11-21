"""Nested DWARF receivers work with no headers, unrelated headers, and real type headers.

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

# method (as the preview row spells it), receiver member, member value, call result
CASES = (
    ("Outer::Nested::f(", "v", 3, 10),
    ("Outer::Sibling::g(", "w", 5, 10),
    ("Outer2::Mid::Deep::f(", "v", 1, 2),
    ("UnionScope::Nested::simple(", "n", 6, 7),
    # The enclosing type is also an argument: one declaration tree, no second definition.
    ("OwnerCase::Nested::owner(", "n", 3, 12),
    # A sibling is named by the method before it is declared.
    ("SiblingCase::A::sibling(", "n", 4, 6),
    # The enclosing type is only named by pointer and has no recoverable layout.
    ("OpaqueOwner::Nested::owner(", "n", 5, 9),
    ("UnionOwner::Nested::owner(", "n", 4, 11),
    # The method names a type nested in a sibling.
    ("DeepSibling::A::sibling(", "n", 6, 11),
    ("Plain::get(", "a", 1, 3),
)
PARTIAL = ("Partial::Nested::partial(", "n", 9, 10)


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
        selectors = {}
        for key, *_ in CASES + (PARTIAL,):
            found = [row for row in rows if row.startswith("DWARF_SUPPORTED ") and f"{NS}::{key}" in row]
            assert len(found) == 1, (key, rows)
            selectors[key] = re.search(r'"([^"]+)"', found[0])[1]
        assert not any("nested debug receiver" in row for row in rows), rows

        # An unrelated callback include must preserve the same recovered layouts
        # and public nested types. Real header declarations must still be reused.
        for mode in ("none", "stdint", "real"):
            for index, (key, member, value, result) in enumerate(CASES + (PARTIAL,)):
                selector = selectors[key]
                partial_header = HEADER.with_name("repl_nested_partial.hpp")
                if key == PARTIAL[0] and mode == "real":
                    # A real parent cannot be extended with recovered nested types,
                    # whether the header forward-declares that type or omits it.
                    observer = root / "observer-partial.cpp"
                    observer.write_text(callback(member))
                    missing_nested = root / "partial-without-nested.hpp"
                    missing_nested.write_text(f"namespace {NS} {{ struct Partial {{}}; }}\n")
                    for included in (partial_header, missing_nested):
                        send(f'%probe inject "{selector}" --callback "{observer}" --include "{included}" '
                             '--prepared-mode off')
                        failure = output.expect(r"^installed probe=(\d+)|^error: .*", timeout=90)
                        assert failure[0].startswith("error:") and "does not define its nested record" in failure[0], failure[0]
                        assert "redefinition" not in failure[0], failure[0]
                    continue
                observer = root / f"observer-{mode}-{index}.cpp"
                prefix = "#include <cstdint>\n" if mode == "stdint" else ""
                observer.write_text(prefix + callback(member))
                option = " --prepared-mode off"
                if mode == "real":
                    option += f' --include "{HEADER}"'
                send(f'%probe inject "{selector}" --callback "{observer}"{option}')
                probe = output.expect(r"^installed probe=(\d+)|^error: .*", timeout=90)
                assert probe[0].startswith("installed"), ("".join(output.seen[-5:]), selector, mode)
                send(f"%collect --probe {probe[1]} --count 2 --timeout 20 --as nested_events")
                output.expect(r"^received=2 ")
                mark = len(output.seen)
                send('for (const auto& event : tracer.batch("nested_events")) tracer.print(event);')
                send('std::cout << "nested-events-printed" << std::endl;')
                output.expect(r"^nested-events-printed$")
                text = "".join(output.seen[mark:])
                assert field(text, "m") == value, (selector, mode, text[-1500:])
                assert field(text, "r") == result, (selector, mode, text[-1500:])
                send(f"%probe remove {probe[1]}")
                output.expect(rf"^removed probe={probe[1]} restored=true$")
        send("%detach")
        output.expect(r"^detached session=")
        send("%quit")
        assert console.wait(timeout=30) == 1, "".join(output.seen)  # the expected injection error
        assert child.poll() is None
    print("Nested receivers observed with no headers, unrelated headers, and real type headers.")


if __name__ == "__main__":
    main()
