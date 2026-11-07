"""Attach Gum to an already-running fixture through the native tracer."""

import argparse
from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from compare import expected_pcs, symbols

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))
from repl_browser_integration import Transcript, stop


def inspect_plugin(plugin):
    dynamic = subprocess.check_output(["readelf", "-Wd", plugin], text=True)
    segments = subprocess.check_output(["readelf", "-Wl", plugin], text=True)
    versions = subprocess.check_output(["readelf", "-WV", plugin], text=True)
    exports = subprocess.check_output(["nm", "-D", "--defined-only", plugin], text=True)
    needed = sorted(re.findall(r"Shared library: \[([^\]]+)\]", dynamic))
    allowed = {"libc.so.6", "libdl.so.2", "libgcc_s.so.1", "libm.so.6",
               "libpthread.so.0", "libresolv.so.2"}
    names = {line.split()[-1] for line in exports.splitlines() if line}
    assert names == {"pilot_gum_init", "pilot_gum_shutdown", "pilot_gum_run",
                     "pilot_gum_live_start", "pilot_gum_live_stop"}, names
    assert "libc.so.6" in needed and set(needed) <= allowed, needed
    assert "(BIND_NOW)" in dynamic or re.search(r"Flags:.*\bNOW\b", dynamic)
    assert not re.search(r"\((?:TEXTREL|RPATH|RUNPATH)\)", dynamic)
    symbols = subprocess.check_output(["readelf", "-W", "--dyn-syms", plugin], text=True)
    private = sorted(re.findall(r"\bUND\s+(\S+@GLIBC_PRIVATE)\b", symbols))
    stack = next(line.split() for line in segments.splitlines() if "GNU_STACK" in line)
    assert "RW" in stack and "GNU_RELRO" in segments
    glibc = max(re.findall(r"\bGLIBC_([0-9.]+)", versions),
                key=lambda version: tuple(map(int, version.split("."))))
    return {"bytes": plugin.stat().st_size, "needed": needed, "max_glibc": glibc,
            "private_glibc_imports": private,
            "exports": sorted(names), "relro": "full", "executable_stack": False}


