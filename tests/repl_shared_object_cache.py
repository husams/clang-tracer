"""Verify shared virtual writes and lifetime reuse through the native REPL."""

import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import (Output, VALUE_FIELDS, assert_untraced_tasks, maps, read_code, read_redirects,
                 send, stop, verify_restored_redirects)


def preview(control, out, token, selector, module=None, elf_address=None):
    option = f' --module "{module}"' if module else ""
    if elf_address is None:
        send(control, f'%probe entry {selector} --mapping {token}{option}')
    else:
        send(control, f'%probe entry --module "{module}" --elf-address {elf_address:#x} '
                      f'--abi linux-x86-64-sysv --mapping {token}')
    entry = out.expect(r'^entry=(0x[0-9a-f]+) elf=(0x[0-9a-f]+) module="([^"]+)" .*|^error:')
    assert entry[1], entry.string
    rows = {}
    while True:
        row = out.expect(r"^(0x[0-9a-f]+)  ((?:[0-9a-f]{2} )+) (.*)$|^patch_size=|^error:")
        if row.string.startswith("patch_size="):
            break
        assert row[1], row.string
        rows[int(row[1], 16)] = bytes.fromhex(row[2]), row[3]
    return (int(entry[1], 16), int(entry[2], 16), Path(entry[3])), rows


def life_symbols(library):
    symbols = subprocess.check_output(["nm", "-D", "--defined-only", library], text=True)
    result = {}
    for line in symbols.splitlines():
        address, _, name = line.split()
        variant = re.fullmatch(r"_ZN4demo6Circle(C[12]Ei|D[012]Ev)", name)
        if variant:
            result[variant[1][:2]] = int(address, 16)
    assert set(result) == {"C1", "C2", "D0", "D1", "D2"}, result
    return result


def elf_symbol(path, name):
    symbols = subprocess.check_output(["nm", "--defined-only", path], text=True)
    return next(int(row.split()[0], 16) for row in symbols.splitlines()
                if row.split()[-1] == name)


def verify_unpinned_unload(args):
    with ExitStack() as cleanup:
        target = subprocess.Popen([args.fixture, args.library], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, target)
        app = Output(target.stdout, args.timeout)
        app.expect(r"^READY ")
        mappings = Path(f"/proc/{target.pid}/maps")
        assert args.library in mappings.read_text()
        send(target, "u")
        app.expect(r"^CLOSED$")
        assert args.library not in mappings.read_text()
        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=args.timeout) == 0


def verify_events(events, args, target, owner, circle, field, addresses, no_values):
    getter, add, constructor, destroy, store, store_elf = addresses
    kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
    sequence = ["CREATED", "OWNER_ADDED", "BORROW_RETURN", kind, "DESTROY_BEGIN", "OWNER_REMOVED"]
    assert len(events) == 2 * len(sequence)
    lifetime_ids, borrow_ids = [], []
    for cycle in (1, 2):
        start = (cycle - 1) * len(sequence)
        group = events[start:start + len(sequence)]
        first = group[0]["objectObservation"]
        life = int(first["lifetimeId"])
        assert life
        lifetime_ids.append(life)
        borrow_id = int(group[2]["objectObservation"]["borrowId"])
        assert borrow_id
        borrow_ids.append(borrow_id)
        for i, (event, expected_kind) in enumerate(zip(group, sequence), start + 1):
            item = event["objectObservation"]
            assert event["tid"] == target and int(event["objectAddress"]) == circle
            assert int(event["probeSequence"]) == i and item["kind"] == expected_kind, item
            assert not int(event.get("agentDroppedEvents", 0))
            assert not int(event.get("serverDroppedEvents", 0))
            assert item["concreteType"] == "demo::Circle"
            assert int(item["lifetimeId"]) == life and int(item["generation"]) == cycle
            if expected_kind != "CREATED":
                assert int(item["ownerAddress"]) == owner
            if no_values or expected_kind not in ("CREATED", kind):
                assert not VALUE_FIELDS.intersection(item), item
        created, added, borrowed, write, destroyed, removed = (e["objectObservation"] for e in group)
        assert int(created["pc"]) == constructor and created["module"] == args.library
        assert int(added["pc"]) == add and added["module"] == args.fixture
        assert int(borrowed["pc"]) == getter and borrowed["module"] == args.fixture
        assert int(destroyed["pc"]) == int(removed["pc"]) == destroy
        assert destroyed["module"] == removed["module"] == args.library
        assert int(write["borrowId"]) == borrow_id and write["field"] == "demo::Circle::radius"
        assert write["function"] == "demo::Circle::resize(int)"
        assert int(write["destination"]) == field and int(write["pc"]) == store
        assert int(write["elfPc"]) == store_elf and write["module"] == args.library
        assert Path(write["file"]).name == "library.cpp"
        assert "RADIUS_WRITE" in Path(write["file"]).read_text().splitlines()[write["line"] - 1]
        if not no_values:
            assert created["newInteger"] == cycle * 10
            assert (write["oldInteger"], write["newInteger"]) == (cycle * 10, cycle * 10 + 5)
    assert lifetime_ids[0] != lifetime_ids[1] and borrow_ids[0] != borrow_ids[1]
    return lifetime_ids


