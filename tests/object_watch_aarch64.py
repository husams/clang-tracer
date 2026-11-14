"""Exercise both AArch64 capture backends against a preserved ELF."""

import argparse
from base64 import b64decode
from collections import defaultdict
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import tempfile
import time


def receive(process, timeout=20):
    deadline = time.monotonic() + timeout
    line = bytearray()
    while select.select([process.stdout], [], [], max(0, deadline - time.monotonic()))[0]:
        byte = os.read(process.stdout.fileno(), 1)
        if not byte:
            raise AssertionError(f"process {process.pid} exited with status {process.poll()}")
        if byte == b"\n":
            return line.decode()
        line += byte
    raise AssertionError(f"process {process.pid} produced no complete line")


def start_process(cleanup, cwd, argv):
    child = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def stop():
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=4)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()

    cleanup.callback(stop)
    return child


def preview(binary, pid, request, agent):
    report = json.loads(subprocess.check_output([binary, "preview", str(pid), request], text=True))
    return report["plan"] if agent else report


def start_watch(cleanup, root, binary, target, request, agent, live, originals, stores=1):
    command = [binary, "watch", str(target.pid), request]
    if agent:
        command.extend([agent, "arm_rendezvous"])
    trace = start_process(cleanup, root, command)
    armed = json.loads(receive(trace))
    assert armed["active"] and armed["storeSites"] == stores, armed
    if not agent:
        assert all(live(pc, 4) == bytes.fromhex("000020d4") for pc in originals)
        return trace
    status = Path(f"/proc/{target.pid}/status").read_text()
    assert next(line for line in status.splitlines() if line.startswith("TracerPid:")).split()[1] == "0"
    for redirect in armed["redirects"]:
        pc = int(redirect["pc"])
        installed = b64decode(redirect["installed"])
        assert int.from_bytes(installed, "little") >> 26 == 5, redirect
        assert live(pc, 4) == installed, redirect
        assert any(before[pc - start:pc - start + 4] == b64decode(redirect["original"])
                   for start, before in originals.items() if start <= pc < start + len(before)), redirect
    return trace


def run(binary, fixture, no_values, agent):
    with tempfile.TemporaryDirectory(prefix="ct-aarch64-object-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        target = start_process(cleanup, root, [fixture])
        assert receive(target) == "READY"
        request = root / "definition.json"
        request.write_text(json.dumps({
            "concreteType": "tagged_test::State",
            "field": "payload.integer",
            "codeSymbol": "_ZN11tagged_test10write_bitsERNS_5StateEj",
            "unionTag": "kind",
            "unionCases": [{"tag": 1, "member": "integer"}, {"tag": 2, "member": "real"}],
            "synchronization": "THREAD_CONFINED",
            "noValues": no_values,
            "stackDepth": 5,
            "backend": "CODE_CACHE" if agent else "PTRACE",
        }))
        plan = preview(binary, target.pid, request, agent)
        assert plan["fieldEncoding"] == "tagged union" and plan["fieldOffset"] == "4", plan
        code = {int(binding["runtimePc"]): b64decode(binding["original"])
                for binding in plan["bindings"] if binding["role"] == "store"}
        windows = {int(binding["runtimePc"]): len(b64decode(binding["original"]))
                   for binding in plan["bindings"] if binding["role"] in ("construct", "destroy")}
        assert len(code) == 1 and all(len(original) == 4 for original in code.values()), code
        with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
            def live(pc, length):
                memory.seek(pc)
                return memory.read(length)

            assert all(live(pc, len(original)) == original for pc, original in code.items())
            anchors = {pc: live(pc, size) for pc, size in windows.items()}
            assert anchors
            trace = start_watch(cleanup, root, binary, target, request, agent, live, {**code, **anchors})
            (root / "write.now").touch()
            assert receive(target) == "DONE integer=23 real=2.5 raw=01020304"
            trace.send_signal(signal.SIGINT)
            output, _ = trace.communicate(timeout=20)
            assert trace.returncode == 0, output
            rows = [json.loads(line) for line in output.splitlines()]
            assert all(live(pc, len(original)) == original for pc, original in code.items())
            assert all(live(pc, len(original)) == original for pc, original in anchors.items())
        assert not rows[-1].get("active", False) and not rows[-1].get("restorationPending", False), rows[-1]
        writes = [event for event in rows if event.get("objectObservation", {}).get("locationKind") == "STORE"]
        assert len(writes) == 3, rows
        for event in writes:
            obs = event["objectObservation"]
            assert obs["kind"] == ("FIELD_WRITTEN" if no_values else "FIELD_CHANGED"), obs
            assert obs["pc"] == str(next(iter(code))) and obs["generation"] == "1", obs
            assert obs["moduleBuildId"] == plan["executableBuildId"] and obs["line"], obs
            assert event["stack"]["frames"][0]["pc"] == obs["pc"]
        observations = [event["objectObservation"] for event in writes]
        if no_values:
            assert all(not any(field in item for field in
                               ("oldUnionTag", "newUnionTag", "snapshotBefore", "snapshotAfter",
                                "oldBytes", "newBytes")) for item in observations), observations
        else:
            assert [item["oldUnionTag"] for item in observations] == [1, 2, 3], observations
            assert observations[0]["snapshotBefore"]["signedValue"] == "-2"
            assert observations[0]["snapshotAfter"]["signedValue"] == "23"
            assert observations[1]["snapshotBefore"]["realValue"] == 1.5
            assert observations[1]["snapshotAfter"]["realValue"] == 2.5
            assert b64decode(observations[2]["oldBytes"]) == bytes.fromhex("78563412")
            assert b64decode(observations[2]["newBytes"]) == bytes.fromhex("04030201")
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} values={not no_values} "
          "three writes, original PCs and restored bytes")


