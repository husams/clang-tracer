"""Verify an installed C++ observer registers on attach and emits target returns."""
from contextlib import ExitStack
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop


def main(repl, server, agent, fixture, example):
    with tempfile.TemporaryDirectory(prefix="ct-agent-observer-") as directory, ExitStack() as stack:
        root = Path(directory)
        env = dict(os.environ, XDG_DATA_HOME=str(root / "data"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"),
                   CLANG_TRACER_ALLOW_AGENT_PLUGINS="1")
        child = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 text=True, env=env)
        stack.callback(stop, child)
        target = Transcript(child.stdout)
        target.expect(r"^ready entry=")
        package = root / "data/clang-tracer/plugins/latency"
        shutil.copytree(example, package)
        manifest = package / "plugin.toml"
        manifest.write_text(manifest.read_text().replace("/absolute/path/to/your/application",
                                                         os.readlink(f"/proc/{child.pid}/exe")))
        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, env=env)
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def trigger(command):
            child.stdin.write(command)
            child.stdin.flush()

        def attached(result):
            output.expect(rf"^attached session=\S+ pid={child.pid}$")
            output.expect(rf"^agent latency generation 1 active \({result}\)$")
            send('%probe inject "observed(int)"')
            return output.expect(r"^installed probe=(\d+)")[1]

        def events(probe, after):
            send(f"%events --probe {probe} --count 3 --after {after} --timeout 10")
            calls = [output.expect(rf"^cursor=\d+ .*probe={probe} phase={phase} call=(\d+) ")
                     for phase in ("PRE", "POST")]
            emitted = output.expect(
                rf"^cursor=(\d+) probe={probe} phase=AGENT_OBSERVER call=(\d+) plugin=latency "
                r"elapsed_ns=(\d+) function=observed\(int\)$")
            assert calls[0][1] == calls[1][1] == emitted[2]
            output.expect(r"^received=3 .*complete=true")
            send("%plugin agent run latency completed")
            output.expect(r"^agent latency.completed = 1$")
            return int(emitted[1]), int(emitted[3])

        first = attached("compiled")
        trigger("h")
        target.expect(r"^call held$")
        time.sleep(.025)
        trigger("r")
        target.expect(r"^held call returned 11$")
        cursor, elapsed = events(first, 0)
        assert elapsed >= 20_000_000, elapsed

        send("%detach")
        output.expect(r"^detached session=")
        send(f"%attach {child.pid}")
        second = attached("cache hit")
        trigger("s")
        target.expect(r"^short call returned 12$")
        events(second, cursor)
        send("%quit")
        assert console.wait(timeout=20) == 0 and child.poll() is None
        print("".join(output.seen), end="")


if __name__ == "__main__":
    main(*sys.argv[1:])
