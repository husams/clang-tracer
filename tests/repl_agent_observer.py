"""Exercise automatic attach and live package reload with two C++ observers."""
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
        executable = os.readlink(f"/proc/{child.pid}/exe")
        packages = root / "data/clang-tracer/plugins"
        for name in ("a", "b"):
            package = packages / name
            shutil.copytree(example, package)
            manifest = package / "plugin.toml"
            manifest.write_text(manifest.read_text().replace('id = "latency"', f'id = "{name}"')
                                .replace("/absolute/path/to/your/application", executable))
        source = packages / "a/src/latency.cpp"

        console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, env=env)
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def trigger(command, message):
            child.stdin.write(command)
            child.stdin.flush()
            target.expect(message)
            output.seen.append(target.seen[-1])

        def attached(a_status):
            output.expect(rf"^attached session=\S+ pid={child.pid}$")
            output.expect(rf"^agent a generation 1 active \({a_status}\)$")
            output.expect(r"^agent b generation 1 active \(cache hit\)$")
            send('%probe inject "observed(int)"')
            return output.expect(r"^installed probe=(\d+)")[1]

        def counters(a, b):
            for name, count in (("a", a), ("b", b)):
                send(f"%plugin agent run {name} completed")
                output.expect(rf"^agent {name}\.completed = {count}$")

        def events(probe, after, phases, call=None, minimum_latency=0):
            send(f"%events --probe {probe} --count {len(phases)} --after {after} --timeout 10")
            for phase in phases:
                event_phase = phase if phase in ("PRE", "POST") else "AGENT_OBSERVER"
                suffix = (rf" plugin={phase} elapsed_ns=(\d+) function=observed\(int\)$"
                          if phase in ("a", "b") else " ")
                row = output.expect(
                    rf"^cursor=(\d+) .*probe={probe} phase={event_phase} call=(\d+)" + suffix)
                if call is None:
                    call = row[2]
                assert row[2] == call, output.seen
                if phase in ("a", "b"):
                    assert int(row[3]) >= minimum_latency, output.seen
            output.expect(rf"^received={len(phases)} .*complete=true")
            return int(row[1]), call

        probe = attached("compiled")
        trigger("s", r"^short call returned 12$")
        cursor, first = events(probe, 0, ("PRE", "POST", "a", "b"))
        counters(1, 1)

        trigger("h", r"^call held$")
        time.sleep(.025)
        cursor, held = events(probe, cursor, ("PRE",))
        assert held != first
        send("%plugin agent reload a")
        output.expect(r"^agent a generation 2 active \(cache hit\)$")
        counters(0, 1)

        trigger("s", r"^short call returned 12$")
        cursor, short = events(probe, cursor, ("PRE", "POST", "a", "b"))
        assert short not in (first, held)
        counters(1, 2)
        trigger("r", r"^held call returned 11$")
        cursor, _ = events(probe, cursor, ("POST", "a", "b"), held, minimum_latency=20_000_000)
        counters(2, 3)

        send("%detach")
        output.expect(r"^detached session=")
        send(f"%attach {child.pid}")
        second = attached("cache hit")
        counters(0, 0)
        trigger("s", r"^short call returned 12$")
        events(second, cursor, ("PRE", "POST", "a", "b"))
        counters(1, 1)
        send("%quit")
        assert console.wait(timeout=20) == 0 and child.poll() is None
        assert source.read_bytes() == (Path(example) / "src/latency.cpp").read_bytes()
        print("".join(output.seen), end="")


if __name__ == "__main__":
    main(*sys.argv[1:])