def run_case(args, no_values):
    with tempfile.TemporaryDirectory(prefix="ct-shared-shapes-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, args.timeout)

        target, app = start([args.fixture, args.library])
        ready = app.expect(r"^READY image=(0x[\da-f]+) circle_target=(0x[\da-f]+) alternate_target=(0x[\da-f]+)$")
        owner, dynamic_target, other = (int(value, 16) for value in ready.groups())
        assert dynamic_target != other
        endpoint = f"unix://{root / 'control.sock'}"
        _, server = start([args.server, "--listen", endpoint, "--agent", args.agent])
        server.expect("gRPC server listening")
        common = [args.repl, "--endpoint", endpoint]
        control, out = start([*common, "--attach", str(target.pid)])
        session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        token = maps(control, out)
        caller, calls = preview(control, out, token, "borrowed_resize")
        getter, _ = preview(control, out, token, '"demo::Image::get()"')
        add, add_code = preview(control, out, token, "", args.fixture,
                                elf_symbol(args.fixture, "_ZN4demo5Image3addEPNS_5ShapeE"))
        _, remove = preview(control, out, token, '"demo::Image::remove()"')
        writer, code = preview(control, out, token, '"demo::Circle::resize(int)"', args.library)
        _, direct = preview(control, out, token, "direct_circle_resize", args.library)
        aliases = life_symbols(args.library)
        lifecycle, lifecycle_code = {}, {}
        for variant in ("C1", "D0", "D1"):
            entry, rows = preview(control, out, token, "", args.library, aliases[variant])
            lifecycle[variant] = entry[0]
            lifecycle_code.update(rows)
        assert writer[0] == dynamic_target and writer[2] == Path(args.library)
        assert caller[2] == getter[2] == add[2] == Path(args.fixture)
        [(get_pc, get_bytes)] = [(pc, raw) for pc, (raw, text) in calls.items()
                                 if f"flow=CALL target={getter[0]:#x}" in text]
        [(call_pc, call_bytes, assembly)] = [(pc, raw, text) for pc, (raw, text) in calls.items()
                                            if "flow=CALL target=unresolved" in text]
        [(remove_pc, remove_bytes)] = [(pc, raw) for pc, (raw, text) in remove.items()
                                       if raw[0] == 0xff and "flow=CALL target=unresolved" in text]
        [(direct_pc, direct_bytes)] = [(pc, raw) for pc, (raw, text) in direct.items()
                                       if raw[0] == 0xe8 and "flow=CALL target=0x" in text]
        assert get_bytes[0] == 0xe8 and call_bytes[0] == 0xff and "*" in assembly
        assert get_pc + len(get_bytes) <= call_pc
        [(store_pc, store_bytes)] = [(pc, raw) for pc, (raw, text) in code.items()
                                     if re.search(r"^movl\b.*\(%rdi\)", text)]
        assert writer[0] < store_pc
        original = {}
        before = [(get_pc, get_bytes), (call_pc, call_bytes), (direct_pc, direct_bytes),
                  (remove_pc, remove_bytes)]
        for rows in (code, add_code, lifecycle_code):
            before.extend((pc, row[0]) for pc, row in rows.items())
        for pc, raw in before:
            original[pc] = read_code(control, out, pc, token, len(raw))
            assert original[pc] == raw

        mode = "off" if no_values else "on"
        option = " --no-values" if no_values else ""
        send(control, '%object start --backend code-cache --type demo::Circle --base demo::Shape '
                      '--owner demo::Image --collection _shapes --field radius '
                      f'--caller borrowed_resize --module "{args.library}" '
                      f'--code "demo::Circle::resize(int)"{option}')
        watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
        assert watch[1], "".join(out.seen[-18:])
        assert f"values={mode} " in watch.string
        redirects = read_redirects(control, out, watch, "code-cache", target.pid, ())
        expected_sites = {get_pc, writer[0], add[0], *lifecycle.values()}
        assert set(redirects) == expected_sites, (
            {hex(pc) for pc in redirects}, {hex(pc) for pc in expected_sites}, lifecycle,
        )
        dispatch = (rf"^dispatch={call_pc:#x} target={dynamic_target:#x} "
                    rf"return={call_pc + len(call_bytes):#x} matched=")
        assert any(re.search(dispatch + "0$", line) for line in out.seen)
        for pc, (before, installed, translated) in redirects.items():
            expected = b"".join(original[step] for step in sorted(original) if pc <= step < pc + len(before))
            assert before == expected
            print(f"redirect={pc:#x} original={before.hex(' ')} installed={installed.hex(' ')} "
                  f"translated={translated:#x}", flush=True)
        send(target, "c")
        app.expect(r"^CLOSED$")
        token = maps(control, out)
        for pc in (store_pc, call_pc, remove_pc):
            assert read_code(control, out, pc, token, len(original[pc])) == original[pc]
        writer_redirect = redirects[writer[0]][1]
        assert read_code(control, out, writer[0], token, len(writer_redirect)) == writer_redirect
        send(target, "o")
        app.expect(r"^OTHER result=21$")
        send(control, f"%object status {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=yes .*observations=0 .*executed_stores=0 dropped=0 ")
        out.expect(dispatch + "0$")
        send(target, "d")
        app.expect(r"^DIRECT result=17$")
        send(control, f"%object status {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=yes .*observations=0 .*executed_stores=1 dropped=0 ")
        out.expect(dispatch + "0$")

        db, export = root / "shared.db", root / "shared.json"
        recorder, recording = start(common)
        send(recorder, f"%record {db} --sqlite --session {session} --name object.trace "
                       f"--probe {watch[1]} --count 12 --timeout {args.timeout}")
        objects, fields, times = [], [], []
        for cycle in (1, 2):
            send(target, "s")
            added = app.expect(rf"^ADDED cycle={cycle} circle=(0x[\da-f]+) field=(0x[\da-f]+) name=icon destroy=(0x[\da-f]+)$")
            circle, field, destroy = (int(value, 16) for value in added.groups())
            assert circle < field and destroy == lifecycle["D0"]
            objects.append(circle)
            fields.append(field)
            token = maps(control, out)
            vptr = int.from_bytes(read_code(control, out, circle, token, 8), "little")
            assert int.from_bytes(read_code(control, out, vptr + 8, token, 8), "little") == destroy
            assert int.from_bytes(read_code(control, out, vptr + 16, token, 8), "little") == dynamic_target
            send(target, "g")
            value = cycle * 10 + 5
            times.append(int(app.expect(rf"^WROTE cycle={cycle} returned={value} radius={value} ns=(\d+)$")[1]))
            send(target, "x")
            app.expect(rf"^REMOVED cycle={cycle} slot_free=yes$")
        assert objects[0] == objects[1] and fields[0] == fields[1]
        recording.expect(r"^received=12 .*complete=true")
        send(recorder, "%quit")
        assert recorder.wait(timeout=args.timeout) == 0
        assert_untraced_tasks(target.pid, ())
        send(control, f"%object status {watch[1]}")
        out.expect(rf"^watch={watch[1]} active=yes .*observations=12 .*executed_stores=3 dropped=0 ")
        out.expect(dispatch + "2$")

        send(control, "%detach")
        out.expect(rf"^detached session={re.escape(session)}$")
        send(control, f"%attach {target.pid}")
        next_session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
        assert next_session != session
        token = maps(control, out)
        verify_restored_redirects(control, out, token, target.pid, redirects)
        for pc, raw in original.items():
            assert read_code(control, out, pc, token, len(raw)) == raw
        send(target, "r")
        app.expect(r"^AFTER_DETACH radius=15 slot_free=yes$")

        send(control, f"%events open {db} --as shared")
        out.expect(r"^opened events=12 retained=12 evicted=0$")
        kind = "FIELD_WRITTEN" if no_values else "FIELD_CHANGED"
        for cycle in (1, 2):
            for index, label in enumerate(
                    ("CREATED", "OWNER_ADDED", "BORROW_RETURN", kind, "DESTROY_BEGIN", "OWNER_REMOVED"),
                    (cycle - 1) * 6):
                send(control, f"%events show shared --index {index}")
                row = out.expect(rf" {label} .*generation={cycle} address={circle:#x}").string.strip()
                assert ("->" in row) == (label == kind and not no_values), row
                if label == kind:
                    assert "demo::Circle::resize(int)" in row and args.library in row
                    assert f"destination={field:#x} pc={store_pc:#x}" in row
                    if not no_values:
                        assert f"{cycle * 10} -> {cycle * 10 + 5}" in row
                    else:
                        assert "demo::Circle::radius destination=" in row
                    print(row)
                elif no_values and label == "CREATED":
                    assert " radius pc=" in row
        send(control, f"%events export shared --format json --output {export}")
        send(control, "%detach")
        out.expect(rf"^detached session={re.escape(next_session)}$")
        send(control, "%quit")
        assert control.wait(timeout=args.timeout) == 0
        store_elf = writer[1] + store_pc - writer[0]
        ids = verify_events(json.loads(export.read_text()), args, target.pid, owner, circle, field,
                            (getter[0], add[0], lifecycle["C1"], destroy, store_pc, store_elf), no_values)
        send(target, "q")
        app.expect(r"^EXIT$")
        assert target.wait(timeout=args.timeout) == 0
        print(f"values={mode} pid={target.pid} reused={circle:#x} destination={field:#x} "
              f"lifetime_ids={ids} generations=[1,2] indirect={call_pc:#x} bytes={call_bytes.hex(' ')} "
              f"remove={remove_pc:#x} bytes={remove_bytes.hex(' ')} "
              f"store={store_pc:#x} elf={store_elf:#x} bytes={store_bytes.hex(' ')} "
              f"elapsed_ns={times} restored={len(redirects)} "
              f"constructor_alias={aliases['C1'] == aliases['C2']} "
              f"destructor_alias={aliases['D1'] == aliases['D2']} exit=0", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture", "library"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--timeout", type=int, default=20)
    args = parser.parse_args()
    files = [Path(args.fixture), Path(args.library)]
    before = [hashlib.sha256(path.read_bytes()).digest() for path in files]
    verify_unpinned_unload(args)
    for no_values in (False, True):
        run_case(args, no_values)
    assert [hashlib.sha256(path.read_bytes()).digest() for path in files] == before


if __name__ == "__main__":
    main()
