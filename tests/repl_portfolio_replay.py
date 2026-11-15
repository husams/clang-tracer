"""Replay an independently sampled Portfolio rebalance on a preserved ELF."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, send, stop


def object_watch(args, root, target, control, out):
    selector = 'demo::Portfolio::rebalance()'
    send(control, f'%object prepare demo::Portfolio --on "{selector}" '
                  "--synchronization thread-confined "
                  "--field legs --field risk --field active_snapshot --key legs=id --key risk=key "
                  "--include ./portfolio.hpp --policy ./snapshot-policy.hpp "
                  "--compdb ./compile_commands.json --translation-unit ./portfolio.cpp "
                  "--module ./portfolio --output ./portfolio-watch --emit-source ./generated")
    prepared = out.expect(r"^object package=|^error:")
    assert prepared.string.startswith("object package="), prepared.string
    assert (root / "generated" / "method-1.hpp").stat().st_size > 0
    send(control, f"%attach {target.pid}")
    out.expect(rf"^attached session=\S+ pid={target.pid}$")
    module = " --module portfolio-next" if args.compatible_rebuild else ""
    send(control, f"%object preview --plan ./portfolio-watch{module}")
    preview = out.expect(r"^watch=0 active=no .*backend=method-detour |^error:")
    if args.reject_rebuild:
        assert "compatible method rebinding requires a verified receiver and callable ABI" in preview.string
        return
    assert preview.string.startswith("watch="), preview.string
    binding = "compatible-rebuild" if args.compatible_rebuild else "same-build"
    out.expect(rf"^binding={binding}$")
    site = out.expect(r'^site role="method" elf=(0x[0-9a-f]+) pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
    pc, original = int(site[2], 16), bytes.fromhex(site[3])

    def live_bytes(address, size):
        if platform.machine() == "aarch64":
            with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
                memory.seek(address)
                return memory.read(size)
        token = maps(control, out)
        return b"".join(read_code(control, out, address + offset, token, min(16, size - offset))
                        for offset in range(0, size, 16))

    def entry_bytes():
        return live_bytes(pc, len(original))

    assert entry_bytes() == original
    stack = "" if args.no_stack else " --stack-depth 8"
    budget = " --max-snapshot-bytes 8" if args.truncated else ""
    limit = " --max-events 1" if args.event_limit else ""
    send(control, f"%object start --plan ./portfolio-watch{module}{stack}{budget}{limit}")
    watch = out.expect(r"^watch=(\d+) active=yes .*backend=method-detour |^error:")
    assert watch[1], watch.string
    probe = out.expect(r"^method_probe=(\d+) active=yes restored=no$")
    assert entry_bytes() != original
    redirects = []
    if platform.machine() == "aarch64":
        width = int(watch.string.split("redirects=", 1)[1].split()[0])
        for _ in range(width):
            row = out.expect(r"^redirect=(0x[0-9a-f]+) translated=0x[0-9a-f]+ original=([0-9a-f]+) installed=([0-9a-f]+)$")
            address = int(row[1], 16)
            before, armed = bytes.fromhex(row[2]), bytes.fromhex(row[3])
            assert before != armed and live_bytes(address, len(armed)) == armed
            redirects.append((address, before))
        assert redirects and any(address == pc for address, _ in redirects)
    count = 1 if args.truncated or args.event_limit else 3
    send(control, f"%record portfolio.db --sqlite --name object.trace --probe {watch[1]} --count {count} --timeout 20")
    (root / "rebalance.now").touch()
    out.expect(rf"^received={count} .*complete=true")
    send(control, f"%object status {watch[1]}")
    status = out.expect(rf"^watch={watch[1]} active=yes .*observations={count} .*dropped=0 .*coverage_gaps={int(args.truncated)} ")
    if args.event_limit:
        assert " capture_limit=max-events " in status.string
    send(control, f"%object stop {watch[1]}")
    out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
    out.expect(rf"^method_probe={probe[1]} active=no restored=yes$")
    assert entry_bytes() == original
    assert all(live_bytes(address, len(before)) == before for address, before in redirects)
    send(control, "%detach")
    out.expect(r"^detached session=")
    send(control, "%events open portfolio.db --as saved")
    out.expect(rf"^opened events={count} retained={count} evicted=0$")
    send(control, "%events export saved --format json --output snapshots.json")
    send(control, "%quit")
    assert control.wait(timeout=20) == 0, "".join(out.seen[-20:])
    changes = set()
    invocations = set()
    events = json.loads((root / "snapshots.json").read_text())
    if args.truncated:
        assert len(events) == 1
        gap = events[0]["objectObservation"]
        assert gap["kind"] == "COVERAGE_GAP" and "exceeds capture budget" in gap["diagnostic"], gap
        print("bounded snapshot emitted one coverage gap and withheld incomplete deltas")
        return
    for event in events:
        assert event["probeId"] == watch[1] and event["functionName"] == "object.trace", event
        invocations.add((event["objectAddress"], event["callId"], event["tid"]))
        sample = event["objectObservation"]
        assert sample["kind"] == "SNAPSHOT_DELTA" and sample["locationKind"] == "CALL_BOUNDARY", sample
        assert sample["synchronization"] == "THREAD_CONFINED", sample
        assert sample["boundaryProbeId"] == probe[1] and sample["pc"] == str(pc), sample
        assert str(sample["snapshotBefore"]["signedValue"]) + " -> " + str(sample["snapshotAfter"]["signedValue"]) == sample["diagnostic"], sample
        assert sample["elfPc"] == str(int(site[1], 16)), sample
        mapped = root / ("portfolio-next" if args.compatible_rebuild else "portfolio")
        assert sample["moduleSha256"] == hashlib.sha256(mapped.read_bytes()).hexdigest(), sample
        assert sample["line"] and sample["file"].endswith("/portfolio.cpp"), sample
        assert args.no_stack == ("stack" not in event), event
        if not args.no_stack:
            assert event["stack"]["status"] == "COMPLETE" and event["stack"]["frames"][0]["pc"] == str(pc), event
        changes.add((sample["field"], sample["diagnostic"]))
    assert len(invocations) == 1, invocations
    expected = {("legs[id=12].quantity", "100 -> 75"), ('risk["AAPL"].limit', "50 -> 60"),
                ("active_snapshot.visible", "30 -> 25")}
    assert (changes < expected if args.event_limit else changes == expected), changes
    print(f"object watch {watch[1]} restored entry pc={pc:#x}, stack={not args.no_stack}, recorded {count} deltas")
    for field, delta in sorted(changes):
        print(f"snapshot_delta {field} {delta}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--object-watch", action="store_true")
    parser.add_argument("--no-stack", action="store_true")
    parser.add_argument("--stale-layout", action="store_true")
    capture = parser.add_mutually_exclusive_group()
    capture.add_argument("--truncated", action="store_true")
    capture.add_argument("--event-limit", action="store_true")
    parser.add_argument("--compatible-rebuild", type=lambda value: Path(value).resolve())
    parser.add_argument("--reject-rebuild", action="store_true")
    args = parser.parse_args()
    if args.reject_rebuild and not (args.object_watch and args.compatible_rebuild):
        parser.error("--reject-rebuild requires --object-watch and --compatible-rebuild")
    preserved = ("portfolio", "portfolio.cpp", "portfolio.hpp", "snapshot-policy.hpp")
    with tempfile.TemporaryDirectory(prefix="ct-portfolio-pair-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        for name in preserved:
            shutil.copy2(args.fixture_dir / name, root / name)
        hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in preserved}
        expected = (args.fixture_dir / "portfolio.sha256").read_text().split()[0]
        assert hashes["portfolio"] == expected
        compdb = (args.fixture_dir / "compile_commands.json").read_text()
        compdb = compdb.replace(str(args.fixture_dir), str(root))
        (root / "compile_commands.json").write_text(compdb)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        if args.stale_layout:
            original = (root / "portfolio.hpp").read_text()
            stale = original.replace("unsigned id; int quantity;", "int quantity; unsigned id;")
            assert stale != original
            (root / "stale.hpp").write_text(stale)
            control, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
            send(control, '%object prepare demo::Portfolio --on "demo::Portfolio::rebalance()" '
                          '--synchronization thread-confined '
                          '--include ./stale.hpp --policy ./snapshot-policy.hpp --compdb ./compile_commands.json '
                          '--translation-unit ./portfolio.cpp --module ./portfolio --output ./rejected')
            out.expect(r"^error: .*source layout differs or is unverified for demo::Leg::id")
            assert control.wait(timeout=10) != 0
            assert not (root / "rejected").exists()
            assert hashes == {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in preserved}
            print("stale same-size field reorder rejected before any process was attached")
            return

        target_path = root / "portfolio"
        if args.compatible_rebuild:
            target_path = root / "portfolio-next"
            shutil.copy2(args.compatible_rebuild, target_path)
            rebuilt_hash = hashlib.sha256(target_path.read_bytes()).hexdigest()
        target, app = start([target_path])
        app.expect(r"^READY$")
        if args.object_watch:
            control, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
            object_watch(args, root, target, control, out)
            if args.reject_rebuild:
                assert control.wait(timeout=20) != 0
                (root / "rebalance.now").touch()
            (root / "stop.now").touch()
            assert target.wait(timeout=10) == 0
            assert hashes == {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in preserved}
            print(f"verified unchanged prebuilt Portfolio sha256={hashes['portfolio']}")
            if args.compatible_rebuild:
                assert rebuilt_hash == hashlib.sha256(target_path.read_bytes()).hexdigest()
                assert rebuilt_hash == hashlib.sha256(args.compatible_rebuild.read_bytes()).hexdigest()
                outcome = "rejected incompatible" if args.reject_rebuild else "verified compatible"
                print(f"{outcome} unchanged PIE rebuild sha256={rebuilt_hash}")
            return
        control, out = start([args.repl, "--server", args.server, "--agent", args.agent,
                              "--attach", str(target.pid), "--fail-fast"])
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        selector = '"demo::Portfolio::rebalance()"'
        send(control, f"%probe prepare {selector} --include ./portfolio.hpp "
                      "--include ./snapshot-policy.hpp --compdb ./compile_commands.json "
                      "--translation-unit ./portfolio.cpp --module ./portfolio --output ./prepared-rebalance")
        prepared = out.expect(r"^prepared directory=|^error:")
        assert prepared.string.startswith("prepared directory="), prepared.string
        send(control, f"%probe inject {selector} --prepared ./prepared-rebalance "
                      "--prepared-mode require --stack-depth 8 --snapshot-receiver-at-return")
        installed = out.expect(r"^installed probe=(\d+).*|^error:")
        assert installed[1], installed.string
        send(control, f"%record portfolio.db --sqlite --probe {installed[1]} --count 2 --timeout 20")
        (root / "rebalance.now").touch()
        out.expect(r"^received=2 .*complete=true")
        send(control, "%detach")
        out.expect(r"^detached session=")
        send(control, "%events open portfolio.db --as saved")
        out.expect(r"^opened events=2 retained=2 evicted=0$")
        send(control, "%events diff saved --pre 0 --post 1 --key legs=id --key risk=key")
        out.expect(r'^snapshot_window call=\d+ function="demo::Portfolio::rebalance\(\)"$')
        expected_deltas = {
            'snapshot_delta active_snapshot.visible 30 -> 25',
            'snapshot_delta legs[id=12].quantity 100 -> 75',
            'snapshot_delta risk["AAPL"].limit 50 -> 60',
        }
        actual = {out.expect(r"^snapshot_delta ").string.strip() for _ in expected_deltas}
        assert actual == expected_deltas, actual
        out.expect(r"^snapshot_changes=3 gaps=0$")
        send(control, "%events stack saved --index 0")
        out.expect(r"^stack COMPLETE ")
        send(control, "%events export saved --format json --output snapshots.json")
        send(control, "%quit")
        assert control.wait(timeout=20) == 0, "".join(out.seen[-20:])
        before, after = json.loads((root / "snapshots.json").read_text())
        assert before["phase"] == "PRE" and after["phase"] == "POST"
        assert before["callId"] == after["callId"]
        assert before["objectAddress"] == after["objectAddress"]
        assert after["receiverSampledAtReturn"] and not before.get("receiverSampledAtReturn")
        assert before["receiverSnapshot"]["payload"] != after["receiverSnapshot"]["payload"]
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        assert hashes == {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in preserved}
        print(f"verified prebuilt Portfolio sha256={hashes['portfolio']} call={before['callId']} "
              "method=demo::Portfolio::rebalance() stack=COMPLETE exit=0")
        for delta in sorted(actual):
            print(delta)


if __name__ == "__main__":
    main()
