"""Verify more than 32 active probes, event/log lookup, and byte restoration."""

from contextlib import ExitStack
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, fixture):
    with tempfile.TemporaryDirectory(prefix="ct-many-") as directory, ExitStack() as stack:
        root = Path(directory)
        agent_log = root / "agent.log"
        child = subprocess.Popen(
            [fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
            env=dict(os.environ, CLANG_TRACER_AGENT_LOG=str(agent_log)),
        )
        stack.callback(stop, child)
        target_out = Transcript(child.stdout)
        target_out.expect(r"^ready entry=")
        memory = stack.enter_context(open(f"/proc/{child.pid}/mem", "rb", buffering=0))
        console = subprocess.Popen(
            [repl, "--server", server, "--timeout", "300", "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={child.pid}$")

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def call_all():
            child.stdin.write("m")
            child.stdin.flush()
            target_out.expect(r"^many calls returned 820$")

        send(f'%symbols --module "{fixture}" --filter "many_probe_" --addresses')
        original = {}
        for _ in range(40):
            row = output.expect(r"^many_probe_\d+\(int\) .*address=(0x[0-9a-f]+) elf=")
            entry = int(row[1], 16)
            original[entry] = os.pread(memory.fileno(), 16, entry)
        assert len(original) == 40
        assert all(len(code) == 16 for code in original.values())
        observer = root / "observer.cpp"
        observer.write_text('''struct Interceptor {
  void pre(const Call &call) { call.log("probe-cap-check"); }
  void post(const Call &) {}
};
''')
        manifest = root / "many.textproto"
        send(f'%trace preview --module "{fixture}" --include "many_probe_*" --save "{manifest}"')
        output.expect(r"^dry-run matched=40 dwarf_supported=40 limit=unlimited .*available=unlimited fits=yes$")
        output.expect(r"^manifest=")
        send(f'%trace apply --manifest "{manifest}" --callback "{observer}"')
        batch = output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
        probes = []
        for _ in range(40):
            row = output.expect(r'^INSTALLED "many_probe_\d+\(int\)" .*probe=(\d+) restored=false$', timeout=300)
            probes.append(row[1])
        assert len(set(probes)) == 40
        output.expect(rf"^bulk apply batch={batch} complete=true$")
        assert all(os.pread(memory.fileno(), 16, entry) != code
                   for entry, code in original.items())
        call_all()
        for probe in probes:
            send(f"%events --probe {probe} --count 2 --timeout 10")
            pre = output.expect(rf"^cursor=\d+ .*probe={probe} phase=PRE call=(\d+) ")
            output.expect(rf"^cursor=\d+ .*probe={probe} phase=POST call={pre[1]} ")
            output.expect(r"^received=2 .*complete=true")
        send(f"%trace remove {batch}")
        for _ in probes:
            output.expect(r"^REMOVED .*restored=true$")
        output.expect(rf"^bulk remove batch={batch} complete=true$")
        assert all(os.pread(memory.fileno(), 16, entry) == code
                   for entry, code in original.items())
        call_all()
        send("%detach")
        output.expect(r"^detached session=")
        logs = agent_log.read_text()
        for probe in probes:
            assert re.search(rf"probe_id={probe} .*operation=callback .*message=probe-cap-check$",
                             logs, re.MULTILINE), logs
        send("%quit")
        assert console.wait(timeout=15) == 0, "".join(output.seen)
        assert child.poll() is None


if __name__ == "__main__":
    main(*sys.argv[1:])
