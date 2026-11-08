"""Exercise the remaining local C++ features through an owned native REPL."""

import argparse
import errno
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

from repl_browser_integration import Transcript, start_time, stop
from repl_followups_integration import wait_file


def verify(args):
    with tempfile.TemporaryDirectory(prefix="ct-more-") as name:
        root = Path(name)
        control, stdout = root / "control", root / "stdout"
        callback = root / "observer.cpp"
        callback.write_text("""#ifndef TRACER_BACKPORT_EXTRA_ARG
#error supplemental compiler argument was not forwarded
#endif
struct Interceptor {
  void pre(const Call &call) {
    if (call.arg<int>(0) == 9) {
      try { (void)call.call_original<int>(); }
      catch (...) { call.field("original_threw", 1); }
      return;
    }
    const int value = call.call_original<int>();
    call.field("original", value);
    call.field("cached", call.call_original<int>());
  }
  void post(const Call &call) { call.field("returned", call.result<int>()); }
};
""")
        env = dict(os.environ)
        if args.gum:
            env["CLANG_TRACER_GUM_PLUGIN"] = str(args.gum)
        invocation = [args.repl, "--server", args.server, "--agent", args.agent]
        console = subprocess.Popen(
            invocation, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, env=env,
        )
        output = Transcript(console.stdout)
        try:
            def send(line):
                console.stdin.write(line + "\n")
                console.stdin.flush()

            def trigger(letter, path=control):
                draft = path.with_suffix(".tmp")
                draft.write_text(letter)
                os.replace(draft, path)

            def mapping():
                send("%memory maps")
                token = output.expect(r"^memory mapping=(\S+)$")[1]
                output.expect(r"^memory mappings=\d+$")
                return token

            def inject():
                send(f'%probe inject "remaining_observed(int)" --callback "{callback}" '
                     "--allow-call-original --trace-exceptions --stack-depth full "
                     "--extra-arg -DTRACER_BACKPORT_EXTRA_ARG=1")
                return int(output.expect(r"^installed probe=(\d+)")[1])

            send(f'%launch "{args.target}" --arg parent --arg "{control}" --stdout "{stdout}" '
                 f'--preload "{args.preload}" --supervise --gate-children --timeout 15')
            glibc = output.expect(r'^target JIT runtime .*glibc_version: "([0-9.]+)"')[1]
            if args.glibc:
                assert glibc == args.glibc, glibc
            root_pid = int(output.expect(r"^launched pid=(\d+) session=\S+ paused=true$")[1])
            assert not stdout.read_text()
            probe = inject()
            send("%resume")
            output.expect(rf"^resumed pid={root_pid}$")
            ready = wait_file(stdout, rf"^ready pid={root_pid} tid=(\d+) data=(0x\w+) "
                                      r"entry=(0x\w+) direct=(0x\w+) indirect=(0x\w+)$")
            tid, data, entry, direct, indirect = ready.groups()
            trigger("v")
            wait_file(stdout, rf"^value=42 calls=1 errno={errno.ERANGE}$")
            trigger("e")
            wait_file(stdout, rf"^caught=91 calls=1 errno={errno.ERANGE}$")
            send(f"%collect --probe {probe} --count 4 --timeout 6 --as outcomes")
            cursor = int(output.expect(r"^received=4 cursor=(\d+) complete=true .*collection=outcomes")[1])
            send("%events list outcomes")
            output.expect(r"phase=PRE .*original=42.*cached=42")
            output.expect(r"phase=POST .*returned=42")
            output.expect(r"phase=PRE .*original_threw=1")
            output.expect(r"phase=EXCEPTION ")
            send("%events calls outcomes")
            output.expect(r"PRE=present EXCEPTION=present duration_ns=\d+")
            print(f"glibc {glibc}: callable original ran once, preserved errno and propagated C++ exception", flush=True)

            token = mapping()
            send(f'%memory write {data} --mapping {token} --expect "00" --replace "58"')
            output.expect(r"expected patch bytes changed")
            send(f'%memory write {data} --mapping {token} --expect "13" --replace "58"')
            patch = int(output.expect(r"^memory patch=(\d+) address=0x\w+ size=1 restored=false$")[1])
            trigger("w")
            wait_file(stdout, r"^writable=88$")
            send(f"%memory restore {patch}")
            output.expect(rf"^memory patch={patch} .*restored=true$")
            trigger("w")
            wait_file(stdout, r"^writable=19$")
            send("%memory allocate 8192")
            allocation, address = output.expect(
                r"^memory allocation=(\d+) address=(0x\w+) size=8192 freed=false$"
            ).groups()
            send(f'%memory read {address} --mapping {token} --type u8')
            output.expect(r"mapping snapshot is stale")
            token = mapping()
            send(f'%memory write {address} --mapping {token} --expect "00" --replace "2a"')
            patch = output.expect(r"^memory patch=(\d+) .*restored=false$")[1]
            send(f"%memory free {allocation}")
            output.expect(r"restore patches in the allocation")
            send(f"%memory restore {patch}")
            output.expect(rf"^memory patch={patch} .*restored=true$")
            send(f"%memory free {allocation}")
            output.expect(rf"^memory allocation={allocation} .*freed=true$")
            token = mapping()
            send(f"%memory read {address} --mapping {token} --type u8")
            output.expect(r"not mapped|not contained|unmapped|readable mapping|outside a mapping|mapping gap")
            print(f"glibc {glibc}: verified writable patch, restore, allocation ownership and free", flush=True)

            send(f'%library load "{callback}"')
            output.expect(r"must be a readable Linux x86-64 ELF shared object")
            for _ in range(2):
                send(f'%library load "{args.library}"')
                output.expect(r"^library loaded .*lifetime=target-process$")
            wait_file(stdout, r"^injected-library-constructor$")
            assert stdout.read_text().count("injected-library-constructor") == 1
            print(f"glibc {glibc}: loaded target library and ran its constructor once", flush=True)

            recordings = []
            if args.gum:
                symbols = subprocess.check_output(["nm", "-S", args.target], text=True)
                size = int(re.search(r"^\w+ (\w+) T remaining_discovery_entry$", symbols, re.M)[1], 16)
                for serial, mode in enumerate(("registers", "discover"), 1):
                    send(f"%instructions start --tid {tid} --address {entry} --size {size} "
                         f"--mapping {mapping()} --{mode} --timeout 10")
                    output.expect(rf"^instructions started tid={tid} ")
                    trigger("i")
                    wait_file(stdout, rf"^capture result=304 observed=42 serial={serial}$")
                    file = root / (mode + ".trace")
                    send(f'%instructions stop --save "{file}"')
                    first = len(output.seen)
                    match = output.expect(r"^instructions stopped .*count=(\d+) reason=STOPPED")
                    assert int(match[1]) > 0
                    lines = "".join(output.seen[first:])
                    if mode == "discover":
                        assert f"discovered target={direct} calls=1\n" in lines, lines
                        assert f"discovered target={indirect} calls=1\n" in lines, lines
                    else:
                        assert " rax=0x" in lines, lines
                    recordings.append((file, lines))
                    send(f"%events --probe {probe} --after {cursor} --count 2 --timeout 6")
                    output.expect(rf"probe={probe} phase=PRE .*original=42")
                    output.expect(rf"probe={probe} phase=POST .*returned=42")
                    cursor = int(output.expect(r"^received=2 cursor=(\d+) complete=true")[1])
                print(f"glibc {glibc}: captured registers and discovered direct and indirect C++ callees", flush=True)

            if args.perf:
                send("%faults --count 1 --timeout 6")
                output.expect(r"^local capture ready kind=faults ")
                trigger("n")
                wait_file(stdout, r"^fault-recovered$")
                output.expect(r"^kind=native-fault .*signal=11 code=2$")
                output.expect(r"^local capture complete kind=faults events=1 lost=0 ")
                send("%sample allocations --count 8192 --timeout 2")
                output.expect(r"^local capture ready kind=allocations ")
                trigger("a")
                wait_file(stdout, r"^allocations-done$")
                first = len(output.seen)
                output.expect(r"^local capture complete kind=allocations .*lost=0 ")
                lines = "".join(output.seen[first:])
                for allocator, size in (("malloc", 12347), ("calloc", 1141), ("realloc", 23459)):
                    assert re.search(rf"allocator={allocator} .*requested={size}$", lines, re.M), lines
                    assert f"allocation-summary allocator={allocator} " in lines, lines
                print(f"glibc {glibc}: observed a recovered native fault and profiled all three glibc allocators", flush=True)

            trigger("f")
            child = int(wait_file(stdout, r"^child-started=(\d+)$")[1])
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                send("%children")
                send("%status")
                boundary = len(output.seen)
                output.expect(r"^owned_target pid=")
                if f"gated child pid={child} owner={root_pid}\n" in output.seen[boundary:]:
                    break
                time.sleep(0.025)
            else:
                raise AssertionError("child checkpoint unavailable")
            assert "child-ready" not in stdout.read_text()
            send(f"%attach {child} --start-time {start_time(child)}")
            output.expect(rf"^attached session=\S+ pid={child}$")
            child_probe = inject()
            send(f"%resume {child}")
            output.expect(rf"^resumed child pid={child}$")
            wait_file(stdout, rf"^child-ready pid={child} ")
            trigger("v", control.with_name("control.child"))
            send(f"%events --probe {child_probe} --count 2 --timeout 6")
            output.expect(rf"probe={child_probe} phase=PRE .*original=42")
            output.expect(r"^received=2 .*complete=true")
            send(f"%terminate {root_pid}")
            output.expect(rf"^terminated pid={root_pid}$")
            send("%quit")
            # The stream deliberately exercised five rejected operations. A
            # noninteractive REPL returns 1 when any submitted command fails.
            assert console.wait(timeout=15) == 1, "".join(output.seen)
            while (line := output.lines.get(timeout=2)) is not None:
                output.seen.append(line)
            assert sum(line.startswith("error:") for line in output.seen) == 5, "".join(output.seen)
            assert not any("cleanup requires recovery" in line for line in output.seen)
            deadline = time.monotonic() + 5
            child_stat = Path(f"/proc/{child}/stat")
            while time.monotonic() < deadline:
                try:
                    if child_stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
                        break
                except FileNotFoundError:
                    break
                time.sleep(0.025)
            else:
                raise AssertionError("gated child survived termination of its owned group")
            print(f"glibc {glibc}: installed a probe before resuming a gated exec child and reaped its group",
                  flush=True)

            for file, expected in recordings:
                replay = subprocess.run(invocation, input=f'%instructions replay "{file}"\n%quit\n',
                                        cwd=root, capture_output=True, text=True, timeout=15, env=env)
                assert replay.returncode == 0 and expected in replay.stdout, replay.stdout + replay.stderr
            if recordings:
                invalid = root / "invalid.trace"
                invalid.write_bytes(recordings[0][0].read_bytes()[:-1])
                replay = subprocess.run(invocation, input=f'%instructions replay "{invalid}"\n%quit\n',
                                        cwd=root, capture_output=True, text=True, timeout=15, env=env)
                assert replay.returncode == 1
                assert "invalid instruction recording" in replay.stdout + replay.stderr
                print(f"glibc {glibc}: replayed both captures offline and rejected a truncated recording", flush=True)
        except Exception:
            print("".join(line for line in output.seen if not line.startswith("0x")), flush=True)
            print(stdout.read_text() if stdout.exists() else "", flush=True)
            raise
        finally:
            stop(console)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "target", "library", "preload"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--gum", type=lambda value: Path(value).resolve())
    parser.add_argument("--perf", action="store_true")
    parser.add_argument("--glibc")
    verify(parser.parse_args())
