"""Verify task-scoped kernel events, loss accounting and perf teardown."""

import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop


def perf_fds(pid):
    result = []
    for entry in Path(f"/proc/{pid}/fd").iterdir():
        try:
            if "perf_event" in os.readlink(entry):
                result.append(entry.name)
        except FileNotFoundError:
            pass
    return result


def worker_info(path):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        line = re.search(r"worker tid=(\d+) hot=(0x[0-9a-f]+)-(0x[0-9a-f]+)", path.read_text())
        if line:
            return int(line[1]), int(line[2], 16), int(line[3], 16)
        time.sleep(0.01)
    raise AssertionError("target worker did not start")


def main(repl, server, fixture, mode):
    with tempfile.TemporaryDirectory(prefix="ct-local-perf-") as directory:
        root = Path(directory)
        console = subprocess.Popen(
            [repl, "--server", server], cwd=root, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        try:
            output = Transcript(console.stdout)

            def send(command):
                console.stdin.write(command + "\n")
                console.stdin.flush()

            send(f'%launch "{fixture}" --arg {mode} --stdout target.log --stderr target.err')
            pid = int(output.expect(r"^launched pid=(\d+) session=\S+ paused=true$")[1])
            send("%resume")
            output.expect(rf"^resumed pid={pid}$")
            tid, hot_begin, hot_end = worker_info(root / "target.log")
            assert tid != pid and hot_begin < hot_end
            command = "%syscalls" if mode == "syscalls" else "%sample cpu"

            def start_capture(options):
                send(f"{command} {options}")
                result = output.expect(
                    rf"^(?:local capture ready kind={mode} pid={pid} threads=(\d+)|error: (.+))$"
                )
                if result[2]:
                    assert not perf_fds(console.pid)
                    return result[2]
                assert int(result[1]) >= 2 and perf_fds(console.pid)
                return None

            def finish_capture():
                start = len(output.seen)
                summary = output.expect(
                    rf"^local capture complete kind={mode} events=(\d+) lost=(\d+) "
                    r"loss_accounting=(?:records|counter) throttles=(\d+) discarded=(\d+) stopped=(\S+)$"
                )
                rows = [line for line in output.seen[start:] if line.startswith("kind=")]
                times = []
                for row in rows:
                    sample = re.match(r"kind=\S+ pid=(\d+) tid=(\d+) time_ns=(\d+)", row)
                    assert sample and int(sample[1]) == pid and int(sample[2]) in {pid, tid}
                    times.append(int(sample[3]))
                assert len(rows) == int(summary[1]) and times == sorted(times)
                assert not perf_fds(console.pid)
                return summary, rows

            error = start_capture("--timeout 3 --count 128")
            if error:
                assert ("permission denied" in error.lower() or "operation not permitted" in error.lower()
                        or "raw syscall tracepoints are unavailable" in error), error
                print(f"{mode} unavailable with these permissions: {error}")
                send("%quit")
                assert console.wait(timeout=15) == 1
                return 77

            if mode == "syscalls":
                burst = root / "burst"
                burst.touch()
                _, rows = finish_capture()
                burst.unlink()
                assert any(re.search(rf"^kind=syscall-enter pid={pid} tid={tid} .* nr=110$", row)
                           for row in rows), rows
                assert any(re.search(rf"^kind=syscall-exit pid={pid} tid={tid} .* nr=110 result={console.pid}$", row)
                           for row in rows), rows

                assert start_capture("--timeout 4 --count 8192") is None
                os.kill(console.pid, signal.SIGSTOP)
                try:
                    burst.touch()
                    time.sleep(0.3)
                finally:
                    os.kill(console.pid, signal.SIGCONT)
                summary, _ = finish_capture()
                burst.unlink()
                assert int(summary[2]) > 0, summary[0]
            else:
                _, rows = finish_capture()
                addresses = [int(sample[1], 16) for row in rows
                             if (sample := re.search(rf"^kind=cpu-sample pid={pid} tid={tid} .* ip=(0x[0-9a-f]+)$", row))]
                assert any(hot_begin <= ip < hot_end for ip in addresses), (hex(hot_begin), hex(hot_end), addresses)
                assert start_capture("--timeout 1 --count 8192") is None
                summary, _ = finish_capture()
                assert int(summary[1]) > 0 and summary[5] == "timeout"

            assert start_capture("--timeout 15 --count 8192") is None
            console.send_signal(signal.SIGINT)
            summary, _ = finish_capture()
            assert summary[5] == "interrupted"

            assert start_capture("--timeout 10 --count 8192") is None
            (root / "exit-target").touch()
            summary, _ = finish_capture()
            assert summary[5] == "target-exit"
            send("%quit")
            assert console.wait(timeout=15) == 0, "".join(output.seen)
            assert not Path(f"/proc/{pid}").exists()
            return 0
        finally:
            stop(console)


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
