"""Observe the fixed arguments of variadic callables without changing the original call.

The fixture (tests/fixtures/repl_variadic.cpp) returns values that depend on the whole variadic
tail: integer registers, stack arguments, vector registers with the %al count, and a printf-style
call. One "g" byte runs one pass that checks every result, so a lost or changed tail shows up as
`bad=` in the pass line of the target itself.
"""

from contextlib import ExitStack
import json
from pathlib import Path
import queue
import re
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop

PASSES = 3
PRE = "struct Interceptor { void pre(const Call &call) { (void)call.argument<%s>(0); } };\n"
SELECTORS = {
    "vlog": ("variadic::vlog(const char*, ...)", "const char *"),
    "log": ("variadic::Logger::log(const char*, ...)", "const char *"),
    "c_vlog": ("c_vlog(const char*, ...)", "const char *"),
    "sum_ints": ("variadic::sum_ints(int, ...)", "int"),
    "spill": ("variadic::spill(const char*, ...)", "const char *"),
    "dsum": ("variadic::dsum(int, ...)", "int"),
    "mixed": ("variadic::mixed(const char*, ...)", "const char *"),
    "fmtlog": ("variadic::fmtlog(const char*, ...)", "const char *"),
}


class Output(Transcript):
    def expect(self, pattern, timeout=90):
        deadline = time.monotonic() + timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                raise AssertionError((pattern, "".join(self.seen)))
            self.seen.append(line)
            if match := re.search(pattern, line):
                return match


def send(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


def main():
    repl, server, agent, target_path = sys.argv[1:5]
    with ExitStack() as stack:
        root = Path(stack.enter_context(tempfile.TemporaryDirectory()))
        target = subprocess.Popen([target_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        stack.callback(stop, target)
        app = Output(target.stdout)
        app.expect(r"^READY$")
        passes = 0

        def run_passes(count=PASSES):
            nonlocal passes
            for _ in range(count):
                send(target, "g")
                line = app.expect(rf"^PASS {passes} bad=(\d+)")
                assert line.group(1) == "0", "".join(app.seen)
                passes += 1

        run_passes(1)  # the fixture's own results, before any probe
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(target.pid),
             "--timeout", "120"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        stack.callback(stop, console)
        output = Output(console.stdout)
        output.expect(rf"^attached session=\S+ pid={target.pid}$")

        send(console, '%trace preview --include "variadic::*" --include "c_vlog*"')
        rows = []
        while True:
            line = output.expect(r"^(\w+) address=\S+ \"([^\"]+)\".*|^dry-run (.*)")
            if line.group(3):
                break
            rows.append((line.group(1), line.group(2), line.group(0)))
        assert all(status == "DWARF_SUPPORTED" for status, _, _ in rows), rows
        variadic = [text for _, selector, text in rows if "..." in selector]
        assert len(variadic) == len(SELECTORS), rows
        assert all("variadic, fixed arguments only" in text for text in variadic), rows
        assert not any("fixed arguments only" in text for _, selector, text in rows if "..." not in selector)

        probes = {}
        for name, (selector, argument) in SELECTORS.items():
            callback = root / f"{name}.cpp"
            callback.write_text(PRE % argument)
            send(console, f'%probe inject "{selector}" --callback "{callback}"')
            probes[name] = output.expect(r"^installed probe=(\d+) ")[1]

        # The target keeps computing exactly what it did without probes.
        run_passes()
        recorded = {}
        for name in ("sum_ints", "dsum"):
            path = root / f"{name}.json"
            send(console, f"%collect --probe {probes[name]} --phase PRE --count {PASSES} "
                          f"--timeout 10 --as {name}")
            output.expect(rf"received={PASSES} ")
            send(console, f"%events export {name} --format json --output {path}")
            deadline = time.monotonic() + 15
            while True:
                try:
                    events = json.loads(path.read_text())
                    break
                except (OSError, ValueError):
                    assert time.monotonic() < deadline, name
                    time.sleep(0.1)
            recorded[name] = [int(e["namedArguments"][0]["value"]["signedValue"]) for e in events]
        # The fixed argument is the only one recorded; the tail is forwarded, not captured.
        assert set(recorded["sum_ints"]) == {3, 12}, recorded
        assert set(recorded["dsum"]) == {10}, recorded
        for name in ("vlog", "log", "c_vlog", "spill", "mixed", "fmtlog"):
            send(console, f"%collect --probe {probes[name]} --phase PRE --count {PASSES} --timeout 10")
            output.expect(rf"received={PASSES} ")

        # A post() would run after an original whose return nothing observes: refused, precisely.
        callback = root / "post.cpp"
        callback.write_text("struct Interceptor { void pre(const Call &) {} "
                            "void post(const Call &call) { (void)call.result<int>(); } };\n")
        send(console, f'%probe inject "{SELECTORS["vlog"][0]}" --callback "{callback}"')
        error = output.expect(r"^error: (.*)")[1]
        assert "variadic callable: post() is unsupported" in error, error
        callback.write_text("struct Interceptor { void pre(const Call &) {} };\n")
        send(console, f'%probe inject "{SELECTORS["sum_ints"][0]}" --callback "{callback}" '
                      "--stack-depth 3")
        error = output.expect(r"^error: (.*)")[1]
        assert "variadic callable: stack capture is unsupported" in error, error

        run_passes()
        for name, probe in probes.items():
            send(console, f"%probe remove {probe}")
            output.expect(r"^removed probe=\d+ restored=true$")
        # The default observer declares an empty post(): it observes nothing, so it installs.
        send(console, f'%probe inject "{SELECTORS["sum_ints"][0]}"')
        default = output.expect(r"^installed probe=(\d+) ")[1]
        run_passes()
        send(console, f"%probe remove {default}")
        output.expect(r"^removed probe=\d+ restored=true$")
        send(console, "%detach")
        output.expect(r"^detached session=")
        run_passes(1)


if __name__ == "__main__":
    main()
