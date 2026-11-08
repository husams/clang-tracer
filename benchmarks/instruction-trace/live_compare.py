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
    if re.search(r"\bGLIBC_PRIVATE\b", versions):
        raise ValueError(f"{plugin} requires private glibc symbols")
    stack = next(line.split() for line in segments.splitlines() if "GNU_STACK" in line)
    assert "RW" in stack and "GNU_RELRO" in segments
    glibc = max(re.findall(r"\bGLIBC_([0-9.]+)", versions),
                key=lambda version: tuple(map(int, version.split("."))))
    return {"bytes": plugin.stat().st_size, "needed": needed, "max_glibc": glibc,
            "private_glibc_imports": [],
            "exports": sorted(names), "relro": "full", "executable_stack": False}


def verify(repl, server, agent, fixture, plugin, command, target_library_path):
    names = symbols(fixture)
    workloads = {"w": (3, 19, 0, 0), "v": (2, 16, 0, 0),
                 "s": (-3, 19, 1, 0), "f": (0, 39, 0, 1)}
    expected = {}
    for letter, (rounds, _, signals, _) in workloads.items():
        labels = expected_pcs(abs(rounds), signals) if rounds else ["fault_load", "fault_return"]
        expected[letter] = [names[f"pilot_pc_{label}"] - names["pilot_work"]
                            for label in labels]
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
        observers = [root / f"observer-{version}.cpp" for version in (1, 2)]
        for version, observer in enumerate(observers, start=1):
            observer.write_text(f"""
struct Interceptor {{
  void pre(const Call &call) {{ call.field("live_probe", {version}); }}
  void post(const Call &call) {{ call.field("live_probe", {version}); }}
}};
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
        assert tid != child.pid
        assert end - begin == names["pilot_live_work_end"] - names["pilot_work"]
        maps = Path(f"/proc/{child.pid}/maps")
        task = Path(f"/proc/{child.pid}/task/{tid}")
        memory = stack.enter_context(open(f"/proc/{child.pid}/mem", "rb", buffering=0))

        def mapped():
            return str(plugin) in maps.read_text()

        def code():
            return os.pread(memory.fileno(), 16, entry)

        original = code()
        assert not mapped() and task.exists()
        initial_libdl = any(re.search(r"/libdl(?:-\S+|\.so\.2)(?:\s|$)", line)
                            for line in maps.read_text().splitlines())
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

        def worker(letter):
            _, value, signals, faults = workloads[letter]
            target(letter, rf"^worker returned {value} signals={signals} faults={faults}$")

        def native_call(letter, probe, version=1):
            nonlocal cursor
            rounds, value, _, _ = workloads[letter]
            worker(letter)
            send(f"%events --probe {probe} --after {cursor} --count 2 --timeout 8")
            pre = output.expect(
                rf"^cursor=(\d+) .*tid={tid} probe={probe} phase=PRE call=(\d+) "
                rf".*rounds={rounds}.*live_probe={version}(?: |$)"
            )
            post = output.expect(
                rf"^cursor=(\d+) .*tid={tid} probe={probe} phase=POST call={pre[2]} "
                rf".*live_probe={version} .*result={value}(?: |$)"
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

        def remove(probe):
            send(f"%probe remove {probe}")
            output.expect(rf"^removed probe={probe} restored=true$")
            assert code() == original

        def check_capture(calls, cap):
            wanted = [pc for letter in calls for pc in expected[letter]]
            actual = json.loads(capture.read_text())
            assert actual == {"truncated": int(len(wanted) > cap), "offsets": wanted[:cap]}, (
                calls, actual, wanted)
            capture.unlink()
            return len(actual["offsets"])

        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        output.expect(rf'^target JIT runtime .*glibc_version: "{re.escape(glibc)}"')
        selector = "pilot_observed(int)"
        native_probe = install(selector, observers[0])
        patch = code()
        assert patch != original
        native_call("w", native_probe)
        control_probe = install("pilot_control(int, int, unsigned long, unsigned long, int)", loader)
        for arm, calls, cap in (("b", "wv", 256), ("c", "w", 7)):
            assert not mapped()
            control(arm)
            assert mapped() and code() == patch and task.exists()
            for letter in calls:
                native_call(letter, native_probe)
            control("e")
            assert not mapped() and code() == patch and task.exists()
            check_capture(calls, cap)
            native_call("w", native_probe)

        remove(native_probe)
        worker("s")
        worker("f")
        control("b")
        assert mapped() and task.exists()
        worker("w")  # Warm the unpatched entry before modifying its instructions.
        first = install(selector, observers[0])
        first_patch = code()
        assert first_patch != original and mapped()
        native_call("w", first)
        native_call("s", first)

        send(f'%probe reload {first} --callback "{observers[1]}"')
        second = output.expect(rf"^reloaded probe=(\d+) replaces={first} ")[1]
        assert first != second and code() not in (original, first_patch) and mapped()
        native_call("f", second, version=2)
        native_call("v", second, version=2)
        remove(second)
        assert mapped() and task.exists()
        worker("v")
        send(f"%events --after {cursor} --count 1 --timeout 1")
        output.expect(r"^received=0 .*timed_out=true")

        control("e")
        assert not mapped() and code() == original and task.exists()
        mutation_records = check_capture("wwsfvv", 256)

        send("%detach")
        output.expect(r"^detached session=\S+$")
        assert code() == original and not mapped()
        worker("v")
        worker("s")
        worker("f")
        send("%quit")
        assert console.wait(timeout=15) == 0, "".join(output.seen)
        child.stdin.write("q")
        child.stdin.flush()
        assert child.wait(timeout=5) == 0, "".join(target_out.seen)
        return {"glibc": glibc, "pid": child.pid, "worker_tid": tid,
                "libdl_mapped_before_attach": initial_libdl,
                "full_records": len(expected["w"]) + len(expected["v"]), "capped_records": 7,
                "mutation_records": mutation_records, "signals": 1, "recovered_faults": 1,
                "native_pairs": 10, "probe_changes": ["install", "reload", "remove"],
                "unloaded": True, "restored": True}


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
