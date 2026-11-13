"""Exercise constructor overloads, delegation, unwind, and ambiguous Itanium aliases."""

import argparse
from collections import defaultdict
from contextlib import ExitStack
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_redirects, send, stop, verify_restored_redirects


def run(args):
    with tempfile.TemporaryDirectory(prefix="ct-object-lifetime-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, cwd=root)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 25)

        target, app = start([args.fixture])
        app.expect(r"^READY$")
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        live_read = None
        if platform.machine() == "aarch64":
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))
            live_read = lambda pc, size: os.pread(memory.fileno(), size, pc)

        def verify_restored(redirects):
            token = maps(repl, out) if redirects and live_read is None else None
            verify_restored_redirects(repl, out, token, target.pid, redirects, live_read=live_read)

        options = f"--backend {args.backend}" + (" --no-values" if args.no_values else "")
        definition = '--type life::Record --field value_ --code "life::Record::write(int)" --stack-depth 8 ' + options
        send(repl, f"%object preview {definition}")
        out.expect(r"^watch=0 active=no stores=[1-9][0-9]* ")
        for _ in range(4):
            out.expect(r'^site role="construct" ')
        send(repl, f"%object start {definition}")
        watch = out.expect(r"^watch=(\d+) active=yes stores=[1-9][0-9]* .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, args.backend, target.pid, (), live_read=live_read)
        send(repl, f"%record variants.db --sqlite --probe {watch[1]} --count 25 --timeout 20")
        send(target, "g")
        app.expect(r"^caught$")
        app.expect(r"^variants done$")
        out.expect(r"^received=25 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        verify_restored(redirects)
        send(repl, "%events open variants.db --as variants")
        out.expect(r"^opened events=25 retained=25 evicted=0$")
        send(repl, "%events export variants --format json --output variants.json")
        send(repl, "%events show variants --index 0")
        out.expect(r"^object_observation \{$")
        variants = json.loads((root / "variants.json").read_text())
        timeline = defaultdict(list)
        abort = None
        for event in variants:
            item = event["objectObservation"]
            if item["kind"] == "CONSTRUCTION_ABORTED":
                assert abort is None
                abort = event
                continue
            timeline[item["lifetimeId"]].append((event, item))
        assert abort is not None
        assert abort["objectObservation"]["closureReason"] == "UNWOUND"
        assert not abort["objectObservation"].get("lifetimeId") and abort["objectAddress"]
        assert abort["stack"]["status"] in ("PARTIAL", "TRUNCATED")
        assert len(timeline) == 6, timeline
        transitions = set()
        addresses = defaultdict(set)
        write_kind = "FIELD_WRITTEN" if args.no_values else "FIELD_CHANGED"
        for life, series in timeline.items():
            assert [item["kind"] for _, item in series] == [
                "CREATED", write_kind, "DESTROY_BEGIN", "TEARDOWN_END"], series
            before, write, _, end = [item for _, item in series]
            assert end["closureReason"] == "DESTRUCTOR_RETURN"
            if args.no_values:
                assert all(not {"oldInteger", "newInteger"}.intersection(item) for _, item in series)
            else:
                assert before["newInteger"] == write["oldInteger"]
                transitions.add((write["oldInteger"], write["newInteger"]))
            addresses[series[0][0]["objectAddress"]].add(int(before["generation"]))
            assert series[0][0]["stack"]["frames"][0]["pc"] == before["pc"]
        if not args.no_values:
            assert transitions == {(8, 10), (10, 20), (20, 30), (40, 41), (50, 51), (60, 61)}, transitions
        assert sum(generations == {1, 2} for generations in addresses.values()) == 1, addresses

        if args.check_reuse:
            send(repl, f"%object start {definition}")
            watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            assert watch[1], watch.string
            redirects = read_redirects(repl, out, watch, args.backend, target.pid, (), live_read=live_read)
            send(repl, f"%record reuse.db --sqlite --probe {watch[1]} --count 7 --timeout 20")
            send(target, "r")
            app.expect(r"^reuse done$")
            out.expect(r"^received=7 .*complete=true")
            if args.backend == "code-cache":
                target.send_signal(signal.SIGSTOP)
                _, status = os.waitpid(target.pid, os.WUNTRACED)
                assert os.WIFSTOPPED(status)
            send(repl, "%object stop")
            out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
            verify_restored(redirects)
            if args.backend == "code-cache":
                for task in Path(f"/proc/{target.pid}/task").iterdir():
                    state = next(line for line in (task / "status").read_text().splitlines()
                                 if line.startswith("State:"))
                    assert state.split()[1] == "T", state
                target.send_signal(signal.SIGCONT)
            send(repl, "%events open reuse.db --as reuse")
            out.expect(r"^opened events=7 retained=7 evicted=0$")
            send(repl, "%events export reuse --format json --output reuse.json")
            send(repl, "%events list reuse")
            out.expect(r"^6 ")
            records = [event["objectObservation"] for event in json.loads((root / "reuse.json").read_text())]
            assert [event["kind"] for event in records] == [
                "CREATED", write_kind, "OBSERVATION_ENDED", "CREATED", write_kind,
                "DESTROY_BEGIN", "TEARDOWN_END"], records
            assert records[2]["closureReason"] == "SCOPE_CHANGED"
            assert records[0]["lifetimeId"] == records[1]["lifetimeId"] == records[2]["lifetimeId"]
            assert records[0]["lifetimeId"] != records[3]["lifetimeId"] == records[4]["lifetimeId"]
            assert int(records[3]["generation"]) == int(records[0]["generation"]) + 1

        alias = '--type life::Base --field value_ --code "life::Base::write(int)" ' + options
        send(repl, f"%object preview {alias}")
        out.expect(r'^site role="construct \(complete/base alias\)" ')
        send(repl, f"%object start {alias}")
        watch = out.expect(r"^watch=(\d+) active=yes stores=[1-9][0-9]* .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, args.backend, target.pid, (), live_read=live_read)
        send(repl, f"%record alias.db --sqlite --probe {watch[1]} --count 1 --timeout 15")
        send(target, "b")
        app.expect(r"^alias done$")
        out.expect(r"^received=1 .*complete=true")
        send(repl, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*observations=1 .*restoration_pending=no ")
        verify_restored(redirects)
        send(repl, "%events open alias.db --as alias")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        send(repl, "%events show alias --index 0")
        out.expect(r"^  kind: LIFETIME_UNCERTAIN$")
        out.expect(r'^  diagnostic: "complete and base constructors share the same entry"$')
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(target, "q")
        assert target.wait(timeout=10) == 0
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        print(f"backend={args.backend} values={not args.no_values} six lifetime timelines, "
              "one constructor unwind, one uncertain alias; restored both watches")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    parser.add_argument("--no-values", action="store_true")
    parser.add_argument("--check-reuse", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
