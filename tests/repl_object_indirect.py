"""Observe a verified leaf reached through a native nonleaf indirect caller."""

import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, maps, read_code, read_redirects, send, stop, verify_restored_redirects


def run(args):
    fixture = Path(args.fixture)
    digests = {file: hashlib.sha256(file.read_bytes()).hexdigest()
               for file in (fixture, *map(Path, args.plugin))}
    with tempfile.TemporaryDirectory(prefix="ct-object-indirect-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        target, app = start([fixture, *args.plugin])
        app.expect(r"^READY$")
        live_read = None
        if args.runtime and platform.machine() == "aarch64":
            memory = cleanup.enter_context(Path(f"/proc/{target.pid}/mem").open("rb", buffering=0))

            def live_read(address, count):
                return os.pread(memory.fileno(), count, address)
        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        writer = "nonleaf_writer" if args.nonleaf else "indirect_writer"
        if args.runtime:
            writer = "nonleaf_dispatch"
        selector = f'"demo::{writer}(demo::PlanRecord &)"'
        if args.runtime == "selected_dso":
            writer = selector = "plugin_nested"
        definition = f"--type demo::PlanRecord --field value --backend {args.backend} --code {selector}"
        if args.runtime == "selected_dso":
            definition += f' --module "{args.plugin[0]}"'
        if args.multi:
            definition += ' --code "demo::write_value(demo::PlanRecord &)"'
        elif args.entry_multi:
            definition += ' --code "demo::indirect_again(demo::PlanRecord &)"'
        if args.runtime:
            definition += " --follow-calls"
            if args.bounded:
                definition += " --runtime-limit 1"
        elif not args.nonleaf and args.backend == "code-cache":
            definition += " --writer-entry"
        if args.no_values:
            definition += " --no-values"
        if args.discover:
            source = fixture.parent
            send(repl, f'%object prepare demo::PlanRecord --field value --discover-writers '
                       f'--compdb "{source / "compile_commands.json"}" --translation-unit "{source / "record.cpp"}" '
                       f'--translation-unit "{source / "indirect.cpp"}" --code {selector} '
                       f'--backend code-cache --writer-entry --module "{fixture}" --output watch')
            prepared = out.expect(r"^object package=|^error:")
            assert prepared.string.startswith("object package="), "".join(out.seen[-20:])
            out.expect(r'^source_writer="demo::write_value" symbol="\S+" file=".*record.cpp" line=[1-9][0-9]*$')
            definition = (root / "watch" / "object-definition.textproto").read_text()
            assert definition.count("source_writers {") == 1
            assert "demo::indirect_writer" in definition and "demo::write_value" in definition
        send(repl, f"%attach {target.pid}")
        out.expect(rf"^attached session=\S+ pid={target.pid}$")
        sites = []
        writes = 3 if args.multi or args.runtime == "shared" else (
            2 if args.nonleaf or args.entry_multi or args.runtime == "indirect" else 1)
        if args.nonleaf or args.entry_multi:
            send(repl, f"%object preview {definition}")
            status = out.expect(rf"^watch=0 active=no stores={writes} unsupported=\d+ .*|^error:")
            assert status.string.startswith("watch="), status.string
            for _ in range(writes):
                site = out.expect(r'^site role="store" elf=0x[0-9a-f]+ pc=(0x[0-9a-f]+) .*original=([0-9a-f]+)$')
                sites.append((int(site[1], 16), bytes.fromhex(site[2])))
            token = maps(repl, out)
            assert all(read_code(repl, out, pc, token, len(before)) == before for pc, before in sites)
        stack = f" --stack-depth {args.stack_depth}" + (" --stack-cfi" if args.cfi else "")
        if args.discover:
            send(repl, "%object start --plan watch" + stack)
        else:
            send(repl, f"%object start {definition}" + stack)
        count = 3 + writes
        static_writes = 0 if args.runtime else writes
        watch = out.expect(rf"^watch=(\d+) active=yes stores={static_writes} .*|^error:")
        assert watch[1], watch.string
        redirects = read_redirects(repl, out, watch, args.backend, target.pid, (),
                                   live_read=live_read, runtime_calls=bool(args.runtime))
        send(repl, f"%record indirect.db --sqlite --probe {watch[1]} --name object.trace --count {count} --timeout 20")
        send(target, "g")
        app.expect(rf"^WROTE value={19 if args.entry_multi or args.runtime == 'indirect' else 23}$")
        out.expect(rf"^received={count} .*complete=true")
        if args.runtime in ("discovered_nonleaf", "discovered_dso"):
            send(repl, f"%object status {watch[1]}")
            expanded = out.expect(rf"^watch={watch[1]} active=yes ")
            assert "runtime_translations=3 " in expanded.string, (expanded.string, "".join(app.seen[-12:]))
            current = read_redirects(repl, out, expanded, args.backend, target.pid, (),
                                     live_read=live_read, runtime_calls=True)
            assert len(current) == len(redirects) + 2
            redirects = current
        send(repl, f"%object stop {watch[1]}")
        final_status = out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
        if args.bounded:
            assert "coverage_gaps=1" in final_status.string, final_status.string
        if args.runtime:
            translated = (1 if args.bounded else 2 if args.runtime == "tail" else
                          3 if args.runtime in ("discovered_nonleaf", "discovered_dso") else writes)
            reclaimed = f"runtime_translations={translated} runtime_reclaimed={translated} "
            deadline = time.monotonic() + 5
            while reclaimed not in final_status.string:
                assert time.monotonic() < deadline, final_status.string
                time.sleep(0.01)
                send(repl, f"%object status {watch[1]}")
                final_status = out.expect(rf"^watch={watch[1]} active=no ")
        token = maps(repl, out) if live_read is None else None
        verify_restored_redirects(repl, out, token, target.pid, redirects, live_read=live_read)
        assert all(read_code(repl, out, pc, token, len(before)) == before for pc, before in sites)
        send(repl, "%detach")
        out.expect(r"^detached session=")
        send(repl, "%events open indirect.db --as saved")
        out.expect(rf"^opened events={count} retained={count} evicted=0$")
        send(repl, "%events export saved --format json --output indirect.json")
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 0
        events = json.loads((root / "indirect.json").read_text())
        observed = [event["objectObservation"] for event in events]
        kind = "FIELD_WRITTEN" if args.no_values else "FIELD_CHANGED"
        outcomes = [kind] * writes
        if args.bounded:
            outcomes[-1] = "COVERAGE_GAP"
            gap = observed[writes]
            assert gap.get("diagnostic") == "runtime translation limit reached", gap
            assert "oldInteger" not in gap and "newInteger" not in gap, gap
        assert [event["kind"] for event in observed] == [
            "CREATED", *outcomes, "DESTROY_BEGIN", "TEARDOWN_END"], observed
        if args.multi:
            values = [(17, 23), (23, 19), (19, 23)]
        elif args.runtime == "shared":
            values = [(17, 23), (23, 19), (19, 23)]
        elif args.entry_multi or args.runtime == "indirect":
            values = [(17, 23), (23, 19)]
        else:
            values = [(17, 19), (19, 23)] if args.nonleaf else [(17, 23)]
        for index, value in enumerate(values, 1):
            if args.bounded and index == writes:
                continue
            item = observed[index]
            expected = writer
            if args.runtime in ("direct", "nested", "discovered_nonleaf"):
                expected = "write_value"
            elif args.runtime == "indirect":
                expected = "indirect_again" if index == 1 else "indirect_writer"
            if index == 1:
                if args.multi:
                    expected = "write_value"
                elif args.entry_multi:
                    expected = "indirect_again"
            if args.runtime == "tail":
                assert item["function"] == "trace_tail_value", item
            elif args.runtime == "shared":
                plugin = Path(args.plugin[(index - 1) % 2])
                assert item["function"] == "plugin_write" and Path(item["module"]) == plugin, item
                assert item["moduleSha256"] == digests[plugin], item
                assert Path(item["file"]).name == "runtime_plugin.cpp" and item["line"] > 0, item
                assert events[index]["tid"] != target.pid, events[index]
            elif args.runtime in ("selected_dso", "discovered_dso"):
                plugin = Path(args.plugin[0])
                assert "plugin_nested_store" in item["function"] and Path(item["module"]) == plugin, item
                assert item["moduleSha256"] == digests[plugin], item
                assert Path(item["file"]).name == "runtime_plugin.cpp" and item["line"] > 0, item
            else:
                assert item["function"] == f"demo::{expected}(demo::PlanRecord&)"
            if args.no_values:
                assert "oldInteger" not in item and "newInteger" not in item
            else:
                assert (item["oldInteger"], item["newInteger"]) == value
            captured = events[index]["stack"]
            if args.runtime == "shared":
                assert captured["status"] in ("COMPLETE", "PARTIAL"), captured
            else:
                assert captured["status"] == (
                    ("TRUNCATED" if args.stack_depth == 1 else "COMPLETE") if args.cfi else "PARTIAL"), captured
            if args.cfi:
                assert "original application instruction" in captured["detail"]
            if args.runtime == "tail":
                assert any("nonleaf_dispatch" in frame.get("function", "") for frame in captured["frames"])
            if args.runtime in ("nested", "discovered_nonleaf"):
                assert any("nested_relay" in frame.get("function", "") for frame in captured["frames"])
                assert any("nonleaf_dispatch" in frame.get("function", "") for frame in captured["frames"])
            if args.runtime == "discovered_nonleaf":
                assert any("discovered_relay" in frame.get("function", "") for frame in captured["frames"])
            if args.runtime in ("selected_dso", "discovered_dso"):
                assert any("plugin_relay" in frame.get("function", "") for frame in captured["frames"])
                assert any("plugin_nested" in frame.get("function", "") for frame in captured["frames"])
                assert any("nonleaf_dispatch" in frame.get("function", "") for frame in captured["frames"])
            assert events[index]["stack"]["frames"][0]["pc"] == item["pc"], events[index]
        if args.runtime == "shared":
            assert len({item["runtimeModuleGeneration"] for item in observed[1:4]}) == 3, observed
            assert observed[1]["elfPc"] == observed[3]["elfPc"], observed
        send(target, "q")
        assert target.wait(timeout=10) == 0
        assert all(hashlib.sha256(file.read_bytes()).hexdigest() == digest for file, digest in digests.items())
        print(f"backend={args.backend} values={not args.no_values} writer={writer} writes={writes} "
              f"stack={events[1].get('stack', {}).get('status', 'unavailable')} "
              f"restored={len(redirects)} redirects")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--discover", action="store_true")
    parser.add_argument("--nonleaf", action="store_true")
    parser.add_argument("--multi", action="store_true")
    parser.add_argument("--entry-multi", action="store_true")
    parser.add_argument("--runtime", choices=("direct", "indirect", "shared", "tail", "nested", "selected_dso",
                                              "discovered_nonleaf", "discovered_dso"))
    parser.add_argument("--plugin", action="append", type=lambda value: str(Path(value).resolve()), default=[])
    parser.add_argument("--bounded", action="store_true")
    parser.add_argument("--cfi", action="store_true")
    parser.add_argument("--stack-depth", type=int, default=8)
    parser.add_argument("--no-values", action="store_true")
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="code-cache")
    args = parser.parse_args()
    if args.multi and (not args.nonleaf or args.discover):
        parser.error("--multi requires --nonleaf with explicit writers")
    if args.entry_multi and (args.multi or args.nonleaf or args.discover):
        parser.error("--entry-multi requires two explicitly selected leaf writers")
    if args.cfi and args.backend != "code-cache":
        parser.error("--cfi requires code-cache")
    if args.runtime and (args.nonleaf or args.multi or args.entry_multi or args.discover or args.backend != "code-cache"):
        parser.error("runtime following requires a standalone code-cache watch")
    if args.runtime == "shared" and len(args.plugin) != 2:
        parser.error("shared runtime following requires two plug-in libraries")
    if args.runtime in ("selected_dso", "discovered_dso") and len(args.plugin) != 1:
        parser.error("a selected shared scope requires one plug-in library")
    if args.bounded and args.runtime not in ("indirect", "tail"):
        parser.error("the runtime limit test requires two callees")
    if args.runtime:
        for args.no_values in (False, True):
            run(args)
    else:
        run(args)


if __name__ == "__main__":
    main()
