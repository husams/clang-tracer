"""Detach while a signal handler has a saved PC inside a relocated call prefix."""

from contextlib import ExitStack
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, read_redirects, send, stop, verify_restored_redirects


def anonymous_code_pages(pid):
    page_size = os.sysconf("SC_PAGE_SIZE")
    pages = set()
    for line in Path(f"/proc/{pid}/maps").read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) != 5 or fields[1] != "r-xp" or fields[4] != "0":
            continue
        start, end = (int(value, 16) for value in fields[0].split("-"))
        pages.update(range(start, end, page_size))
    return pages


def run(repl_path, server, agent, fixture, no_values, mode):
    with tempfile.TemporaryDirectory(prefix="ct-runtime-prefix-") as directory, ExitStack() as cleanup:
        arm = mode == "arm-static"
        def start(argv):
            process = subprocess.Popen(argv, cwd=directory, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([fixture, "park"] if arm else [fixture])
        app.expect(r"^READY$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

        def read(address, count):
            return os.pread(memory.fileno(), count, address)

        repl, out = start([repl_path, "--server", server, "--agent", agent,
                           "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        baseline = anonymous_code_pages(target.pid)
        static = mode in ("static", "arm-static")
        if arm:
            definition = ("%object start --type arm_test::Record --field value --backend code-cache "
                          "--code arm_write_value --rendezvous arm_rendezvous")
        else:
            definition = "%object start --type demo::PlanRecord --field value --backend code-cache "
            definition += ("--code _ZN4demo18held_static_writerERNS_10PlanRecordEPKi" if static else
                           "--code _ZN4demo16nonleaf_dispatchERNS_10PlanRecordE --follow-calls")
        send(repl, definition + (" --no-values" if no_values else ""))
        watch = out.expect(rf"^watch=(\d+) active=yes stores={int(static)} .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, (),
                                   live_read=read, runtime_calls=not static)
        if mode == "prefix":
            [(prefix, armed)] = [(pc, patched) for pc, (_, patched, _) in redirects.items()
                                if len(patched) >= 10 and patched[-5] == 0xe8]

        original_pages = anonymous_code_pages(target.pid)
        if static:
            page_size = os.sysconf("SC_PAGE_SIZE")
            gates = {translated // page_size * page_size for _, _, translated in redirects.values()}
            added_pages = original_pages - baseline - gates
            fragments = int(re.search(r"static_fragments=(\d+)", watch.string)[1])
            assert len(added_pages) >= fragments > 0
        if arm:
            (Path(directory) / "write.now").touch()
        else:
            send(target, {"prefix": "g", "callee": "h", "static": "s"}[mode])
        app.expect(r"^PREFIX_FAULT$" if mode == "prefix" else r"^BODY_FAULT$")
        send(repl, f"%object stop {watch[1]}")
        stopped = rf"^watch={watch[1]} active=no .*restoration_pending=no "
        if mode != "prefix":
            status = out.expect(stopped)
            if static:
                counter = "static_reclaimed"
                expected = fragments
            else:
                added_pages = anonymous_code_pages(target.pid) - original_pages
                assert added_pages, "no executable callee pages were installed"
                counter = "runtime_reclaimed"
                expected = 1
            assert f"{counter}=0 " in status.string, status.string
            assert added_pages <= anonymous_code_pages(target.pid)
            verify_restored_redirects(repl, out, None, target.pid, redirects, live_read=read)
        else:
            time.sleep(0.25)
            assert read(prefix, len(armed)) == armed, "redirect restored while its prefix could still resume"

        send(target, "r")
        if arm:
            fault = app.expect(r"^FAULT count=1 pc=(0x[0-9a-f]+) value=19$")
            assert int(fault[1], 16) in redirects
            app.expect(r"^DONE value=23$")
        else:
            app.expect(r"^WROTE value=23$")
        if mode != "prefix":
            deadline = time.monotonic() + 5
            while f"{counter}={expected} " not in status.string:
                assert time.monotonic() < deadline, status.string
                time.sleep(0.01)
                send(repl, f"%object status {watch[1]}")
                status = out.expect(stopped)
            assert added_pages.isdisjoint(anonymous_code_pages(target.pid))
        else:
            out.expect(stopped)
        verify_restored_redirects(repl, out, None, target.pid, redirects, live_read=read)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        if arm:
            (Path(directory) / "stop.now").touch()
        else:
            send(target, "q")
        assert target.wait(timeout=10) == 0
        print(f"values={not no_values} fault={mode} "
              f"resumed and restored {len(redirects)} redirects")


if __name__ == "__main__":
    repl, server, agent, fixture = (Path(arg).resolve() for arg in sys.argv[1:5])
    mode = sys.argv[5].removeprefix("--") if len(sys.argv) > 5 else "prefix"
    digest = hashlib.sha256(fixture.read_bytes()).digest()
    for no_values in (False, True):
        run(repl, server, agent, fixture, no_values, mode)
    assert hashlib.sha256(fixture.read_bytes()).digest() == digest
