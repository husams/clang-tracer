"""Trace the first C++ call after an owned application's startup checkpoint."""

from contextlib import ExitStack
from pathlib import Path
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop


def main(repl, server, fixture):
    with tempfile.TemporaryDirectory(prefix="ct-launch-") as directory, ExitStack() as stack:
        root = Path(directory).resolve()
        console = subprocess.Popen(
            [repl, "--server", server],
            cwd=root,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def launch(options="", ready=True, executable=fixture):
            send(f'%launch "{executable}" {options}')
            row = output.expect(r'^launching pid=(\d+) stdout="([^"]+)" stderr="([^"]+)"$')
            pid, stdout, stderr = int(row[1]), Path(row[2]), Path(row[3])
            if ready:
                output.expect(rf"^launched pid={pid} session=\S+ paused=true$")
            return pid, stdout, stderr

        def reaped(pid):
            assert not Path(f"/proc/{pid}").exists(), f"owned PID {pid} was not reaped"

        def terminate(pid):
            send(f"%terminate {pid}")
            output.expect(rf"^terminated pid={pid}$")
            output.expect(r"^detached session=\S+$")
            reaped(pid)

        def launch_default(options="", ready=True, executable=fixture):
            pid, stdout, stderr = launch(options, ready, executable)
            stack.callback(stdout.parent.rmdir)
            stack.callback(stderr.unlink, missing_ok=True)
            stack.callback(stdout.unlink, missing_ok=True)
            return pid, stdout, stderr

        (root / "work space").mkdir()
        (root / "input.txt").write_text("fixture input\n")
        ordinary = subprocess.run(
            [fixture, "--exit-after-start"], cwd=root, input="plain run\n",
            capture_output=True, text=True, timeout=5, check=True,
        )
        assert "result=17\n" in ordinary.stdout and "stdin=plain run\n" in ordinary.stdout
        (root / "observer.cpp").write_text(
            'struct Interceptor { void pre(const Call &) {} '
            'void post(const Call &call) { call.field("startup", 1); } };'
        )
        pid, stdout, stderr = launch(
            '--arg "hello world" --arg "second" --cwd "work space" '
            '--env TRACER_LAUNCH_TEST=overridden --env "TRACER_LAUNCH_TEST=fixture value" '
            '--stdin input.txt --stdout out.txt --stderr err.txt'
        )
        assert stdout == root / "out.txt" and stderr == root / "err.txt"

        def call(command, result):
            send(f"%call {command}")
            output.expect(rf"^call result pid={pid} {result}$")

        send("%call-result")
        output.expect("no pending target call")
        call("app::startup-thread", "type=bool value=true")
        call("app::float --arg 1.25", r"type=float value=1\.25")
        call("app::double --arg -2.5", r"type=double value=-2\.5")
        call("app::void", "type=void")
        send("%call app::unknown")
        output.expect("no function registered under this name")
        send("%call app::duplicate")
        output.expect("more than one function is registered under this name")
        send("%call app::scalar --arg 1")
        output.expect("registered function expects 7 arguments")
        scalar_args = (
            "--arg -2147483648 --arg 4294967295 --arg -9223372036854775808 "
            "--arg 18446744073709551615 --arg true --arg 1.25 --arg -2.5"
        )
        for value, invalid, index in (
            ("-2147483648", "-2147483649", 1),
            ("4294967295", "-1", 2),
            ("18446744073709551615", "18446744073709551616", 4),
            ("true", "1", 5),
            ("1.25", "1e100", 6),
            ("-2.5", "1.0junk", 7),
        ):
            send(f"%call app::scalar {scalar_args.replace(value, invalid)}")
            output.expect(rf"argument {index} is not valid for the registered scalar type")
        call("app::count", "type=uint32 value=0")
        call(f"app::scalar {scalar_args}", "type=uint64 value=18446744073709551615")
        send("%call app::throw")
        output.expect(r"target function threw a C\+\+ exception")
        call("app::count", "type=uint32 value=1")
        send("%call app::held --timeout 1")
        output.expect("target call wait timed out; function may still be running")
        send("%resume")
        output.expect("target call pending; use %call-result or %terminate")
        send("%call app::count")
        output.expect("target call pending; use %call-result or %terminate")
        send("%probe remove 1")
        output.expect("target call pending; use %call-result or %terminate")
        send("%threads")
        output.expect(rf"^tid={pid} start=\d+ name=")
        assert stdout.read_text() == stderr.read_text() == ""
        (root / "work space" / "release-call").touch()
        send("%call-result --timeout 2")
        output.expect(rf"^call result pid={pid} type=int64 value=-9000000000$")
        call("app::count", "type=uint32 value=2")
        send("%detach")
        output.expect(r"resume or terminate the paused target before detaching")
        send("%server stop")
        output.expect(r"terminate owned targets before changing server")
        send('%probe inject "startup_call(int)" --callback observer.cpp')
        probe = output.expect(r"^installed probe=(\d+)")[1]
        assert stdout.read_text() == stderr.read_text() == ""

        call("app::startup --arg 9", "type=int32 value=19")
        send(f"%events --probe {probe} --count 2 --timeout 10")
        invoked = output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+)")
        explicit = output.expect(
            rf"^cursor=(\d+) .*probe={probe} phase=POST call={invoked[1]} .* startup=1 result=19(?: |$)"
        )
        output.expect(r"^received=2 .*complete=true")

        send("%resume")
        output.expect(rf"^resumed pid={pid}$")
        send(f"%events --probe {probe} --count 2 --after {explicit[1]} --timeout 10")
        pre = output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+)")
        output.expect(
            rf"^cursor=\d+ .*probe={probe} phase=POST call={pre[1]} .* startup=1 result=17(?: |$)"
        )
        output.expect(r"^received=2 .*complete=true")
        send(f"%resume {pid}")
        output.expect(r"target has already resumed")
        send("%call app::count")
        output.expect("target calls require a paused checkpoint with registered functions")
        # An attached event stream can receive POST just before the program
        # writes its logs. Terminate only after the writes have completed.
        for path in (stdout, stderr):
            wait_for_log(path)
        terminate(pid)
        assert stdout.read_text() == (
            f"result=17\ncwd={root / 'work space'}\nargs=hello world,second\n"
            "env=fixture value\nstdin=fixture input\n"
        )
        assert stderr.read_text() == "target stderr\n"

        send(f'%launch "{fixture}" --stdout out.txt --stderr other.txt')
        output.expect(r"cannot create target stdout .*File exists")
        assert stdout.read_text().startswith("result=17\n") and not (root / "other.txt").exists()
        send(f'%launch "{fixture}" --stdout new.txt --stderr err.txt')
        output.expect(r"cannot create target stderr .*File exists")
        assert not (root / "new.txt").exists() and stderr.read_text() == "target stderr\n"
        send(f"%terminate {console.pid}")
        output.expect(r"specify a PID launched by this REPL")

        early, _, _ = launch_default('--arg "--early"', ready=False)
        output.expect(r"target exited with status 7 before readiness")
        reaped(early)
        slow, _, _ = launch_default('--arg "--no-ready" --timeout 1', ready=False)
        output.expect(r"target readiness timed out")
        reaped(slow)
        bad = root / "cannot-exec"
        bad.write_text("not executable\n")
        failed, _, _ = launch_default(ready=False, executable=bad)
        output.expect(r"target launch could not exec: Permission denied")
        reaped(failed)

        finished, _, stderr = launch_default('--arg "--exit-after-start" --stdin input.txt')
        send(f"%resume {finished}")
        output.expect(rf"^resumed pid={finished}$")
        wait_for_log(stderr)
        stat = Path(f"/proc/{finished}/stat")
        deadline = time.monotonic() + 5
        while stat.read_text().rsplit(")", 1)[1].split()[0] != "Z":
            assert time.monotonic() < deadline
            time.sleep(0.01)
        send("%status")
        output.expect(r"^ready=true$")
        reaped(finished)
        send("%detach")
        output.expect(r"^detached session=\S+$")

        send("%server stop")
        output.expect(r"^owned server stopped after restoration$")
        send(f'%server start --agent "{bad}"')
        output.expect(r"^server ready endpoint=")
        unattached, stdout, stderr = launch_default(ready=False)
        output.expect(r"error: Attach RPC ")
        reaped(unattached)
        assert stdout.read_text() == stderr.read_text() == ""
        send("%server stop")
        output.expect(r"^owned server stopped after restoration$")

        paused, stdout, stderr = launch_default('--arg "--no-calls"')
        send("%call app::count")
        output.expect("target calls require a paused checkpoint with registered functions")
        terminate(paused)
        assert stdout.read_text() == stderr.read_text() == ""
        leaving, stdout, stderr = launch_default()
        send("%call app::held --timeout 1")
        output.expect("target call wait timed out; function may still be running")
        send("%quit")
        assert console.wait(timeout=15) == 1, "".join(output.seen)
        reaped(leaving)
        assert stdout.read_text() == stderr.read_text() == ""


def wait_for_log(path):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if path.read_text():
            return
        time.sleep(0.01)
    raise AssertionError(f"target did not write {path}")


if __name__ == "__main__":
    main(*sys.argv[1:])
