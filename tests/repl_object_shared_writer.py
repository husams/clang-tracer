"""Exercise a same-DSO PLT call while watching a field through the native REPL."""

from contextlib import ExitStack
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, send, stop


def start(argv, cwd, cleanup, timeout=30):
    process = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)
    cleanup.callback(stop, process)
    return process, Output(process.stdout, timeout)


def expect_error(out, pattern):
    row = out.expect(r"^error: (.*)$")
    assert pattern in row[1], row.string


def capture(root, args, app, app_out, repl, repl_out, session, endpoint, module, executable, label):
    package = root / f"{label}-plan"
    if executable:
        send(repl, f'%object prepare shared_writer::Record --executable "{args.fixture}" '
                   f'--module "{module}" --field value '
                   f'--code "shared_writer::write(shared_writer::Record &, int, bool)" '
                   f'--backend code-cache --follow-calls --output "{package}"')
        prepared = repl_out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(repl, f"%object start --plan {package}")
    else:
        send(repl, '%object start --type shared_writer::Record --field value '
                   '--code "shared_writer::write(shared_writer::Record &, int, bool)" '
                   f'--module "{module}" --backend code-cache --follow-calls')
    watch = repl_out.expect(r"^watch=(\d+) active=yes .*|^error:")
    assert watch[1], "".join(repl_out.seen[-20:])
    probe = watch[1]
    database = root / f"{label}.db"
    export = root / f"{label}.json"
    recorder, recording = start([args.repl, "--endpoint", endpoint],
                                root, cleanup)
    send(recorder, f"%record {database} --sqlite --session {session} --name writes "
                   f"--probe {probe} --count 9 --timeout 20")
    send(app, "g")
    app_out.expect(r"^DONE value=7$")
    recording.expect(r"^received=9 .*complete=true")
    send(recorder, "%quit")
    assert recorder.wait(timeout=10) == 0
    send(repl, f"%object status {probe}")
    status = repl_out.expect(rf"^watch={probe} active=yes .*$")
    assert "executed_stores=6" in status.string and "coverage=partial" not in status.string, status.string
    send(repl, f"%object stop {probe}")
    repl_out.expect(rf"^watch={probe} active=no .*restoration_pending=no .*")
    send(repl, f"%events open {database} --as {label}")
    repl_out.expect(r"^opened events=9 retained=9 evicted=0$")
    send(repl, f"%events export {label} --format json --output {export}")
    send(repl, f"%events list {label}")
    repl_out.expect(r"^\d+ cursor=")
    events = json.loads(export.read_text())
    observations = [event["objectObservation"] for event in events]
    assert [item["kind"] for item in observations] == [
        "CREATED", *(["FIELD_CHANGED"] * 6), "DESTROY_BEGIN", "TEARDOWN_END"
    ], observations
    writes = observations[1:7]
    assert [(item["oldInteger"], item["newInteger"]) for item in writes] == [
        (value, value + 1) for value in range(1, 7)
    ], writes
    source = Path(__file__).resolve().parent / "fixtures" / "object_shared_writer" / "library.cpp"
    store_line = next(i for i, line in enumerate(source.read_text().splitlines(), 1)
                      if "SHARED_HELPER_STORE" in line)
    assert all(item["locationKind"] == "STORE" and item["field"] == "shared_writer::Record::value"
               and item["function"] == "shared_writer::adjust(shared_writer::Record&, int, bool)"
               and Path(item["file"]).name == "library.cpp" and item["line"] == store_line
               for item in writes), writes
    assert not any(item["kind"] == "COVERAGE_GAP" for item in observations)


def run(args):
    with tempfile.TemporaryDirectory(prefix="ct-object-shared-writer-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        endpoint = f"unix://{root / 'control.sock'}"
        _, server = start([args.server, "--listen", endpoint, "--agent", args.agent], root, cleanup)
        server.expect("gRPC server listening")
        target, app = start([args.fixture], root, cleanup)
        app.expect(r"^READY$")
        disassembly = subprocess.check_output([args.objdump, "-Cd", args.library], text=True)
        assert any("call" in line and "<shared_writer::adjust(" in line and "@plt>" in line
                   for line in disassembly.splitlines()), "fixture must retain an intra-DSO PLT call"
        repl, out = start([args.repl, "--endpoint", endpoint, "--attach", str(target.pid)],
                          root, cleanup)
        session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]

        common = ('--type shared_writer::Record --field value '
                  '--code "shared_writer::write(shared_writer::Record &, int, bool)" '
                  '--backend code-cache')
        send(repl, f"%object start {common}")
        expect_error(out, "writer is in shared module")
        assert repl.poll() is None, "the REPL should remain available for the next diagnostic case"
        send(repl, f'%object start {common} --module "{args.library}"')
        expect_error(out, "--follow-calls --module")
        assert repl.poll() is None, "the REPL should remain available for the next diagnostic case"

        capture(root, args, target, app, repl, out, session, endpoint, args.library, False, "live")
        send(repl, f'%object prepare shared_writer::Record --module "{args.library}" '
                   '--field value --code "shared_writer::write(shared_writer::Record &, int, bool)" '
                   f'--backend code-cache --follow-calls --output "{root / "invalid-plan"}"')
        expect_error(out, "names a shared ELF without a program interpreter")
        capture(root, args, target, app, repl, out, session, endpoint, args.library, True, "saved")

        send(repl, "%detach")
        out.expect(rf"^detached session={session}$")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 1, "expected command errors should set the piped REPL exit status"
        send(target, "q")
        assert target.wait(timeout=10) == 0


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture", "library"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("objdump")
    run(parser.parse_args())
