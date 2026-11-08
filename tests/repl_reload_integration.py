"""Verify observer cutover while a target invocation is still running."""

from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, fixture):
    with tempfile.TemporaryDirectory(prefix="ct-reload-") as directory, ExitStack() as stack:
        root = Path(directory)
        files = [root / f"observer-{n}.cpp" for n in (1, 2)]
        for version, path in enumerate(files, start=1):
            path.write_text(f"""struct Interceptor {{
  void pre(const Call &call) {{ call.field("observer", {version}); }}
  void post(const Call &call) {{ call.field("observer", {version}); }}
}};
""")
        broken = root / "broken.cpp"
        broken.write_text("struct Interceptor { void pre(const Call &) { missing_function(); } };")
        child = subprocess.Popen([fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        stack.callback(stop, child)
        target_out = Transcript(child.stdout)
        address = int(target_out.expect(r"^ready entry=(0x[0-9a-f]+)$")[1], 16)
        memory = stack.enter_context(open(f"/proc/{child.pid}/mem", "rb", buffering=0))
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def read_code():
            return os.pread(memory.fileno(), 16, address)

        def target(command):
            child.stdin.write(command)
            child.stdin.flush()

        def events(probe, count, after, phases, version):
            send(f"%events --probe {probe} --count {count} --after {after} --timeout 10")
            seen = []
            for phase in phases:
                row = output.expect(
                    rf"^cursor=(\d+) .*probe={probe} phase={phase} call=(\d+) .* observer={version}(?: |$)"
                )
                seen.append((int(row[1]), int(row[2])))
            output.expect(rf"^received={count} .*complete=true")
            return seen

        def short():
            target("s")
            target_out.expect(r"^short call returned 12$")

        def reload(probe, path, retained):
            send(f'%probe reload {probe} --callback "{path}"')
            return output.expect(
                rf"^reloaded probe=(\d+) replaces={probe} retained_limit=32 agent_retained={retained}$"
            )[1]

        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        original = read_code()
        assert len(original) == 16
        send(f'%probe inject "observed(int)" --callback "{files[0]}"')
        old = output.expect(r"^installed probe=(\d+)")[1]
        old_patch = read_code()
        assert old_patch != original

        target("h")
        target_out.expect(r"^call held$")
        [(pre_seq, held_call)] = events(old, 1, 0, ["PRE"], 1)

        send(f'%probe reload {old} --callback "{broken}"')
        output.expect(r"error: .*agent rejected probe:")
        assert read_code() == old_patch
        short()
        short_old = events(old, 2, pre_seq, ["PRE", "POST"], 1)
        assert short_old[0][1] == short_old[1][1] != held_call

        current = reload(old, files[1], 2)
        assert current != old
        current_patch = read_code()
        assert current_patch not in (old_patch, original)
        short()
        short_new = events(current, 2, short_old[-1][0], ["PRE", "POST"], 2)
        assert short_new[0][1] == short_new[1][1]

        target("r")
        target_out.expect(r"^held call returned 11$")
        [(post_seq, completed_call)] = events(old, 1, short_new[-1][0], ["POST"], 1)
        assert completed_call == held_call and post_seq > short_new[-1][0]

        send(f'%probe reload {old} --callback "{files[0]}"')
        output.expect(r"RPC 9: reload requires the active probe that owns the entry")
        send(f"%probe remove {old}")
        output.expect(rf"^removed probe={old} restored=false$")
        assert read_code() == current_patch

        for retained in range(3, 33):
            current = reload(current, files[1], retained)
        last_patch = read_code()
        send(f'%probe reload {current} --callback "{files[0]}"')
        output.expect(r"RPC 8: reload reached the 32 retained-probe limit")
        assert read_code() == last_patch
        short()
        events(current, 2, post_seq, ["PRE", "POST"], 2)
        send("%detach")
        output.expect(r"^detached session=")
        assert read_code() == original
        short()
        send("%quit")
        assert console.wait(timeout=15) == 1, "".join(output.seen)
        assert child.poll() is None


if __name__ == "__main__":
    main(*sys.argv[1:])
