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
import time

from compare import expected_pcs, symbols

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))
from repl_browser_integration import Transcript, stop


def inspect_plugin(plugin):
    dynamic = subprocess.check_output(["readelf", "-Wd", plugin], text=True)
    segments = subprocess.check_output(["readelf", "-Wl", plugin], text=True)
    versions = subprocess.check_output(["readelf", "-WV", plugin], text=True)
    exports = subprocess.check_output(["nm", "-D", "--defined-only", plugin], text=True)
    needed = sorted(re.findall(r"Shared library: \[([^\]]+)\]", dynamic))
    allowed = {"ld-linux-x86-64.so.2", "libc.so.6", "libdl.so.2", "libgcc_s.so.1", "libm.so.6",
               "libpthread.so.0", "libresolv.so.2"}
    names = {line.split()[-1] for line in exports.splitlines() if line}
    pilot = {"pilot_gum_init", "pilot_gum_shutdown", "pilot_gum_run",
             "pilot_gum_live_start", "pilot_gum_live_stop"}
    supported = {"tracer_gum_instruction_start_v1", "tracer_gum_instruction_stop_v1"}
    extended = {"tracer_gum_instruction_start_v2", "tracer_gum_instruction_stop_v2"}
    assert names in (pilot, pilot | supported, pilot | supported | extended), names
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


