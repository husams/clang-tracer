"""Exercise source compilation, object caching and reload in the injected agent."""
from contextlib import ExitStack
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, agent, fixture, examples, mode="reuse"):
    with tempfile.TemporaryDirectory(prefix="ct-agent-plugins-") as directory, ExitStack() as stack:
        root = Path(directory)
        env = {**os.environ, "XDG_CACHE_HOME": str(root / "cache"),
               "CLANG_TRACER_ALLOW_AGENT_PLUGINS": "1"}
        target = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  text=True, env=env)
        stack.callback(stop, target)
        Transcript(target.stdout).expect(r"^ready entry=")
        memory = stack.enter_context(open(f"/proc/{target.pid}/mem", "rb", buffering=0))
        command = [repl, "--server", server, "--agent", agent, "--attach", str(target.pid)]
        if mode == "fallback":
            command.extend(["--cpp-arg", "-DTRACER_TEST_DIFFERENT_PLUGIN_PROFILE=1"])
        denied = subprocess.run(
            command, input='%plugin agent load "' + str(examples / "plugin_a.cpp") +
            '" --as a\n%quit\n', text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=25,
            env={**env, "CLANG_TRACER_ALLOW_AGENT_PLUGINS": "0",
                 "XDG_CACHE_HOME": str(root / "denied-cache")},
        )
        assert denied.returncode == 1, denied.stdout
        assert "agent plugins require CLANG_TRACER_ALLOW_AGENT_PLUGINS=1" in denied.stdout

        console = subprocess.Popen(
            command,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, env=env,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={target.pid}$")

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def plugin(name, operation, expected=None):
            send(f"%plugin agent run {name} {operation}")
            value = int(output.expect(rf"^agent {name}\.{operation} = (\d+)$")[1])
            if expected is not None:
                assert value == expected, output.seen
            return value

        def load(name, generation, result):
            source = examples / f"plugin_{name}.cpp"
            if generation == 1:
                send(f'%plugin agent load "{source}" --as {name}')
            else:
                send(f'%plugin agent reload {name} "{source}"')
            output.expect(rf"^agent {name} generation {generation} active \({result}\)$")

        def target_value(address):
            return struct.unpack("=I", os.pread(memory.fileno(), 4, address))[0]

        first_load = "REPL object" if mode == "reuse" else "compiled"
        load("a", 1, first_load)
        load("b", 1, first_load)
        agent_objects = list((root / "cache/clang-tracer/agent/plugins").glob("*.o"))
        repl_objects = list((root / "cache/clang-tracer/repl/plugins").glob("*.o"))
        assert len(agent_objects) == len(repl_objects) == 2
        if mode == "reuse":
            assert {p.read_bytes() for p in agent_objects} == {
                p.read_bytes() for p in repl_objects}
            for name in ("a", "b"):
                source = examples / f"plugin_{name}.cpp"
                send(f'%plugin load "{source}" --as local-{name}')
                output.expect(rf"^local-{name} generation 1 active \(cache hit\)$")
        a, b = plugin("a", "address"), plugin("b", "address")
        assert a != b
        plugin("a", "tick", 1)
        plugin("b", "tick", 10)
        plugin("a", "tick", 2)
        plugin("b", "tick", 20)
        assert target_value(a) == 2 and target_value(b) == 20

        load("a", 2, "cache hit")
        new_a = plugin("a", "address")
        assert new_a not in (a, b)
        assert plugin("b", "address") == b
        plugin("a", "value", 0)
        plugin("b", "value", 20)
        assert target_value(new_a) == 0 and target_value(b) == 20
        plugin("a", "tick", 1)
        plugin("b", "tick", 30)

        broken = root / "broken.cpp"
        broken.write_text((examples / "plugin_b.cpp").read_text().replace(
            "unsigned plugin_counter = 0;", "extern unsigned plugin_counter;"))
        send(f'%plugin agent reload b "{broken}"')
        output.expect(r"^error: .*plugin_counter")
        plugin("b", "value", 30)

        send("%detach")
        output.expect(r"^detached")
        send(f"%attach {target.pid}")
        output.expect(rf"^attached session=\S+ pid={target.pid}$")
        load("a", 1, "cache hit")
        load("b", 1, "cache hit")
        plugin("a", "value", 0)
        plugin("b", "value", 0)
        send("%quit")
        assert console.wait(timeout=20) == 1  # The deliberate failed reload.
        assert target.poll() is None
        print("".join(output.seen), end="")


if __name__ == "__main__":
    main(*sys.argv[1:5], Path(sys.argv[5]), *sys.argv[6:])
