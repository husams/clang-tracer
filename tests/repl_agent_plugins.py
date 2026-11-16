"""Exercise source compilation, object caching and reload in the injected agent."""
from contextlib import ExitStack
import json
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
        env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_DATA_HOME=str(root / "data"),
                   CLANG_TRACER_ALLOW_AGENT_PLUGINS="1")
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



def autoload(repl, server, agent, fixture, examples):
    with tempfile.TemporaryDirectory(prefix="ct-agent-autoload-") as directory, ExitStack() as stack:
        root = Path(directory)
        env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_DATA_HOME=str(root / "data"),
                   CLANG_TRACER_ALLOW_AGENT_PLUGINS="1")
        target = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  text=True, env=env)
        stack.callback(stop, target)
        Transcript(target.stdout).expect(r"^ready entry=")
        exact = os.readlink(f"/proc/{target.pid}/exe")
        memory = stack.enter_context(open(f"/proc/{target.pid}/mem", "rb", buffering=0))

        def install(name, executable, sources, extra=""):
            package = root / "data/clang-tracer/plugins" / name
            (package / "src").mkdir(parents=True)
            manifest = (f"id = {json.dumps(name)}\nsdk = 1\n[agent]\n"
                        f"sources = {json.dumps(sources)}\n{extra}"
                        f"[agent.target]\nexecutable = {json.dumps(executable)}\n")
            (package / "plugin.toml").write_text(manifest)
            return package

        single = install("a", exact, ["src/plugin.cpp"])
        (single / "src/plugin.cpp").write_text((examples / "plugin_a.cpp").read_text())
        bundled = install("b", exact, ["src/plugin.cpp", "src/state.cpp"],
                          'extra_flags = ["-DAGENT_STEP=10"]\n')
        (bundled / "include").mkdir()
        (bundled / "include/agent_step.hpp").write_text("constexpr unsigned step = AGENT_STEP;\n")
        (bundled / "src/state.hpp").write_text("#pragma once\nextern unsigned plugin_counter;\n")
        (bundled / "src/state.cpp").write_text('#include "state.hpp"\nunsigned plugin_counter = 0;\n')
        body = (examples / "plugin_b.cpp").read_text().replace(
            "unsigned plugin_counter = 0;", '#include "state.hpp"\n#include <agent_step.hpp>'
        ).replace("+= 10", "+= step")
        (bundled / "src/plugin.cpp").write_text(body)
        mismatch = install("mismatch", exact + ".unapproved", ["src/plugin.cpp"])
        (mismatch / "src/plugin.cpp").write_text((single / "src/plugin.cpp").read_text())

        command = [repl, "--server", server, "--agent", agent, "--attach", str(target.pid),
                   "--cpp-arg", "-DTRACER_TEST_DIFFERENT_PLUGIN_PROFILE=1"]
        denied = subprocess.run(command, input="%quit\n", text=True, capture_output=True,
                                timeout=30, env={**env, "CLANG_TRACER_ALLOW_AGENT_PLUGINS": "0"})
        assert denied.returncode == 0 and "attached session=" in denied.stdout, denied
        assert "agent plugins require CLANG_TRACER_ALLOW_AGENT_PLUGINS=1" in denied.stderr, denied
        assert "mismatch" not in denied.stderr, denied

        console = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, env=env)
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(text):
            console.stdin.write(text + "\n")
            console.stdin.flush()

        def call(name, operation):
            send(f"%plugin agent run {name} {operation}")
            return int(output.expect(rf"^agent {name}\.{operation} = (\d+)$")[1])

        def loaded(status):
            output.expect(rf"^attached session=\S+ pid={target.pid}$")
            for name in ("a", "b"):
                output.expect(rf"^agent {name} generation 1 active \({status}\)$")
            assert "mismatch" not in "".join(output.seen), output.seen

        loaded("compiled")
        a, b = call("a", "address"), call("b", "address")
        assert a != b
        assert call("a", "tick") == 1 and call("b", "tick") == 10
        assert struct.unpack("=I", os.pread(memory.fileno(), 4, a))[0] == 1
        assert struct.unpack("=I", os.pread(memory.fileno(), 4, b))[0] == 10

        send("%detach")
        output.expect(r"^detached")
        send(f"%attach {target.pid}")
        loaded("cache hit")
        assert call("a", "value") == 0 and call("b", "value") == 0
        assert call("a", "tick") == 1 and call("b", "tick") == 10
        send("%quit")
        assert console.wait(timeout=20) == 0 and target.poll() is None
        print("".join(output.seen), end="")


if __name__ == "__main__":
    args = (*sys.argv[1:5], Path(sys.argv[5]))
    if sys.argv[6:] == ["autoload"]:
        autoload(*args)
    else:
        main(*args, *sys.argv[6:])