def verify(repl, server, agent, fixture, plugin, command, target_library_path):
    names = symbols(fixture)
    expected = {
        rounds: [names[f"pilot_pc_{label}"] - names["pilot_work"]
                 for label in expected_pcs(rounds, False)]
        for rounds in (2, 3)
    }
    with tempfile.TemporaryDirectory(prefix="ct-gum-live-") as tmp, ExitStack() as stack:
        root = Path(tmp)
        capture = root / "capture.json"
        loader = root / "loader.cpp"
        source = Path(__file__).resolve().parent
        body = (source / "live_loader.cpp").read_text()
        loader.write_text(
            f'#include {json.dumps(str(source / "pilot.h"))}\n'
            + body.replace("PILOT_PLUGIN_PATH", json.dumps(str(plugin)))
                  .replace("PILOT_RESULT_PATH", json.dumps(str(capture)))
        )
        observer = root / "observer.cpp"
        observer.write_text("""
struct Interceptor {
  void pre(const Call &call) { call.field("live_probe", 1); }
  void post(const Call &call) { call.field("live_probe", 1); }
};
""")
        environment = None
        if target_library_path is not None:
            environment = dict(os.environ, LD_LIBRARY_PATH=target_library_path)
        child = subprocess.Popen(command, cwd=root, env=environment, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        console = None

        def cleanup():
            stop(child)
            if console is not None:
                stop(console)

        stack.callback(cleanup)
        target_out = Transcript(child.stdout)
        ready = target_out.expect(
            r"^ready tid=(\d+) begin=(0x\w+) end=(0x\w+) probe=(0x\w+) glibc=([\d.]+)$"
        )
        tid, begin, end, entry, glibc = (
            int(ready[1]), int(ready[2], 16), int(ready[3], 16),
            int(ready[4], 16), ready[5]
        )
        assert tid != child.pid and end - begin == names["pilot_work_end"] - names["pilot_work"]
        maps = Path(f"/proc/{child.pid}/maps")
        task = Path(f"/proc/{child.pid}/task/{tid}")
        memory = stack.enter_context(open(f"/proc/{child.pid}/mem", "rb", buffering=0))

        def mapped():
            return str(plugin) in maps.read_text()

        def code():
            return os.pread(memory.fileno(), 16, entry)

        original = code()
        assert not mapped() and task.exists()
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(child.pid)],
            cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        output = Transcript(console.stdout)

        def send(line):
            console.stdin.write(line + "\n")
            console.stdin.flush()

        def target(letter, result):
            child.stdin.write(letter)
            child.stdin.flush()
            target_out.expect(result)

        def install(selector, callback):
            send(f'%probe inject "{selector}" --callback "{callback}"')
            return output.expect(r"^installed probe=(\d+)")[1]

        cursor = 0

        def native_call(letter, rounds):
            nonlocal cursor
            value = 3 * rounds + 10
            target(letter, rf"^worker returned {value}$")
            send(f"%events --probe {native_probe} --after {cursor} --count 2 --timeout 8")
            pre = output.expect(
                rf"^cursor=(\d+) .*tid={tid} probe={native_probe} phase=PRE call=(\d+) "
                rf".*rounds={rounds}.*live_probe=1"
            )
            post = output.expect(
                rf"^cursor=(\d+) .*tid={tid} probe={native_probe} phase=POST call={pre[2]} "
                rf".*live_probe=1.*result={value}(?: |$)"
            )
            output.expect(r"^received=2 .*complete=true")
            cursor = int(post[1])

        def control(letter):
            nonlocal cursor
            target(letter, rf"^control returned {ord(letter) + 1}$")
            send(f"%events --probe {control_probe} --after {cursor} --count 1 --phase PRE --timeout 8")
            event = output.expect(
                rf"^cursor=(\d+) .*tid={child.pid} probe={control_probe} phase=PRE .*gum_status=(-?\d+)"
            )
            output.expect(r"^received=1 .*complete=true")
            cursor = int(event[1])
            if int(event[2]):
                raise RuntimeError(f"Gum control {letter} failed ({event[2]})\n"
                                   + "".join(target_out.seen))

        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        native_probe = install("pilot_observed(int)", observer)
        patch = code()
        assert patch != original
        native_call("w", 3)
        control_probe = install("pilot_control(int, int, unsigned long, unsigned long, int)", loader)
        for arm, calls, cap in (("b", [("w", 3), ("v", 2)], 256), ("c", [("w", 3)], 7)):
            assert not mapped()
            control(arm)
            assert mapped() and code() == patch and task.exists()
            for letter, rounds in calls:
                native_call(letter, rounds)
            control("e")
            assert not mapped() and code() == patch and task.exists()
            wanted = [pc for _, rounds in calls for pc in expected[rounds]]
            actual = json.loads(capture.read_text())
            assert actual == {"truncated": int(len(wanted) > cap), "offsets": wanted[:cap]}, actual
            capture.unlink()
            native_call("w", 3)

        send("%detach")
        output.expect(r"^detached session=\S+$")
        assert code() == original and not mapped()
        target("v", r"^worker returned 16$")
        send("%quit")
        assert console.wait(timeout=15) == 0, "".join(output.seen)
        child.stdin.write("q")
        child.stdin.flush()
        assert child.wait(timeout=5) == 0, "".join(target_out.seen)
        return {"glibc": glibc, "pid": child.pid, "worker_tid": tid,
                "full_records": len(expected[3]) + len(expected[2]), "capped_records": 7,
                "native_pairs": 6, "unloaded": True, "restored": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("repl", "server", "agent", "fixture", "plugin"):
        parser.add_argument(arg, type=Path)
    parser.add_argument("--loader", type=Path)
    parser.add_argument("--library-path")
    parser.add_argument("--target-library-path",
                        help="set LD_LIBRARY_PATH only for the target fixture")
    args = parser.parse_args()
    if bool(args.loader) != bool(args.library_path):
        parser.error("--loader and --library-path must be supplied together")
    fixture = args.fixture.resolve()
    command = [fixture]
    if args.loader:
        command = [args.loader.resolve(), "--library-path", args.library_path, fixture]
    plugin = args.plugin.resolve()
    packaging = inspect_plugin(plugin)
    result = verify(args.repl.resolve(), args.server.resolve(), args.agent.resolve(),
                    fixture, plugin, command, args.target_library_path)
    print(json.dumps({"capture": result, "packaging": packaging}))


if __name__ == "__main__":
    main()
