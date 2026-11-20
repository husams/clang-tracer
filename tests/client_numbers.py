"""Check public CLI numeric diagnostics before any server connection."""
import os
import subprocess
import sys


def main():
    client = sys.argv[1]
    environment = dict(os.environ)
    environment.pop("CLANG_TRACER_ENDPOINT", None)
    # --help exits after parsing the preceding option, without contacting a server.
    for option, maximum, allow_zero in (
        ("--after", 2**64 - 1, True),
        ("--debug-max-entries", 2**64 - 1, True),
        ("--debug-max-depth", 2**32 - 1, True),
        ("--count", 1_000_000, False),
        ("--timeout", 86_400, False),
        ("--stack", 2**32 - 1, False),
    ):
        def run(value):
            return subprocess.run(
                [client, option, str(value), "--help"], env=environment,
                text=True, capture_output=True, timeout=10)

        for value in [1, maximum, "0001"] + ([0] if allow_zero else []):
            result = run(value)
            assert result.returncode == 0 and result.stdout.startswith("Usage:"), (
                option, value, result)
            assert not result.stderr, (option, value, result.stderr)
        for value in ("", "abc", "1x", "-1", "+1", " 1", 2**64, "99999999999999999999"):
            result = run(value)
            assert result.returncode == 1 and not result.stdout, (option, value, result)
            assert result.stderr == f"clang-tracer: invalid number for {option}\n", (
                option, value, result.stderr)
        for value in ([maximum + 1] if maximum < 2**64 - 1 else []) + (
                [] if allow_zero else [0]):
            result = run(value)
            minimum = 0 if allow_zero else 1
            assert result.returncode == 1 and not result.stdout, (option, value, result)
            assert result.stderr == (
                f"clang-tracer: {option} must be between {minimum} and {maximum}\n"
            ), (option, value, result.stderr)


if __name__ == "__main__":
    main()