def verify(repl, server, agent, fixture, plugin, command, target_library_path,
           native_repl=False, callback_library=None):
    names = symbols(fixture)
    workloads = {"w": (3, 19, 0, 0), "v": (2, 16, 0, 0),
                 "s": (-3, 19, 1, 0), "f": (0, 39, 0, 1), "m": (-4, 27, 0, 0)}
    expected = {}
    for letter, (rounds, _, signals, _) in workloads.items():
        if letter == "m":
            continue
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
        if callback_library:
            match = target_out.expect(r"^memory begin=(0x\w+) end=(0x\w+) data=(0x\w+)$")
            mem_begin, mem_end, data = (int(value, 16) for value in match.groups())
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
        console_environment = None
        if native_repl:
            console_environment = dict(os.environ)
            if plugin == agent.parent / "gum-backend.so":
                console_environment.pop("CLANG_TRACER_GUM_PLUGIN", None)
            else:
                console_environment["CLANG_TRACER_GUM_PLUGIN"] = str(plugin)
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(child.pid)],
            cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, env=console_environment,
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
            worker(letter)
            verify_native_call(letter, probe, version)

        def verify_native_call(letter, probe, version=1):
            nonlocal cursor
            rounds, value, _, _ = workloads[letter]
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

        capture_reply = None
        capture_reason = None

        def mapping_token():
            send("%memory maps")
            token = output.expect(r"^memory mapping=(\S+)$")[1]
            output.expect(r"^memory mappings=\d+$")
            return token

        def start_command(token, worker_tid=tid, size=end - begin, cap=256, seconds=60):
            return (f"%instructions start --tid {worker_tid} --address {begin:#x} "
                    f"--size {size} --mapping {token} --count {cap} --timeout {seconds}")

        def control(letter, seconds=60):
            nonlocal cursor, capture_reply, capture_reason
            if native_repl:
                if letter == "e":
                    send("%instructions stop")
                    offsets = []
                    while True:
                        row = output.expect(
                            rf"^instruction pc=(0x\w+)$|^instructions stopped tid={tid} "
                            r"count=(\d+) reason=(\w+) truncated=(true|false)$")
                        if row[1]:
                            offsets.append(int(row[1], 16) - begin)
                            continue
                        assert int(row[2]) == len(offsets)
                        capture_reply = {"truncated": int(row[4] == "true"), "offsets": offsets}
                        capture_reason = row[3]
                        return
                send(start_command(mapping_token(), cap=7 if letter == "c" else 256,
                                   seconds=seconds))
                output.expect(rf"^instructions started tid={tid} address={begin:#x} ")
                return
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

        def verify_extensions(probe):
            labels = ["read", "write", "rmw", "read_after_write", "add", "return"]
            pcs = [mem_begin + names[f"pilot_pc_memory_{label}"] - names["pilot_memory"]
                   for label in labels]
            assert mem_end - mem_begin == names["pilot_memory_end"] - names["pilot_memory"]

            def extended_start(options):
                return (f"%instructions start --tid {tid} --address {mem_begin:#x} "
                        f"--size {mem_end - mem_begin} --mapping {mapping_token()} {options}")

            def extended_capture(options, count, reason="STOPPED"):
                send(extended_start(options))
                output.expect(rf"^instructions started tid={tid} address={mem_begin:#x} ")
                assert mapped() and code() == patch
                native_call("m", probe)
                send("%instructions stop")
                rows = [dict(item.split("=", 1) for item in output.expect(
                            r"^instruction (.+)$")[1].split()) for _ in range(count)]
                output.expect(rf"^instructions stopped tid={tid} count={count} reason={reason} ")
                assert not mapped() and str(callback_library) not in maps.read_text() and code() == patch
                return rows

            send(extended_start(f'--callback-library "{plugin}"'))
            output.expect(r"RPC 9: target could not load tracer_instruction_callback_v1")
            assert not mapped() and code() == patch
            send(extended_start("--memory-address 1 --memory-size 8"))
            output.expect(r"RPC 9: monitored data must fit one readable mapping")

            options = f'--registers --callback-library "{callback_library}"'
            rows = extended_capture(options, len(pcs))
            assert [int(row["pc"], 16) for row in rows] == pcs
            for index, row in enumerate(rows, start=1):
                assert len(row) == 18 and int(row["rdi"], 16) == data and int(row["rsi"], 16) == 7
                assert int(row["callback"], 16) == (int(row["pc"], 16) ^ data ^ 7) + index
            assert [int(row["rax"], 16) for row in rows[1:]] == [10, 10, 10, 10, 17]
            capped = extended_capture(options + " --count 2", 2, "LIMIT")
            assert [int(row["callback"], 16) for row in capped] == [
                (pc ^ data ^ 7) + index for index, pc in enumerate(pcs[:2], start=1)]

            access = extended_capture(f"--memory-address {data:#x} --memory-size 16", 4)
            assert [(int(row["pc"], 16), int(row["memory"], 16), int(row["size"]), row["access"])
                    for row in access] == list(zip(pcs, [data, data + 8, data, data + 8],
                                                   [8] * 4, ["read", "write", "read-write", "read"]))
            filtered = extended_capture(f"--memory-address {data + 9:#x} --memory-size 1", 2)
            assert [int(row["pc"], 16) for row in filtered] == [pcs[1], pcs[3]]

            def watch(access_mode, hit_pcs):
                send(f"%watchpoint --tid {tid} --address {data + 8:#x} --size 8 "
                     f"--mapping {mapping_token()} --access {access_mode} "
                     f"--count {len(hit_pcs) or 1} --timeout 2")
                output.expect(rf"^local capture ready kind=watchpoint pid={child.pid} threads=1$")
                if hit_pcs:
                    worker("m")
                for ip in hit_pcs:
                    output.expect(rf"^kind=watchpoint pid={child.pid} tid={tid} time_ns=\d+ "
                                  rf"ip={ip:#x} watched={data + 8:#x} size=8 access={access_mode}$")
                reason = "limit" if hit_pcs else "timeout"
                output.expect(rf"^local capture complete kind=watchpoint events={len(hit_pcs)} "
                              rf"lost=0 .* throttles=0 discarded=0 stopped={reason}$")
                if hit_pcs:
                    verify_native_call("m", probe)

            watch("write", [pcs[2]])
            watch("read-write", [pcs[2], pcs[4]])
            watch("write", [])
            native_call("m", probe)
            return {"register_snapshots": len(rows), "callbacks": len(rows) + len(capped),
                    "memory_accesses": len(access) + len(filtered), "hardware_watchpoint_hits": 3,
                    "callback_errno_preserved": True, "watchpoint_idle_timeout": True}

        def check_capture(calls, cap, reason=None):
            wanted = [pc for letter in calls for pc in expected[letter]]
            if native_repl:
                actual = capture_reply
                assert capture_reason == (reason or ("LIMIT" if len(wanted) > cap else "STOPPED"))
            else:
                actual = json.loads(capture.read_text())
                capture.unlink()
            assert actual == {"truncated": int(len(wanted) > cap), "offsets": wanted[:cap]}, (
                calls, actual, wanted)
            return len(actual["offsets"])

        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        output.expect(rf'^target JIT runtime .*glibc_version: "{re.escape(glibc)}"')
        if native_repl:
            send("%instructions stop")
            output.expect(r"RPC 9: session has no instruction capture")
            send(start_command("stale"))
            output.expect(r"RPC 10: instruction mapping snapshot is stale")
            token = mapping_token()
            send(start_command(token, size=65536))
            output.expect(r"RPC 9: instruction range must fit one readable file-backed executable mapping")
            send(start_command(token, worker_tid=2147483647))
            output.expect(r"RPC 9: instruction thread is unavailable in the selected target")
            assert not mapped()
        selector = "pilot_observed(int)"
        native_probe = install(selector, observers[0])
        patch = code()
        assert patch != original
        native_call("w", native_probe)
        extensions = verify_extensions(native_probe) if callback_library else None
        if not native_repl:
            control_probe = install("pilot_control(int, int, unsigned long, unsigned long, int)", loader)
        for arm, calls, cap in (("b", "wv", 256), ("c", "w", 7)):
            assert not mapped()
            control(arm)
            assert mapped() and code() == patch and task.exists()
            if native_repl and arm == "b":
                send(start_command(mapping_token()))
                output.expect(r"RPC 9: stop the existing instruction capture")
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

        deadline_records = None
        if native_repl:
            control("b", seconds=1)
            worker("w")
            time.sleep(1.25)
            worker("v")
            control("e")
            deadline_records = check_capture("w", 256, reason="TIMEOUT")
            assert not mapped()
            control("e")
            assert check_capture("w", 256, reason="TIMEOUT") == deadline_records
            last = install(selector, observers[0])
            if callback_library:
                send(start_command(mapping_token()) + f' --callback-library "{callback_library}"')
                output.expect(rf"^instructions started tid={tid} address={begin:#x} ")
                assert str(callback_library) in maps.read_text()
            else:
                control("b")
            native_call("v", last)
            assert mapped() and code() != original

        send("%detach")
        output.expect(r"^detached session=\S+$")
        assert code() == original and not mapped()
        if callback_library:
            assert str(callback_library) not in maps.read_text()
        worker("v")
        worker("s")
        worker("f")
        send("%quit")
        assert console.wait(timeout=15) == (1 if native_repl else 0), "".join(output.seen)
        child.stdin.write("q")
        child.stdin.flush()
        assert child.wait(timeout=5) == 0, "".join(target_out.seen)
        return {"glibc": glibc, "pid": child.pid, "worker_tid": tid,
                "libdl_mapped_before_attach": initial_libdl,
                "full_records": len(expected["w"]) + len(expected["v"]), "capped_records": 7,
                "mutation_records": mutation_records, "signals": 1, "recovered_faults": 1,
                "native_pairs": (11 + (7 if extensions else 0)) if native_repl else 10,
                "probe_changes": ["install", "reload", "remove"],
                "deadline_records": deadline_records,
                "detached_while_active": native_repl,
                "rejection_checks": (5 + (2 if extensions else 0)) if native_repl else 0,
                "extensions": extensions,
                "unloaded": True, "restored": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("repl", "server", "agent", "fixture", "plugin"):
        parser.add_argument(arg, type=Path)
    parser.add_argument("--loader", type=Path)
    parser.add_argument("--library-path")
    parser.add_argument("--target-library-path",
                        help="set LD_LIBRARY_PATH only for the target fixture")
    parser.add_argument("--native-repl", action="store_true",
                        help="use the supported REPL commands instead of the pilot's control observer")
    parser.add_argument("--callback-library", type=Path,
                        help="verify snapshots, callbacks, memory-access monitoring and watchpoints")
    args = parser.parse_args()
    if bool(args.loader) != bool(args.library_path):
        parser.error("--loader and --library-path must be supplied together")
    if args.callback_library and not args.native_repl:
        parser.error("--callback-library requires --native-repl")
    fixture = args.fixture.resolve()
    command = [fixture]
    if args.loader:
        command = [args.loader.resolve(), "--library-path", args.library_path, fixture]
    plugin = args.plugin.resolve()
    packaging = inspect_plugin(plugin)
    result = verify(args.repl.resolve(), args.server.resolve(), args.agent.resolve(),
                    fixture, plugin, command, args.target_library_path, args.native_repl,
                    args.callback_library.resolve() if args.callback_library else None)
    print(json.dumps({"capture": result, "packaging": packaging}))


if __name__ == "__main__":
    main()
