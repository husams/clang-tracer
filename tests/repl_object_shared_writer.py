"""Exercise a same-DSO PLT call while watching a field through the native REPL."""

from contextlib import ExitStack
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, send, stop


def start(argv, cwd, cleanup, timeout=30, env=None):
    process = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, env=env)
    cleanup.callback(stop, process)
    return process, Output(process.stdout, timeout)


def expect_error(out, pattern):
    row = out.expect(r"^error: (.*)$")
    assert pattern in row[1], row.string


def capture(root, args, app, app_out, repl, repl_out, label, *, saved=False, lazy_first=False):
    module = args.library
    if saved:
        package = root / f"{label}-plan"
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
    send(repl, f"%record {database} --sqlite --name object.trace --probe {probe} --count 9 --timeout 10")
    send(app, "g")
    app_out.expect(r"^DONE value=7$")
    recording = repl_out.expect(r"^received=\d+ .*|^error:")
    assert recording.string.startswith("received=9 ") and "complete=true" in recording.string, recording.string
    send(repl, f"%object status {probe}")
    status = repl_out.expect(rf"^watch={probe} active=yes .*$")
    if lazy_first:
        assert "executed_stores=5 " in status.string, status.string
        assert "coverage_gaps=1 " in status.string and "runtime_gap_hits=1 " in status.string, status.string
    else:
        assert "executed_stores=6 " in status.string and "coverage_gaps=" not in status.string, status.string
    send(repl, f"%object stop {probe}")
    repl_out.expect(rf"^watch={probe} active=no .*restoration_pending=no .*")
    send(repl, f"%events open {database} --as {label}")
    repl_out.expect(r"^opened events=9 retained=9 evicted=0$")
    send(repl, f"%events export {label} --format json --output {export}")
    send(repl, f"%events list {label}")
    repl_out.expect(r"^\d+ cursor=")
    events = json.loads(export.read_text())
    observations = [event["objectObservation"] for event in events]
    writes = [item for item in observations if item["kind"] == "FIELD_CHANGED"]
    gaps = [item for item in observations if item["kind"] == "COVERAGE_GAP"]
    expected_values = range(2, 7) if lazy_first else range(1, 7)
    assert [(item["oldInteger"], item["newInteger"]) for item in writes] == [
        (value, value + 1) for value in expected_values
    ], writes
    assert len(gaps) == (1 if lazy_first else 0), gaps
    if gaps:
        assert gaps[0]["diagnostic"] == "callee ran natively while the PLT slot was lazily bound", gaps[0]
    source_name = "main.cpp" if args.scenario == "interposed" else "library.cpp"
    marker = "EXECUTABLE_HELPER_STORE" if args.scenario == "interposed" else "SHARED_HELPER_STORE"
    expected_module = args.fixture if args.scenario == "interposed" else args.library
    source = Path(__file__).resolve().parent / "fixtures" / "object_shared_writer" / source_name
    store_line = next(i for i, line in enumerate(source.read_text().splitlines(), 1)
                      if marker in line)
    assert all(item["locationKind"] == "STORE" and item["field"] == "shared_writer::Record::value"
               and item["function"] == "shared_writer::adjust(shared_writer::Record&, int, bool)"
               and Path(item["module"]) == expected_module
               and Path(item["file"]).name == source_name and item["line"] == store_line
               for item in writes), writes
    assert [item["kind"] for item in observations].count("CREATED") == 1, observations
    assert [item["kind"] for item in observations].count("DESTROY_BEGIN") == 1, observations
    assert [item["kind"] for item in observations].count("TEARDOWN_END") == 1, observations


def run(args):
    with tempfile.TemporaryDirectory(prefix="ct-object-shared-writer-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        endpoint = f"unix://{root / 'control.sock'}"
        _, server = start([args.server, "--listen", endpoint, "--agent", args.agent], root, cleanup)
        server.expect("gRPC server listening")
        import os
        target_env = dict(os.environ)
        target_env.pop("LD_BIND_NOW", None)
        if args.scenario in {"eager", "interposed"}:
            target_env["LD_BIND_NOW"] = "1"
        target, app = start([args.fixture], root, cleanup, env=target_env)
        app.expect(r"^READY$")
        disassembly = subprocess.check_output([args.objdump, "-Cd", args.library], text=True)
        assert any("call" in line and ("<shared_writer::adjust(" in line or
                                       "<_ZN13shared_writer6adjustE" in line) and "@plt>" in line
                   for line in disassembly.splitlines()), "fixture must retain an intra-DSO PLT call"
        if args.scenario == "interposed":
            symbols = subprocess.check_output([args.nm, "-D", "-C", args.fixture], text=True)
            assert "shared_writer::adjust(shared_writer::Record&, int, bool)" in symbols, \
                "executable must export the interposing helper"
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
        send(repl, f'%object start {common} --module "{args.library}" --writer-entry')
        expect_error(out, "--follow-calls is unavailable for this watch scope")

        capture(root, args, target, app, repl, out, args.scenario,
                lazy_first=args.scenario == "lazy")
        send(repl, f'%object prepare shared_writer::Record --module "{args.library}" '
                   '--field value --code "shared_writer::write(shared_writer::Record &, int, bool)" '
                   f'--backend code-cache --follow-calls --output "{root / "invalid-plan"}"')
        expect_error(out, "names a shared ELF without a program interpreter")
        capture(root, args, target, app, repl, out, "saved", saved=True)

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
    parser.add_argument("nm")
    parser.add_argument("--scenario", choices=("lazy", "eager", "interposed"), default="lazy")
    run(parser.parse_args())
