"""Verify installed agent package dispatch against the portable REPL RPC fixture."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile


def main():
    console, fixture = sys.argv[1:3]
    with tempfile.TemporaryDirectory(prefix="tracer-agent-attach-rpc-") as directory:
        root = Path(directory)
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
        server = subprocess.Popen([fixture, "--listen", endpoint], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)
        try:
            assert server.stdout.readline().strip() == "ready"
            for name, target, flags in (
                ("source", "/test/application", ""),
                ("bundle", "/test/application", 'extra_flags = ["-DTEST_VALUE=7"]\n'),
                ("other", "/test/elsewhere", ""),
            ):
                package = root / "data/clang-tracer/plugins" / name
                (package / "src").mkdir(parents=True)
                (package / "src/plugin.cpp").write_text("// automatic agent fixture\n")
                (package / "plugin.toml").write_text(
                    f'id = "{name}"\nsdk = 1\n[agent]\nsources = ["src/plugin.cpp"]\n'
                    f'{flags}[agent.target]\nexecutable = "{target}"\n'
                )
            commands = """%plugin agent run source tick
%plugin agent run bundle tick
%attach 7
%detach
%attach 7
%plugin agent run source tick
%quit
"""
            result = subprocess.run(
                [console, "--endpoint", endpoint, "--attach", "7"],
                input=commands, text=True, capture_output=True, timeout=20,
                env=dict(os.environ, XDG_DATA_HOME=str(root / "data"),
                         XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache")),
            )
            # gRPC itself reports this on hosts where sched_getcpu() is unavailable (the SDK
            # loader on a newer kernel); it says nothing about the console.
            stderr = "".join(line for line in result.stderr.splitlines(True)
                             if "cpu_linux.cc" not in line or "Error determining current CPU" not in line)
            assert result.returncode == 0 and not stderr.strip(), result
            for name in ("source", "bundle"):
                assert result.stdout.count(f"agent {name} generation 1 active (compiled)") == 2, result.stdout
            assert result.stdout.count("agent source.tick = 1") == 2, result.stdout
            assert "agent bundle.tick = 1" in result.stdout, result.stdout
            assert "agent other" not in result.stdout, result.stdout
            print(result.stdout, end="")
        finally:
            server.terminate()
            server.communicate(timeout=10)


if __name__ == "__main__":
    main()
