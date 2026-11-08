"""Verify named and stripped Linux x86-64 entry previews without patching."""

from contextlib import ExitStack
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


def main(repl, server, fixture, objcopy, nm):
    with tempfile.TemporaryDirectory(prefix="ct-entry-") as directory, ExitStack() as stack:
        stripped = Path(directory) / "stripped"
        subprocess.run([objcopy, "--strip-all", fixture, stripped], check=True)
        stripped.chmod(Path(fixture).stat().st_mode & 0o777)
        symbols = {}
        wanted = {"verified_entry", "indirect_entry", "direct_entry"}
        listing = subprocess.check_output([nm, "--defined-only", fixture], text=True)
        for line in listing.splitlines():
            fields = line.split()
            if len(fields) == 3 and fields[2] in wanted:
                symbols[fields[2]] = int(fields[0], 16)
        assert set(symbols) == wanted, symbols
        dynamic = subprocess.check_output([nm, "-D", stripped], text=True)
        assert all(name not in dynamic for name in wanted), dynamic

        def start(path):
            child = subprocess.Popen([path], stdout=subprocess.PIPE, text=True)
            stack.callback(stop, child)
            target_out = Transcript(child.stdout)
            line = target_out.expect(
                r"^entries good=(0x[0-9a-f]+) indirect=(0x[0-9a-f]+) "
                r"direct=(0x[0-9a-f]+) named=(0x[0-9a-f]+)$"
            )
            return child, target_out, tuple(int(value, 16) for value in line.groups())

        named, _, named_addresses = start(fixture)
        child, target_out, (good, indirect, direct, _) = start(stripped)
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(named.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def maps():
            send("%memory maps")
            token = output.expect(r"^memory mapping=(\S+)$")[1]
            output.expect(r"^memory mappings=")
            return token

        output.expect(rf"^attached session=\S+ pid={named.pid}$")
        token = maps()
        send(f'%probe entry "named_entry(int)" --mapping {token}')
        output.expect(rf'^entry={named_addresses[3]:#x} .*boundary=DWARF .*selector="named_entry\(int\)"$')
        output.expect(r"^patch_size=[1-9][0-9]* relocation=copy$")
        module = Path(fixture).resolve()
        send(f'%probe entry --module "{module.name}" --elf-address {symbols["verified_entry"]:#x} '
             f'--abi linux-x86-64-sysv --mapping {token} --cwd "{module.parent}"')
        output.expect(rf"^entry={named_addresses[0]:#x} .*boundary=ELF_SYMBOL ")
        output.expect(r"^patch_size=14 relocation=copy$")
        send("%detach")
        output.expect(r"^detached session=")

        send(f"%attach {child.pid}")
        output.expect(rf"^attached session=\S+ pid={child.pid}$")
        token = maps()
        assert good != symbols["verified_entry"], "fixture must be PIE"

        def raw(address, snapshot):
            send(f'%probe entry --module "{stripped}" --elf-address {address:#x} '
                 f'--abi linux-x86-64-sysv --mapping {snapshot}')

        with open(f"/proc/{child.pid}/mem", "rb", buffering=0) as memory:
            original = os.pread(memory.fileno(), 21, good)
            assert len(original) == 21
            raw(symbols["verified_entry"], token)
            output.expect(rf"^entry={good:#x} elf={symbols['verified_entry']:#x} "
                          r'.*boundary=EH_FRAME .*verified_bytes=21$')
            output.expect(rf"^{good:#x}  55 .*push")
            output.expect(r"^patch_size=14 relocation=copy$")
            assert os.pread(memory.fileno(), 21, good) == original

            raw(symbols["verified_entry"] + 1, token)
            output.expect(r"RPC 9: ELF address is not a unique function entry")
            raw(symbols["indirect_entry"], token)
            output.expect(rf"^entry={indirect:#x} .*boundary=EH_FRAME ")
            output.expect(rf"^{indirect:#x} .*jmp.*flow=BRANCH target=unresolved$")
            output.expect(r'^patch_size=0 reason="target entry contains an unsupported or PC-relative instruction"$')
            raw(symbols["direct_entry"], token)
            output.expect(rf"^entry={direct:#x} .*boundary=EH_FRAME ")
            output.expect(rf"^{direct + 2:#x} .*flow=BRANCH target={direct + 10:#x} next={direct + 4:#x}$")
            output.expect(rf"^{direct + 10:#x} .*mov")
            output.expect(r"^patch_size=0 reason=")

            child.send_signal(signal.SIGUSR1)
            target_out.expect(r"^mappings changed$")
            raw(symbols["verified_entry"], token)
            output.expect(r"RPC 10: memory mapping snapshot is stale")
            child.send_signal(signal.SIGUSR1)
            target_out.expect(r"^bytes changed$")
            raw(symbols["verified_entry"], maps())
            output.expect(r"RPC 10: live entry bytes differ from the mapped ELF")
            assert os.pread(memory.fileno(), 1, good) == b"\x90"

        send("%quit")
        assert console.wait(timeout=15) == 1, "".join(output.seen)
        assert all(process.poll() is None for process in (named, child))


if __name__ == "__main__":
    main(*sys.argv[1:])
