#!/usr/bin/env python3
"""Keep method callback syntax diagnostics ahead of receiver/record ABI errors."""

import argparse
from contextlib import ExitStack
from pathlib import Path
import re
import subprocess
import tempfile
import time

from typed_this_integration import require, run, stop, text


def main():
    parser = argparse.ArgumentParser()
    for name in ("server", "client", "agent", "target", "output-dir"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    root = args.output_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    with ExitStack() as stack:
        socket_dir = stack.enter_context(tempfile.TemporaryDirectory(prefix="ct-syntax-"))
        endpoint = f"unix://{socket_dir}/server.sock"
        target_log = stack.enter_context((root / "target.log").open("wb"))
        target = subprocess.Popen([str(args.target)], stdout=target_log, stderr=target_log)
        stack.callback(stop, target)
        server_log = stack.enter_context((root / "server.log").open("wb"))
        server = subprocess.Popen(
            [str(args.server), "--agent", str(args.agent), "--listen", endpoint],
            stdout=server_log, stderr=server_log)
        stack.callback(stop, server)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            require(server.poll() is None and target.poll() is None,
                    "server or target exited before readiness")
            ready = subprocess.run([str(args.client), "--endpoint", endpoint, "list"],
                                   capture_output=True, timeout=3, check=False)
            if ready.returncode == 0:
                break
            time.sleep(.1)
        else:
            raise RuntimeError("server did not become ready")
        run(args.client, endpoint, "attach", str(target.pid))
        syntax = root / "syntax-error.cpp"
        syntax.write_text('class Interceptor {\npublic:\n'
                          '  void pre(const Call &call) { call.field("x", 1 }\n};\n')
        record_method = "RecordCalculator::label() const"
        for selector in ("InheritedCalculator::read(int) const", record_method):
            for level in ("syntax", "compile"):
                diagnostic = text(run(
                    args.client, endpoint, "validate", str(target.pid), selector,
                    "--callback", str(syntax), "--level", level, ok=False, timeout=45))
                (root / f"{selector.split('::')[0]}-{level}.log").write_text(diagnostic)
                require("expected ')'" in diagnostic and
                        re.search(r"callback_source:3:\d+", diagnostic),
                        f"{selector} {level} hid callback syntax/location: {diagnostic}")
                require("layout changed" not in diagnostic and "record ABI" not in diagnostic,
                        f"ABI error masked callback syntax: {diagnostic}")
        plain = root / "plain-observer.cpp"
        plain.write_text("class Interceptor { public: void pre(const Call&) {} };\n")
        valid = text(run(
            args.client, endpoint, "validate", str(target.pid),
            "PointerCalculator::read(int) const", "--callback", str(plain), timeout=45))
        require("validation success=true" in valid,
                f"valid method callback failed after syntax rejection: {valid}")
        abi_error = text(run(
            args.client, endpoint, "validate", str(target.pid), record_method,
            "--callback", str(plain), ok=False, timeout=45))
        require("record ABI" in abi_error or
                "standard-library record types require their real headers" in abi_error,
                f"valid callback lost its legitimate ABI diagnostic: {abi_error}")
        run(args.client, endpoint, "detach", str(target.pid))
        require(target.poll() is None, "target exited during validation or detach")
        print("callback syntax locations, valid callback, and ABI rejection passed")


if __name__ == "__main__":
    main()
