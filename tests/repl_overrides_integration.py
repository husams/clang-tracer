"""Exercise opt-in mutations through the real Clang JIT wrapper and local REPL."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop


def main(repl, server, fixture):
    with tempfile.TemporaryDirectory(prefix="ct-overrides-") as directory:
        root = Path(directory)
        target_log = root / "stdout.log"
        files = {
            "observer": """#include <cerrno>
struct Interceptor {
  Interceptor() { errno = EBADF; }
  ~Interceptor() { errno = ERANGE; }
  void pre(const Call &) { errno = EBADF; }
  void post(const Call &) { errno = ERANGE; }
};
""",
            "arguments": """#include <cerrno>
struct Interceptor {
  void pre(const Call &call) {
    errno = EBADF;
    if (call.arg<int>(0) == 0) call.override_argument(6, 40);
  }
  void post(const Call &) { errno = ERANGE; }
};
""",
            "result": """#include <cerrno>
struct Interceptor {
  void pre(const Call &) { errno = EBADF; }
  void post(const Call &call) {
    errno = ERANGE;
    if (call.arg<int>(0) == 1) call.override_result(123.25);
  }
};
""",
            "both": """#include <cerrno>
struct Interceptor {
  void pre(const Call &call) {
    errno = EBADF;
    if (call.arg<int>(0) == 0) call.override_argument(6, 40);
  }
  void post(const Call &call) {
    errno = ERANGE;
    if (call.arg<int>(0) == 1) call.override_result(123.25);
  }
};
""",
            "bad_argument": """struct Interceptor {
  void pre(const Call &call) {
    call.override_argument(6, 40);
    call.override_argument(1, 9.0);
  }
};
""",
            "bad_result": """struct Interceptor {
  void post(const Call &call) {
    call.override_result(123.25);
    throw 17;
  }
};
""",
            "float": """struct Interceptor {
  void pre(const Call &call) { call.override_argument(0, 1.25f); }
  void post(const Call &call) {
    if (call.result<float>() == 2.25f) call.override_result(3.5f);
  }
};
""",
            "integer": """struct Interceptor {
  void pre(const Call &call) { call.override_argument(1, true); }
  void post(const Call &call) {
    if (call.result<long long>() == -9000000000LL)
      call.override_result(-9223372036854775807LL - 1);
  }
};
""",
        }
        for name, source in files.items():
            (root / f"{name}.cpp").write_text(source)
        console = subprocess.Popen(
            [repl, "--server", server], cwd=root, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        try:
            output = Transcript(console.stdout)

            def send(command):
                console.stdin.write(command + "\n")
                console.stdin.flush()

            send(f'%launch "{fixture}" --stdout stdout.log --stderr stderr.log')
            pid = output.expect(r"^launched pid=(\d+) session=\S+ paused=true$")[1]
            selector = "calculate(int, int, long long, unsigned long long, bool, int, int, double)"
            args = "--allow-arg-overrides"
            result = "--allow-result-override"

            def install(hook, flags="", target=selector):
                send(f'%probe inject "{target}" --callback {hook}.cpp {flags}')
                return output.expect(r"^installed probe=(\d+)")[1]

            def remove(probe):
                send(f"%probe remove {probe}")
                output.expect(rf"^removed probe={probe} restored=true$")

            def call(command, kind, value):
                send(f"%call {command}")
                output.expect(rf"^call result pid={pid} type={kind} value={re.escape(value)}$")

            def scenario(value, depth=1, error="4"):
                call(f"scenario --arg {depth}", "double", value)
                call("read_errno", "int32", error)

            send(f'%probe validate "{selector}" --callback both.cpp {args} {result}')
            output.expect(r"^success: true$")
            for rejected, flag, diagnostic in (
                ("pointer_sample(int*)", args, "scalar overrides require an entirely scalar signature"),
                ("void_sample(int)", result, "result overrides require a nonvoid return type"),
                ("other_abi(int, double)", args, "nondefault debug calling convention is unsupported"),
                ("unmarked(int)", args, "scalar overrides require TRACER_SCALAR_OVERRIDE_TARGET"),
            ):
                send(f'%probe inject "{rejected}" --callback both.cpp {flag}')
                output.expect(diagnostic)

            probe = install("observer")
            scenario("23.5")
            remove(probe)

            # Calling the API without permission leaves the original behavior.
            probe = install("both")
            scenario("23.5")
            remove(probe)
            probe = install("arguments", args)
            scenario("59.5")
            remove(probe)
            probe = install("result", result)
            scenario("123.25")
            remove(probe)

            # A bad second write or a throwing hook must discard staged writes.
            probe = install("bad_argument", args)
            scenario("23.5")
            remove(probe)
            probe = install("bad_result", result)
            scenario("23.5")
            remove(probe)

            probe = install("float", f"{args} {result}", "float_sample(float)")
            call("float_sample --arg 8", "float", "3.5")
            remove(probe)
            probe = install("integer", f"{args} {result}", "integer_sample(long long, bool)")
            call("integer_sample --arg -9000000000 --arg false", "int64", "-9223372036854775808")
            remove(probe)

            probe = install("both", f"{args} {result}")
            send("%probes")
            output.expect(rf"^id: {probe} .*allow_argument_overrides: true allow_result_override: true")
            scenario("123.25")
            scenario("-17", depth=-1, error="33")
            send(f"%events --probe {probe} --count 5 --timeout 5")
            outer = output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+) .*tail=4")
            leaf = output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+) .*tail=4")
            assert leaf[1] != outer[1]
            output.expect(rf"^cursor=\d+ .*probe={probe} phase=POST call={leaf[1]} .*tail=40.*result=58\.5")
            output.expect(rf"^cursor=\d+ .*probe={probe} phase=POST call={outer[1]} .*tail=4.*result=59\.5")
            output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=\d+ .*depth=-1")
            output.expect(r"^received=5 .*complete=true")

            send("%resume")
            output.expect(rf"^resumed pid={pid}$")
            wait_for_line(target_log, "resumed result=123.25 errno=4")
            send("%detach")
            output.expect(r"^detached session=\S+$")
            (root / "after-detach").touch()
            wait_for_line(target_log, "detached result=23.50 errno=4")
            send(f"%terminate {pid}")
            output.expect(rf"^terminated pid={pid}$")
            send("%quit")
            assert console.wait(timeout=15) == 1, "".join(output.seen)
        finally:
            stop(console)


def wait_for_line(path, expected):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if expected in path.read_text().splitlines():
            return
        time.sleep(0.01)
    raise AssertionError((expected, path.read_text()))


if __name__ == "__main__":
    main(*sys.argv[1:])
