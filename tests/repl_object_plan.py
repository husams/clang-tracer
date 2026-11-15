"""Prepare an object watch offline and reuse it across PIE runs and a changed layout."""

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
    binaries = [Path(args.fixture), Path(args.rebuilt)]
    hashes = [hashlib.sha256(binary.read_bytes()).hexdigest() for binary in binaries]
    with tempfile.TemporaryDirectory(prefix="ct-object-plan-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        endpoint = f"unix://{root / 'control.sock'}"
        _, server = start([args.server, "--listen", endpoint, "--agent", args.agent])
        server.expect("gRPC server listening")
        common = [args.repl, "--endpoint", endpoint]
        repl, out = start(common)
        package = root / "record-watch"
        send(repl, f'%object prepare demo::PlanRecord --module "{args.fixture}" --field value '
                   f'--code "demo::write_value(demo::PlanRecord &)" --backend code-cache --output {package}')
        prepared = out.expect(r"^object package=|^error:")
        assert prepared.string.startswith("object package="), prepared.string
        out.expect(r"^binding=offline$")
        first_build = out.expect(r"^field=value encoding=\"signed int32\" offset=0 build_id=([a-f0-9]+)$")[1]
        offline = out.expect(r'^site role="store" elf=(0x[a-f0-9]+) bytes=\d+ .+ original=([a-f0-9]+)$')
        assert (package / "object-definition.textproto").is_file()
        saved_bytes = (package / "object-plan.pb").read_bytes()
        assert saved_bytes
        observed_pcs = []
        for index, binary in enumerate((args.fixture, args.fixture, args.rebuilt)):
            target, app = start([binary])
            offset = int(app.expect(r"^READY offset=(\d+)$")[1])
            assert offset == (4 if index == 2 else 0)
            send(repl, f"%attach {target.pid}")
            session = out.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
            before_maps = Path(f"/proc/{target.pid}/maps").read_text()
            send(repl, f"%object preview --plan {package}")
            binding = "compatible-rebuild" if index == 2 else "same-build"
            status = out.expect(r"^binding=|^error:")
            assert status.string.strip() == f"binding={binding}", status.string
            build_id = out.expect(rf"^field=value encoding=\"signed int32\" offset={offset} build_id=([a-f0-9]+)$")[1]
            live = out.expect(r'^site role="store" elf=(0x[a-f0-9]+) pc=(0x[a-f0-9]+) bytes=\d+ .+ original=([a-f0-9]+)$')
            assert Path(f"/proc/{target.pid}/maps").read_text() == before_maps
            pc, original = int(live[2], 16), bytes.fromhex(live[3])
            if index < 2:
                assert (live[1], live[3], build_id) == (*offline.groups(), first_build)
                observed_pcs.append(pc)
            else:
                assert build_id != first_build and live[3] != offline[2]
            token = maps(repl, out)
            assert read_code(repl, out, pc, token, len(original)) == original
            send(repl, f"%object start --plan {package} --stack-depth 4")
            watch = out.expect(r"^watch=(\d+) active=yes stores=1 .*|^error:")
            assert watch[1], "".join(out.seen[-20:])
            redirects = read_redirects(repl, out, watch, "code-cache", target.pid, ())
            assert any(line.strip() == f"binding={binding}" for line in out.seen)
            db, export = root / f"{index}.db", root / f"{index}.json"
            recorder, recording = start(common)
            send(recorder, f"%record {db} --sqlite --session {session} --name object.trace "
                           f"--probe {watch[1]} --count 2 --timeout 20")
            existing_threads = {int(path.name) for path in Path(f"/proc/{target.pid}/task").iterdir()}
            send(target, "t" if index == 1 else "g")
            data = app.expect(r"^WROTE object=(0x[a-f0-9]+) field=(0x[a-f0-9]+) value=23 tid=(\d+)$")
            object_address, field_address = (int(value, 16) for value in data.groups()[:2])
            writer_tid = int(data[3])
            assert (writer_tid not in existing_threads) == (index == 1)
            assert field_address == object_address + offset
            recording.expect(r"^received=2 .*complete=true")
            send(recorder, "%quit")
            assert recorder.wait(timeout=20) == 0
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no .*")
            token = maps(repl, out)
            verify_restored_redirects(repl, out, token, target.pid, redirects)
            assert read_code(repl, out, pc, token, len(original)) == original
            send(repl, f"%events open {db} --as saved{index}")
            out.expect(r"^opened events=2 retained=2 evicted=0$")
            send(repl, f"%events export saved{index} --format json --output {export}")
            send(repl, "%detach")
            out.expect(rf"^detached session={re.escape(session)}$")
            birth, write = json.loads(export.read_text())
            origin = write["objectObservation"]
            assert birth["objectObservation"]["kind"] == "CREATED"
            assert birth["tid"] == write["tid"] == writer_tid
            assert origin["kind"] == "FIELD_CHANGED" and origin["locationKind"] == "STORE"
            assert origin["moduleBuildId"] == build_id and not origin.get("sourceUnavailable")
            assert int(origin["pc"]) == pc and int(origin["elfPc"]) == int(live[1], 16)
            assert int(origin["destination"]) == field_address and (origin["oldInteger"], origin["newInteger"]) == (17, 23)
            assert Path(origin["file"]).name == "record.cpp" and origin["line"] > 0
            assert int(write["stack"]["frames"][0]["pc"]) == pc
            send(target, "q")
            app.expect(r"^EXIT$")
            assert target.wait(timeout=20) == 0
            print(f"{binding} build={build_id} offset={offset} pc={pc:#x} elf={live[1]} "
                  f"new_writer={index == 1} restored_redirects={len(redirects)} 17->23", flush=True)
        assert len(set(observed_pcs)) == 2, observed_pcs
        if args.mismatched_companion:
            wrong = root / "wrong"
            (wrong / ".debug").mkdir(parents=True)
            target_path = wrong / binaries[1].name
            shutil.copy2(args.rebuilt, target_path)
            shutil.copy2(args.fixture, wrong / ".debug" / (target_path.name + ".debug"))
            target, app = start([target_path])
            invalid, rejection = start(common)
            app.expect(r"^READY offset=4$")
            send(invalid, f"%attach {target.pid}")
            session = rejection.expect(rf"^attached session=(\S+) pid={target.pid}$")[1]
            before_maps = Path(f"/proc/{target.pid}/maps").read_text()
            send(invalid, f"%object preview --plan {package}")
            rejection.expect(r"^error: .*(DWARF|missing|cannot|find)")
            assert Path(f"/proc/{target.pid}/maps").read_text() == before_maps
            send(invalid, "%detach")
            rejection.expect(rf"^detached session={re.escape(session)}$")
            send(invalid, "%quit")
            assert invalid.wait(timeout=20) != 0
            send(target, "g")
            app.expect(r"^WROTE .*value=23 ")
            send(target, "q")
            app.expect(r"^EXIT$")
            assert target.wait(timeout=20) == 0
            assert hashlib.sha256(target_path.read_bytes()).hexdigest() == hashes[1]
            print("mismatched companion rejected during preview; copied target executed unchanged")
        assert (package / "object-plan.pb").read_bytes() == saved_bytes
        assert [hashlib.sha256(binary.read_bytes()).hexdigest() for binary in binaries] == hashes
        send(repl, "%quit")
        assert repl.wait(timeout=20) == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture", "rebuilt"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--mismatched-companion", action="store_true",
                        help="reject the debug information from the original build for a stripped rebuild")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