def run_fault(binary, fixture, mode, no_values, agent):
    with tempfile.TemporaryDirectory(prefix="ct-aarch64-fault-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = start_process(cleanup, root, [fixture, mode])
        assert receive(target) == "READY"
        request = root / "definition.json"
        request.write_text(json.dumps({
            "concreteType": "arm_test::Record", "field": "value", "codeSymbol": "arm_write_value",
            "noValues": no_values,
            "backend": "CODE_CACHE" if agent else "PTRACE",
        }))
        plan = preview(binary, target.pid, request, agent)
        windows = {int(site["runtimePc"]): len(b64decode(site["original"])) for site in plan["bindings"]
                   if site["role"] in ("construct", "destroy", "store")}
        [store] = [int(site["runtimePc"]) for site in plan["bindings"] if site["role"] == "store"]
        with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
            def live(pc, length):
                memory.seek(pc)
                return memory.read(length)

            original = {pc: live(pc, size) for pc, size in windows.items()}
            trace = start_watch(cleanup, root, binary, target, request, agent, live, original)
            (root / "write.now").touch()
            if mode == "paced":
                assert receive(target) == "STARTED"
                time.sleep(0.075)
            else:
                expected = 17 if mode == "skip" else 19
                assert receive(target) == f"FAULT count=1 pc={store:#x} value={expected}"
                assert receive(target) == "DONE value=23"
            trace.send_signal(signal.SIGINT)
            output, _ = trace.communicate(timeout=20)
            assert trace.returncode == 0, output
            rows = [json.loads(line) for line in output.splitlines()]
            assert not rows[-1].get("restorationPending", False), rows[-1]
            assert all(live(pc, len(before)) == before for pc, before in original.items())
        events = [row["objectObservation"] for row in rows if "objectObservation" in row]
        writes = [item for item in events if item["kind"] in ("FIELD_CHANGED", "FIELD_WRITTEN")]
        assert all(item["pc"] == str(store) for item in writes), writes
        if mode == "paced":
            assert receive(target) == "DONE count=1000 value=1017"
            assert 0 < len(writes) < 1000
        else:
            gaps = [item for item in events if item["kind"] == "COVERAGE_GAP"]
            assert len(gaps) == 1 and "signal 11" in gaps[0]["diagnostic"], events
            assert len(writes) == (1 if mode == "skip" else 2), events
            if not no_values:
                expected = [(17, 23)] if mode == "skip" else [(17, 19), (19, 23)]
                assert [(item["oldInteger"], item["newInteger"]) for item in writes] == expected, writes
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in writes)
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} {mode} values={not no_values} "
          f"{len(writes)} writes, restored {len(windows)} windows")


