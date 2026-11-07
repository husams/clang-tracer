"""Verify typed bulk apply, partial rollback, and byte restoration on Linux."""

from contextlib import ExitStack
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, target):
    with tempfile.TemporaryDirectory(prefix="ct-bulk-apply-") as directory, ExitStack() as stack:
        root = Path(directory)
        child = subprocess.Popen([target], stdout=subprocess.PIPE, text=True)
        stack.callback(stop, child)
        Transcript(child.stdout).expect(r"^CPP_NAME_FIXTURE pid=")
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={child.pid}$")

        def send(line):
            console.stdin.write(line + "\n")
            console.stdin.flush()

        memory = stack.enter_context(open(f"/proc/{child.pid}/mem", "rb", buffering=0))
        addresses = {}
        for name in ("BoolOnly::b(bool)", "X::f(int)", "X::f(long)"):
            send(f'%symbols --filter "{name}" --addresses')
            addresses[name] = int(output.expect(
                rf"^{re.escape(name)} .*address=(0x[0-9a-f]+) elf="
            )[1], 16)

        def code():
            return {name: os.pread(memory.fileno(), 16, address)
                    for name, address in addresses.items()}

        original = code()
        assert all(len(data) == 16 for data in original.values())
        observer = root / "observer.cpp"
        observer.write_text("struct Interceptor { void pre(const Call&) {} };\n")
        send(f'%probe inject "BoolOnly::b(bool)" --callback "{observer}"')
        prior_probe = output.expect(r"^installed probe=(\d+)")[1]
        prior = code()
        assert prior["BoolOnly::b(bool)"] != original["BoolOnly::b(bool)"]

        manifest = root / "overloads.textproto"
        send(f'%trace preview --include "X::*" --save "{manifest}"')
        output.expect(r"^dry-run matched=2 dwarf_supported=2 .*available=31 fits=yes$")
        output.expect(r"^manifest=")
        conflict = root / "conflict.textproto"
        send(f'%trace preview --exact "BoolOnly::b(bool)" --include "X::*" --save "{conflict}"')
        output.expect(r"^manifest=")
        send(f'%trace apply --manifest "{conflict}"')
        conflict_id = output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
        output.expect(r'^BLOCKED "BoolOnly::b\(bool\)".*reason="entry is already hooked"$')
        output.expect(r'^NOT_ATTEMPTED "X::f\(int\)"')
        output.expect(rf"^bulk apply batch={conflict_id} complete=false available=31$")
        assert code() == prior

        mismatched = root / "int-only.cpp"
        mismatched.write_text(
            "template<> class Interceptor<int(int)> {\n"
            "public: void pre(const Call&) {}\n"
            "};\n"
        )
        send(f'%trace apply --manifest "{manifest}" --callback "{mismatched}"')
        failed_batch = output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
        rolled_back = output.expect(r'^ROLLED_BACK "X::f\(int\)".*probe=(\d+) restored=true$')[1]
        output.expect(r'^INSTALL_FAILED "X::f\(long\)".*argument 0 type does not match')
        output.expect(rf"^bulk apply batch={failed_batch} complete=false available=31$")
        assert code() == prior
        send(f"%collect --probe {prior_probe} --count 2 --timeout 10")
        output.expect(r"received=2")
        send(f"%trace remove {failed_batch}")
        output.expect(rf"^REMOVED .*probe={rolled_back} restored=true$")
        output.expect(rf"^bulk remove batch={failed_batch} complete=true$")

        send(f'%trace apply --manifest "{manifest}"')
        batch = output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
        int_probe = output.expect(r'^INSTALLED "X::f\(int\)".*probe=(\d+) restored=false$')[1]
        long_probe = output.expect(r'^INSTALLED "X::f\(long\)".*probe=(\d+) restored=false$')[1]
        output.expect(rf"^bulk apply batch={batch} complete=true available=30$")
        assert len({prior_probe, rolled_back, int_probe, long_probe}) == 4
        installed = code()
        assert installed["BoolOnly::b(bool)"] == prior["BoolOnly::b(bool)"]
        assert installed["X::f(int)"] != original["X::f(int)"]
        assert installed["X::f(long)"] != original["X::f(long)"]
        for probe in (int_probe, long_probe):
            send(f"%collect --probe {probe} --phase POST --count 2 --timeout 10")
            output.expect(r"received=2")

        for _ in range(2):
            send(f"%trace remove {batch}")
            output.expect(rf"^REMOVED .*probe={long_probe} restored=true$")
            output.expect(rf"^REMOVED .*probe={int_probe} restored=true$")
            output.expect(rf"^bulk remove batch={batch} complete=true$")
            assert code() == prior
        send(f"%collect --probe {prior_probe} --count 2 --timeout 10")
        output.expect(r"received=2")
        send("%detach")
        output.expect(r"^detached session=")
        assert code() == original
        send("%quit")
        assert console.wait(timeout=15) == 1, "".join(output.seen)
        assert child.poll() is None
        print("Bulk rollback and repeated removal restored original bytes and preserved the prior probe.")


if __name__ == "__main__":
    main(*sys.argv[1:])
