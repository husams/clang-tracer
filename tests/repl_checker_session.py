"""Protocol and isolation contract for the persistent REPL checker."""

import argparse
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import time


class Checker:
    def __init__(self, executable, compiler_args=()):
        self.process = subprocess.Popen(
            [executable, "serve", "0", *compiler_args],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            bufsize=0,
        )
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)

    def _read_exactly(self, size, timeout=20):
        result = bytearray()
        deadline = time.monotonic() + timeout
        while len(result) < size:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise TimeoutError("checker response timed out")
            chunk = os.read(self.process.stdout.fileno(), size - len(result))
            if not chunk:
                raise AssertionError("checker closed stdout before completing a reply")
            result.extend(chunk)
        return bytes(result)

    def request(self, source, history=()):
        history_bytes = b"".join(
            str(len(cell)).encode("ascii") + b"\n" + cell for cell in history
        )
        if isinstance(source, str):
            source = source.encode()
        self.process.stdin.write(
            f"{len(history_bytes)} {len(source)}\n".encode("ascii")
            + history_bytes + source
        )
        self.process.stdin.flush()
        header = bytearray()
        deadline = time.monotonic() + 20
        while not header.endswith(b"\n"):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise TimeoutError("checker response header timed out")
            byte = os.read(self.process.stdout.fileno(), 1)
            if not byte:
                raise AssertionError("checker closed stdout before its reply header")
            header.extend(byte)
        status, length = map(int, header[:-1].split())
        assert 0 <= length <= 65536, f"invalid diagnostic length: {length}"
        return status, self._read_exactly(length)

    def close(self):
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
        self.selector.close()
        assert self.process.returncode == 0, self.process.stderr.read().decode(errors="replace")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("checker")
    parser.add_argument("compiler_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    compiler_args = args.compiler_args
    if compiler_args[:1] == ["--"]:
        compiler_args = compiler_args[1:]
    if not compiler_args:
        repl = Path(args.checker).with_name("clang-tracer-repl")
        options = subprocess.run(
            [str(repl), "--endpoint", "127.0.0.1:1"],
            input="%cpp options\n%quit\n", text=True, capture_output=True,
            timeout=10,
        )
        assert options.returncode == 0, (options.stdout, options.stderr)
        heading = "Local C++ compiler arguments (in order):"
        assert heading in options.stdout, (options.stdout, options.stderr)
        compiler_args = [
            line[2:] for line in options.stdout.splitlines()
            if line.startswith("  ") and line[2:]
        ]

    checker = Checker(args.checker, compiler_args)
    with tempfile.TemporaryDirectory(prefix="tracer-checker-session-") as directory:
        marker = Path(directory) / "initializer-ran"
        try:
            accepted = b"#define SESSION_VALUE 29\nconstexpr int kept = 1;\n"
            assert checker.request(accepted)[0] == 0
            history = (accepted,)
            status, diagnostic = checker.request(
                "static_assert(SESSION_VALUE == 29);\nstatic_assert(kept == 1);", history
            )
            assert status == 0, diagnostic.decode(errors="replace")

            later = b"constexpr int later = 7;\n"
            grown_history = history + (later,)
            status, diagnostic = checker.request(
                "static_assert(kept + later == 8);", grown_history
            )
            assert status == 0, diagnostic.decode(errors="replace")
            status, _ = checker.request("static_assert(kept == 1);", ())
            assert status == 1, "rewound history retained declarations from the old prefix"

            status, diagnostic = checker.request("struct Rejected {}\nint nope = absent_name;\n")
            assert status == 1, diagnostic.decode(errors="replace")
            status, diagnostic = checker.request("Rejected value;\n")
            assert status == 1, "rejected source poisoned the next candidate"

            assert checker.request("int candidate_only = 42;\n")[0] == 0
            status, diagnostic = checker.request("static_assert(candidate_only == 42);\n")
            assert status == 1, "successful candidate source leaked without accepted history"

            status, _ = checker.request(
                "missing_semicolon()", history + (b"void missing_semicolon() {}",)
            )
            assert status == 2, "missing void semicolon was not classified separately"
            for source in (
                "int values[1] = {}; for (const auto& value : values) (void)value",
                "int values[1] = {}; for (int i = 0; i < 1; ++i) (void)i",
                "if (true) (void)0",
                "while (false) (void)0",
            ):
                status, diagnostic = checker.request(source)
                assert status == 1 and b"expected ';'" in diagnostic, (source, status, diagnostic)
            assert checker.request("int values[1] = {}; for (const auto& value : values) (void)value;")[0] == 0
            assert checker.request("int values[1] = {}; for (const auto& value : values) { (void)value; }")[0] == 0
            legacy = subprocess.run(
                [args.checker, "check", "0", *compiler_args],
                input="int values[1] = {}; for (const auto& value : values) (void)value",
                text=True, capture_output=True, timeout=20,
            )
            assert legacy.returncode == 1 and "expected ';'" in legacy.stderr, (legacy.stdout, legacy.stderr)

            path_literal = json.dumps(str(marker))
            side_effect = (
                "#include <fstream>\n"
                f"int initializer = [] {{ std::ofstream({path_literal}) << 'x'; return 1; }}();\n"
            )
            status, diagnostic = checker.request(side_effect)
            assert status == 0, diagnostic.decode(errors="replace")
            assert not marker.exists(), "syntax-only checker executed a global initializer"
        finally:
            checker.close()

    malformed = subprocess.Popen(
        [args.checker, "serve", "0", *compiler_args], stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    try:
        stdout, _ = malformed.communicate(b"not-a-frame\n", timeout=10)
        assert malformed.returncode != 0 and not stdout, (
            "malformed protocol input must fail closed", malformed.returncode, stdout
        )
    finally:
        if malformed.poll() is None:
            malformed.kill()
            malformed.wait(timeout=5)
    print("persistent checker history, isolation and protocol passed")


if __name__ == "__main__":
    main()