def run_atomic(binary, fixture, no_values, agent):
    with tempfile.TemporaryDirectory(prefix="ct-aarch64-atomic-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = start_process(cleanup, root, [fixture])
        assert receive(target) == "READY"
        request = root / "definition.json"
        request.write_text(json.dumps({
            "concreteType": "arm_test::AtomicRecord", "field": "value",
            "codeSymbol": "arm_atomic_write", "noValues": no_values,
            "backend": "CODE_CACHE" if agent else "PTRACE",
        }))
        plan = preview(binary, target.pid, request, agent)
        assert plan["fieldEncoding"] == "libstdc++ atomic int32", plan
        windows = {int(site["runtimePc"]): len(b64decode(site["original"])) for site in plan["bindings"]
                   if site["role"] in ("construct", "destroy", "store")}
        stores = {int(site["runtimePc"]) for site in plan["bindings"] if site["role"] == "store"}
        assert len(stores) == 3, plan
        with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
            def live(pc, length):
                memory.seek(pc)
                return memory.read(length)

            original = {pc: live(pc, size) for pc, size in windows.items()}
            trace = start_watch(cleanup, root, binary, target, request, agent, live, original, len(stores))
            (root / "write.now").touch()
            assert receive(target) in ("DONE olds=30,33 cas=1,0 value=70",
                                       "DONE olds=33,30 cas=1,0 value=70")
            trace.send_signal(signal.SIGINT)
            output, _ = trace.communicate(timeout=20)
            assert trace.returncode == 0, output
            assert all(live(pc, len(before)) == before for pc, before in original.items())
        events = [json.loads(line) for line in output.splitlines()]
        writes = [row for row in events if row.get("objectObservation", {}).get("locationKind") == "STORE"]
        assert len(writes) == 4 and len({row["tid"] for row in writes}) == 3, events
        items = [row["objectObservation"] for row in writes]
        assert {int(item["pc"]) for item in items} == stores, items
        if no_values:
            assert all(item["kind"] == "FIELD_WRITTEN" and
                       "oldInteger" not in item and "newInteger" not in item for item in items), items
        else:
            assert sorted((item.get("oldInteger", -1), item["newInteger"]) for item in items) == [
                (-1, 30), (30, 33), (33, 36), (36, 70)], items
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} atomics values={not no_values} "
          "release store, two fetch-adds, successful and failed CAS, exact restoration")


def run_shared(binary, fixture, library, no_values, agent, cfi_depth):
    with tempfile.TemporaryDirectory(prefix="ct-aarch64-shared-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = start_process(cleanup, root, [fixture, library])
        assert receive(target) == "READY"
        request = root / "definition.json"
        stack = {"cfiStack": True, "stackDepth": cfi_depth} if cfi_depth else {}
        request.write_text(json.dumps({
            "concreteType": "demo::PlanRecord", "field": "value",
            "codeSymbol": "arm_shared_store", "codeModule": library.name,
            "noValues": no_values, "backend": "CODE_CACHE" if agent else "PTRACE",
            **stack,
        }))
        plan = preview(binary, target.pid, request, agent)
        stores = [site for site in plan["bindings"] if site["role"] == "store"]
        assert len(stores) == 1 and Path(stores[0]["module"]) == library, plan
        assert plan["writerBuildId"] and plan["writerBuildId"] != plan["executableBuildId"]
        windows = {int(site["runtimePc"]): b64decode(site["original"]) for site in plan["bindings"]
                   if site["role"] in ("construct", "destroy", "store")}
        with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
            def live(pc, length):
                return os.pread(memory.fileno(), length, pc)

            assert all(live(pc, len(before)) == before for pc, before in windows.items())
            trace = start_watch(cleanup, root, binary, target, request, agent, live, windows)
            (root / "write.now").touch()
            assert receive(target) == "DONE value=23"
            trace.send_signal(signal.SIGINT)
            output, _ = trace.communicate(timeout=20)
            assert trace.returncode == 0, output
            assert all(live(pc, len(before)) == before for pc, before in windows.items())
        rows = [json.loads(line) for line in output.splitlines()]
        assert not rows[-1].get("active") and not rows[-1].get("restorationPending"), rows
        writes = [row for row in rows
                  if row.get("objectObservation", {}).get("locationKind") == "STORE"]
        assert len(writes) == 1, rows
        observation = writes[0]["objectObservation"]
        assert observation["pc"] == stores[0]["runtimePc"] and observation["line"], observation
        assert observation["moduleBuildId"] == plan["writerBuildId"], observation
        if no_values:
            assert observation["kind"] == "FIELD_WRITTEN"
            assert "oldInteger" not in observation and "newInteger" not in observation
        else:
            assert observation["kind"] == "FIELD_CHANGED"
            assert (observation["oldInteger"], observation["newInteger"]) == (17, 23)
        if cfi_depth:
            stack = writes[0]["stack"]
            assert stack["frames"][0]["pc"] == observation["pc"], stack
            if cfi_depth == 1:
                assert stack["status"] == "TRUNCATED" and len(stack["frames"]) == 1, stack
            else:
                assert stack["status"] == "COMPLETE" and len(stack["frames"]) >= 2, stack
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} shared writer values={not no_values} "
          f"with an indirect call, verified module identity and exact restoration, CFI depth={cfi_depth}")


