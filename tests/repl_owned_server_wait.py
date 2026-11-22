"""Verify that starting and stopping the console's own server does not stall."""

from contextlib import ExitStack
import subprocess
import sys
import time

from repl_browser_integration import Transcript, stop

# Each stall this guards against lasted about five seconds (gRPC's client
# backup poll); a healthy start or stop takes well under one.
LIMIT = 3.0


def session(repl, server, fixture, quit_commands):
    with ExitStack() as stack:
        target = subprocess.Popen([fixture], stdout=subprocess.PIPE, text=True)
        stack.callback(stop, target)
        Transcript(target.stdout).expect(r"^entries ")
        started = time.monotonic()
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(target.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(r"^server ready ")
        ready = time.monotonic() - started
        output.expect(rf"^attached session=\S+ pid={target.pid}$")
        stopping = time.monotonic()
        for command in quit_commands:
            console.stdin.write(command + "\n")
        console.stdin.flush()
        assert console.wait(timeout=15) == 0, "".join(output.seen)
        stopped = time.monotonic() - stopping
        assert target.poll() is None, "the target must survive the console"
        assert ready < LIMIT, f"the owned server took {ready:.2f} s to become ready"
        assert stopped < LIMIT, f"the console took {stopped:.2f} s to stop its server"


def main(repl, server, fixture):
    session(repl, server, fixture, ["%quit"])
    session(repl, server, fixture, ["%detach", "%server stop", "%quit"])


if __name__ == "__main__":
    main(*sys.argv[1:])
