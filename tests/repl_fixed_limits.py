"""Cross old source, argument, named-value and log-message limits end to end."""
from contextlib import ExitStack
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from repl_browser_integration import Transcript, stop


def main(repl, server, fixture):
    with tempfile.TemporaryDirectory(prefix="ct-limits-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        log = root / "agent.log"
        child = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 text=True, env=dict(os.environ, CLANG_TRACER_AGENT_LOG=str(log)))
        cleanup.callback(stop, child)
        target = Transcript(child.stdout)
        target.expect(r"^ready entry=")
        console = subprocess.Popen([repl, "--server", server, "--timeout", "300", "--attach", str(child.pid)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()
        source = root / "wide.cpp"
        message = "wide-log-" + "x" * 2048
        source.write_text("/*" + "x" * (4 * 1024 * 1024 + 1) + "*/\n" +
                          'struct Interceptor { void pre(const Call &call) {\n'
                          'if (call.arg<int>(39) != 39) throw 1;\n'
                          'call.override_argument(39, 100);\n' +
                          ''.join(f'call.field("field_{i}", {i});\n' for i in range(40)) +
                          f'call.log("{message}");\n' +
                          '} void post(const Call &) {} };\n')
        selector = "wide_probe(" + ", ".join(["int"] * 40) + ")"
        send(f'%probe inject "{selector}" --callback "{source}" --allow-arg-overrides')
        installed = output.expect(r"^(?:installed probe=(\d+)|error: (.*))", timeout=300)
        assert installed[1], installed[2]
        probe = installed[1]
        child.stdin.write("w")
        child.stdin.flush()
        target.expect(r"^wide call returned 841$")
        send(f"%events --probe {probe} --count 2 --timeout 10")
        output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+) ")
        output.expect(rf"^cursor=\d+ .*probe={probe} phase=POST call=(\d+) ")
        output.expect(r"^received=2 .*complete=true")
        send(f"%probe remove {probe}")
        output.expect(rf"^removed probe={probe} restored=true$")
        child.stdin.write("w")
        child.stdin.flush()
        target.expect(r"^wide call returned 780$")
        send("%detach")
        output.expect(r"^detached session=")
        assert message in log.read_text(), "callback log was truncated"
        send("%quit")
        assert console.wait(timeout=15) == 0
        assert child.poll() is None


if __name__ == "__main__":
    main(*sys.argv[1:])