def run_lifecycle(binary, fixture, no_values, agent):
    for alias in (False, True):
        with tempfile.TemporaryDirectory(prefix="ct-aarch64-lifecycle-") as directory, ExitStack() as cleanup:
            root = Path(directory)
            target = start_process(cleanup, root, [fixture])
            assert receive(target) == "READY"
            request = root / "definition.json"
            record = "Base" if alias else "Record"
            request.write_text(json.dumps({
                "concreteType": f"life::{record}", "field": "value_",
                "codeSymbol": f"life::{record}::write(int)", "noValues": no_values,
                "backend": "CODE_CACHE" if agent else "PTRACE",
            }))
            plan = preview(binary, target.pid, request, agent)
            windows = {int(site["runtimePc"]): len(b64decode(site["original"]))
                       for site in plan["bindings"] if site["role"].startswith("construct") or
                       site["role"] in ("destroy", "store")}
            stores = sum(site["role"] == "store" for site in plan["bindings"])
            with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
                def live(pc, length):
                    memory.seek(pc)
                    return memory.read(length)

                original = {pc: live(pc, size) for pc, size in windows.items()}
                trace = start_watch(cleanup, root, binary, target, request, agent, live, original, stores)
                (root / "write.now").touch()
                target.stdin.write("b\n" if alias else "g\n")
                target.stdin.flush()
                if not alias:
                    assert receive(target) == "caught"
                assert receive(target) == ("alias done" if alias else "variants done")
                trace.send_signal(signal.SIGINT)
                output, _ = trace.communicate(timeout=20)
                assert trace.returncode == 0, output
                assert all(live(pc, len(before)) == before for pc, before in original.items())
            rows = [json.loads(line) for line in output.splitlines()]
            observations = [row["objectObservation"] for row in rows if "objectObservation" in row]
            if alias:
                assert [item["kind"] for item in observations] == ["LIFETIME_UNCERTAIN"], observations
            else:
                timelines = defaultdict(list)
                aborted = []
                for item in observations:
                    if item["kind"] == "CONSTRUCTION_ABORTED":
                        aborted.append(item)
                    else:
                        timelines[item["lifetimeId"]].append(item)
                assert len(aborted) == 1 and aborted[0]["closureReason"] == "UNWOUND", observations
                assert len(timelines) == 6, observations
                for items in timelines.values():
                    assert [item["kind"] for item in items] == [
                        "CREATED", "FIELD_WRITTEN" if no_values else "FIELD_CHANGED",
                        "DESTROY_BEGIN", "TEARDOWN_END"], items
                    assert items[-1]["closureReason"] == "DESTRUCTOR_RETURN", items
                if no_values:
                    assert all("oldInteger" not in item and "newInteger" not in item for item in observations)
                else:
                    assert {(items[1]["oldInteger"], items[1]["newInteger"]) for items in timelines.values()} == {
                        (8, 10), (10, 20), (20, 30), (40, 41), (50, 51), (60, 61)}
            target.stdin.write("q\n")
            target.stdin.flush()
            assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} lifecycle values={not no_values} "
          "six lifetimes, one constructor unwind, one uncertain alias, exact restoration")


