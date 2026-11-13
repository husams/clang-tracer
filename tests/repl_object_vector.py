"""Verify logical collection storage generations on a preserved GCC 8.5 Portfolio."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def run(args):
    preserved = ("portfolio", "portfolio.cpp", "portfolio.hpp")
    expected = {name: hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() for name in preserved}
    assert expected["portfolio"] == (args.fixture_dir / "portfolio.sha256").read_text().split()[0]
    notes = subprocess.check_output(["readelf", "--notes", args.fixture_dir / "portfolio"], text=True)
    build_id = re.search(r"Build ID: ([0-9a-f]+)", notes)
    with tempfile.TemporaryDirectory(prefix="ct-object-collection-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        for name in preserved:
            shutil.copy2(args.fixture_dir / name, root / name)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([root / "portfolio"])
        app.expect(r"^READY$")
        control, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        definition = ('--field quantity --vector legs --element-key id=12 --code "demo::Portfolio::rebalance()" '
                      '--mutation _ZNSt6vectorIN4demo3LegESaIS1_EE7reserveEm')
        if args.map:
            definition = ('--field exposure.limit --map risk --map-key AAPL --code "demo::Portfolio::rebalance()" '
                          '--mutation _ZN4demo12replace_riskERSt3mapINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_4RiskESt4lessIS6_ESaISt4pairIKS6_S7_EEE')
        if args.gap == "missing-key":
            definition = definition.replace("--map-key AAPL", "--map-key INVALID")
        elif args.gap == "read-budget":
            definition += " --max-elements 1"
        if args.backend == "code-cache":
            definition += " --backend code-cache --synchronization thread-confined"
        send(control, f"%object prepare demo::Portfolio {definition} --module ./portfolio --output ./vector-watch")
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        send(control, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        send(control, "%object preview --plan ./vector-watch")
        status = out.expect(r"^watch=0 active=no stores=(\d+) .*|^error:")
        assert status[1], status.string
        out.expect(r"^binding=same-build$")
        sites = []
        # The selected writer has more stores than the selected logical range.
        for _ in range(int(status[1])):
            store = out.expect(r'^site role="store" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
            sites.append((int(store[1], 16), bytes.fromhex(store[2])))
        token = maps(control, out)
        assert all(read_code(control, out, pc, token, len(code)) == code for pc, code in sites)
        send(control, "%object start --plan ./vector-watch --stack-depth 8")
        watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(control, out, watch, args.backend, target.pid, ())
        token = maps(control, out)
        if args.backend == "ptrace":
            assert all(read_code(control, out, pc, token, 1) == b"\xcc" for pc, _ in sites)
        else:
            assert redirects and all(read_code(control, out, pc, token, 1) != b"\xcc" for pc, _ in sites)
        count = 4 if args.gap else 7 if args.map else 6
        # Gap cases wait for the record deadline to detect unexpected extra rows.
        wanted = count + bool(args.gap)
        deadline = 3 if args.gap else 20
        send(control, f"%record collection.db --sqlite --probe {watch[1]} --name object.trace --count {wanted} --timeout {deadline}")
        (root / "rebalance.now").touch()
        out.expect(rf"^received={count} .*" + ("timed_out=true" if args.gap else "complete=true"))
        send(control, f"%object stop {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        token = maps(control, out)
        verify_restored_redirects(control, out, token, target.pid, redirects)
        assert all(read_code(control, out, pc, token, len(code)) == code for pc, code in sites)
        send(control, "%detach")
        out.expect(r"^detached session=")
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
        if args.map:
            before = app.expect(r"^before replace limit_addr=(0x[0-9a-f]+) limit=50$")
            after = app.expect(r"^after replace limit_addr=(0x[0-9a-f]+) limit=70$")
            write = app.expect(r"^after write limit_addr=(0x[0-9a-f]+) limit=80$")
        else:
            before = app.expect(r"^before reserve id=12 capacity=1 quantity_addr=(0x[0-9a-f]+) quantity=100$")
            after = app.expect(r"^after reserve id=12 capacity=2 quantity_addr=(0x[0-9a-f]+) quantity=100$")
            write = app.expect(r"^after write id=12 capacity=2 quantity_addr=(0x[0-9a-f]+) quantity=75$")
        assert before[1] != after[1] == write[1]
        send(control, "%events open collection.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(control, "%events export saved --format json --output collection.json")
        send(control, "%quit")
        assert control.wait(timeout=15) == 0, "".join(out.seen[-20:])
        events = json.loads((root / "collection.json").read_text())
        observations = [event["objectObservation"] for event in events]
        for name, digest in expected.items():
            assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
            assert hashlib.sha256((args.fixture_dir / name).read_bytes()).hexdigest() == digest
        if args.gap:
            assert [item["kind"] for item in observations] == [
                "FIRST_SEEN", "COVERAGE_GAP", "COVERAGE_GAP", "OBSERVATION_ENDED"], observations
            assert all("oldInteger" not in item and "newInteger" not in item for item in observations)
            print(f"backend={args.backend} {args.gap} withheld field values and restored all sites")
            return
        kinds = [
            "FIRST_SEEN", "RANGE_BOUND", "RANGE_SUSPENDED", "RANGE_BOUND",
            "FIELD_CHANGED", "OBSERVATION_ENDED"]
        if args.map:
            kinds.insert(2, "FIELD_CHANGED")
        assert [item["kind"] for item in observations] == kinds, observations
        assert len({event["objectAddress"] for event in events}) == 1
        assert len({item["lifetimeId"] for item in observations}) == 1
        assert observations[-1]["closureReason"] == "METHOD_RETURN"
        first_generation_rows = 3 if args.map else 2
        path = 'demo::Portfolio::risk["AAPL"].exposure.limit' if args.map else "demo::Portfolio::legs[id=12].quantity"
        addresses = [before[1]] * first_generation_rows + [after[1]] * 2
        generations = [1] * first_generation_rows + [2, 2]
        for item, address, generation in zip(observations[1:-1], addresses, generations):
            assert item["destination"] == str(int(address, 16)), item
            assert item["storageGeneration"] == str(generation), item
            assert item["field"] == path, item
        writes = [(2, 50, 60, "before = 60;"), (5, 70, 80, "after = 80;")] if args.map else [
            (4, 100, 75, "legs.at(0).quantity = 75;")]
        source = (root / "portfolio.cpp").read_text().splitlines()
        for position, old, new, statement in writes:
            change = observations[position]
            assert (change["oldInteger"], change["newInteger"]) == (old, new), change
            if build_id:
                assert change["moduleBuildId"] == build_id[1], change
            else:
                assert change["moduleSha256"] == expected["portfolio"], change
            assert change["locationKind"] == "STORE", change
            source_line = next(index for index, line in enumerate(source, 1) if statement in line)
            assert change["line"] == source_line and change["file"].endswith("portfolio.cpp"), change
            stack = events[position]["stack"]
            assert stack["status"] == "PARTIAL" and stack["frames"][0]["pc"] == change["pc"], change
            print(f"captured {old} -> {new} at line {source_line}")
        print(f"verified unchanged Portfolio sha256={expected['portfolio']}")
        print(f"generation 1 {before[1]} -> suspended -> generation 2 {after[1]}")
        print(f"backend={args.backend} restored {len(sites)} store sites and {len(redirects)} redirects")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture_dir"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="ptrace")
    parser.add_argument("--map", action="store_true", help="verify map node replacement and nested field writes")
    parser.add_argument("--gap", choices=("missing-key", "read-budget"))
    args = parser.parse_args()
    if args.gap and not args.map:
        parser.error("--gap requires --map")
    run(args)


if __name__ == "__main__":
    main()
