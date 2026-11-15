"""Exercise lifecycle and runtime call redirects in the same writer scope."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, read_redirects, send, stop


def run(args):
    fixture = Path(args.fixture).resolve()
    sources = [fixture, *fixture.parent.glob("*.cpp"), *fixture.parent.glob("*.hpp")]
    digest = {path: hashlib.sha256(path.read_bytes()).digest() for path in sources}
    with tempfile.TemporaryDirectory(prefix="ct-object-follow-lifetime-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def launch(*argv):
            process = subprocess.Popen(argv, cwd=root, text=True, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 25)

        target, app = launch(fixture)
        app.expect(r"^READY$")
        repl, out = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))
        def read(pc, size):
            return os.pread(memory.fileno(), size, pc)

        options = "--type demo::Order --field quantity --backend code-cache --code main --follow-calls"
        if args.no_values:
            options += " --no-values"
        send(repl, f"%object preview {options}")
        out.expect(r"^watch=0 active=no stores=\d+ ")
        send(repl, f"%object start {options}")
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, "code-cache", target.pid, (),
                                   live_read=read, runtime_calls=True)
        send(repl, f"%record order.db --sqlite --probe {watch[1]} --count 10000 --timeout 3")
        # Arming a broad scope can generate coverage gaps in the waiting loop.
        # Record the whole interval and assert the watched events independently.
        deadline = time.monotonic() + 2
        while not (root / "order.db").exists() and time.monotonic() < deadline:
            time.sleep(0.005)
        assert (root / "order.db").exists(), out.seen
        (root / "go.now").touch()
        app.expect(r"^DONE$")
        count = int(out.expect(r"^received=(\d+) .*timed_out=true")[1])
        send(repl, "%detach")
        out.expect(r"^detached session=")
        for pc, (original, _, _) in redirects.items():
            assert read(pc, len(original)) == original
        send(repl, "%events open order.db --as order")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export order --format json --output order.json")
        send(repl, "%quit")
        assert repl.wait(timeout=15) == 0
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0

        events = [event["objectObservation"] for event in json.loads((root / "order.json").read_text())]
        timeline = [item for item in events if item.get("lifetimeId")]
        written = "FIELD_WRITTEN" if args.no_values else "FIELD_CHANGED"
        assert [item["kind"] for item in timeline] == [
            "CREATED", written, written, "DESTROY_BEGIN", "TEARDOWN_END", "CREATED",
            *([written] if args.no_values else []), "DESTROY_BEGIN", "TEARDOWN_END"], timeline
        assert len({item["lifetimeId"] for item in timeline[:5]}) == 1
        assert len({item["lifetimeId"] for item in timeline[5:]}) == 1
        assert timeline[0]["lifetimeId"] != timeline[5]["lifetimeId"]
        writes = [item for item in timeline if item["kind"] == written]
        assert "set_quantity" in writes[0]["function"] and "apply_fill" in writes[1]["function"], writes
        fixture_source = Path(__file__).resolve().parent / "fixtures" / "object_lifetime"
        for item in writes:
            name = Path(item["file"]).name
            assert name in ("follow_order.cpp", "order.cpp"), item
            source = fixture_source / name if name == "follow_order.cpp" else fixture.parent / name
            line = source.read_text().splitlines()[item["line"] - 1]
            assert ("quantity = next" if "set_quantity" in item["function"] else "quantity -= amount") in line, item
            assert int(item["elfPc"]) > 0 and int(item["pc"]) > 0 and item["locationKind"] == "STORE", item
        if args.no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in timeline)
        else:
            assert [(item["oldInteger"], item["newInteger"]) for item in writes] == [(10, 9), (9, 7)]
    assert all(hashlib.sha256(path.read_bytes()).digest() == expected for path, expected in digest.items())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name)
    parser.add_argument("--no-values", action="store_true")
    run(parser.parse_args())