def run_collection(binary, fixture, no_values, agent, multi, distinct):
    with tempfile.TemporaryDirectory(prefix="ct-aarch64-collection-") as directory, ExitStack() as cleanup:
        root = Path(directory)
        target = start_process(cleanup, root, [fixture])
        assert receive(target) == "READY"
        request = root / "definition.json"
        request.write_text(json.dumps({
            "concreteType": "arm_test::Portfolio", "field": "quantity",
            "codeSymbol": "arm_test::Portfolio::rebalance()", "noValues": no_values,
            "vectorField": "legs", "elementKeyField": "id", "elementKey": 12,
            "mutationSymbol": "arm_reserve", "firstSeen": True,
            "additionalMutationSymbols": ["arm_resize"] if distinct else [],
            "synchronization": "THREAD_CONFINED",
            "backend": "CODE_CACHE" if agent else "PTRACE",
        }))
        plan = preview(binary, target.pid, request, agent)
        windows = {int(site["runtimePc"]): len(b64decode(site["original"])) for site in plan["bindings"]}
        stores = sum(site["role"] == "store" for site in plan["bindings"])
        with Path(f"/proc/{target.pid}/mem").open("rb", buffering=0) as memory:
            def live(pc, length):
                memory.seek(pc)
                return memory.read(length)

            original = {pc: live(pc, size) for pc, size in windows.items()}
            trace = start_watch(cleanup, root, binary, target, request, agent, live, original, stores)
            (root / "write.now").touch()
            pattern = (r"DONE previous=(0x[0-9a-f]+) middle=(0x[0-9a-f]+) current=(0x[0-9a-f]+) value=80"
                       if multi else r"DONE previous=(0x[0-9a-f]+) current=(0x[0-9a-f]+) value=75")
            done = re.fullmatch(pattern, receive(target))
            assert done and len(set(done.groups())) == len(done.groups()), done
            trace.send_signal(signal.SIGINT)
            output, _ = trace.communicate(timeout=20)
            assert trace.returncode == 0, output
            assert all(live(pc, len(before)) == before for pc, before in original.items())
        rows = [json.loads(line) for line in output.splitlines()]
        items = [row["objectObservation"] for row in rows if "objectObservation" in row]
        write = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        kinds = ["FIRST_SEEN", "RANGE_BOUND", "RANGE_SUSPENDED", "RANGE_BOUND", write]
        generations = ["1", "1", "2", "2"]
        addresses = [done[1], done[1], done[2], done[2]]
        values = [(100, 75)]
        if multi:
            kinds.extend(["RANGE_SUSPENDED", "RANGE_BOUND", write])
            generations.extend(["2", "3", "3"])
            addresses.extend([done[2], done[3], done[3]])
            values.append((75, 80))
        assert [item["kind"] for item in items] == kinds + ["OBSERVATION_ENDED"], items
        assert [item["storageGeneration"] for item in items[1:-1]] == generations
        assert [item["destination"] for item in items[1:-1]] == [str(int(value, 16)) for value in addresses]
        writes = [item for item in items if item["kind"] == write]
        if no_values:
            assert all("oldInteger" not in item and "newInteger" not in item for item in writes)
        else:
            assert [(item["oldInteger"], item["newInteger"]) for item in writes] == values
        (root / "stop.now").touch()
        assert target.wait(timeout=10) == 0
    print(f"AArch64 {'code-cache' if agent else 'ptrace'} collection values={not no_values} "
          f"{3 if multi else 2} generations, one logical element, exact restoration")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("binary", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument("--faults", action="store_true")
    scenario.add_argument("--atomic", action="store_true")
    scenario.add_argument("--lifecycle", action="store_true")
    scenario.add_argument("--collection", action="store_true")
    scenario.add_argument("--multi-mutation", action="store_true")
    scenario.add_argument("--distinct-mutations", action="store_true")
    scenario.add_argument("--shared-library", type=lambda value: Path(value).resolve())
    parser.add_argument("--agent", type=lambda value: Path(value).resolve())
    parser.add_argument("--cfi-depth", type=int, default=0)
    args = parser.parse_args()
    if args.cfi_depth and not (args.agent and args.shared_library):
        parser.error("--cfi-depth requires a native shared-library watch")
    inputs = [path for path in (args.fixture, args.shared_library) if path]
    digests = [hashlib.sha256(path.read_bytes()).digest() for path in inputs]
    for no_values in (False, True):
        if args.shared_library:
            run_shared(args.binary, args.fixture, args.shared_library, no_values, args.agent, args.cfi_depth)
        elif args.collection or args.multi_mutation or args.distinct_mutations:
            run_collection(args.binary, args.fixture, no_values, args.agent,
                           args.multi_mutation or args.distinct_mutations, args.distinct_mutations)
        elif args.lifecycle:
            run_lifecycle(args.binary, args.fixture, no_values, args.agent)
        elif args.atomic:
            run_atomic(args.binary, args.fixture, no_values, args.agent)
        elif args.faults:
            for mode in ("retry", "skip", "paced"):
                run_fault(args.binary, args.fixture, mode, no_values, args.agent)
        else:
            run(args.binary, args.fixture, no_values, args.agent)
    assert [hashlib.sha256(path.read_bytes()).digest() for path in inputs] == digests
