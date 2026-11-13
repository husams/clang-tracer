"""Exercise the bulk dry run against real local Linux C++ targets."""

from contextlib import ExitStack
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop
from elf_split_fixture import link_split_fixture


def main(repl, server, names_source, concrete, browser, library, compiler):
    with tempfile.TemporaryDirectory(prefix="ct-bulk-") as directory, ExitStack() as stack:
        root = Path(directory)
        target = root / "cpp-name-fixture"
        link_split_fixture(compiler, names_source, root, target.name, "target.debug",
                           ["-std=c++20", "-O0", "-fPIE", "-fno-omit-frame-pointer",
                            "-fcf-protection=none"], ["-pie"])

        def start(*args):
            process = subprocess.Popen(args, stdout=subprocess.PIPE, text=True)
            stack.callback(stop, process)
            return process, Transcript(process.stdout)

        names, names_output = start(target)
        names_output.expect(r"^CPP_NAME_FIXTURE pid=")
        console = subprocess.Popen(
            [repl, "--server", server, "--attach", str(names.pid), "--fail-fast"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
        stack.callback(stop, console)
        output = Transcript(console.stdout)
        output.expect(rf"^attached session=\S+ pid={names.pid}$")

        def send(command):
            console.stdin.write(command + "\n")
            console.stdin.flush()

        def preview(command, summary):
            first = len(output.seen)
            send(command)
            output.expect(r"^dry-run " + summary)
            return [
                line for line in output.seen[first:]
                if line.startswith(("DWARF_SUPPORTED ", "PREPARED_REQUIRED ",
                                    "UNSUPPORTED ", "AMBIGUOUS ", "UNMATCHED "))
            ]

        send('%symbols --filter "X::f(int)" --addresses')
        match = output.expect(r"^X::f\(int\).* address=(0x[0-9a-f]+) elf=(0x[0-9a-f]+)$")
        address, elf_address = map(lambda value: int(value, 16), match.groups())
        assert address != elf_address, "PIE lookup returned an ELF offset"
        manifest = root / "trace.textproto"
        with open(f"/proc/{names.pid}/mem", "rb", buffering=0) as memory:
            before = os.pread(memory.fileno(), 16, address)
            assert len(before) == 16
            rows = preview(
                '%trace preview --module "cpp-*" --include "X::*" --exclude "X::f(long)" '
                '--exact "UnsupportedFixture::unsupported(long double)" '
                '--exact "VirtualOnly::f(int)" --exact "Missing::run(int)" '
                f'--save "{manifest}"',
                r"matched=3 dwarf_supported=1 .*agent_retained=0 available=32 fits=yes$",
            )
            assert [line.split()[0] for line in rows] == [
                "UNSUPPORTED", "PREPARED_REQUIRED", "DWARF_SUPPORTED", "UNMATCHED"
            ], rows
            assert 'unsupported debug builtin: long double' in rows[0] and 'X::f(int)' in rows[2], rows
            output.expect(r"^manifest=")
            assert 'include_globs: "X::*"' in manifest.read_text()
            assert preview(
                f'%trace preview --manifest "{manifest}"',
                r"matched=3 dwarf_supported=1 .*agent_retained=0 available=32 fits=yes$",
            ) == rows
            assert os.pread(memory.fileno(), 16, address) == before
        assert "TracerPid:\t0" in Path(f"/proc/{names.pid}/status").read_text()

        callback = root / "observer.cpp"
        callback.write_text('struct Interceptor { void pre(const Call &) {} };\n')
        send(f'%probe inject "X::f(int)" --callback "{callback}" --module "{target}"')
        probe = output.expect(r"^installed probe=(\d+)")[1]
        send(f"%probe remove {probe}")
        output.expect(r"^removed probe=\d+ restored=true$")
        send("%detach")
        output.expect(r"^detached session=")
        send(f"%attach {names.pid}")
        output.expect(r"^attached session=")
        preview('%trace preview --exact "X::f(int)"',
                r"matched=1 dwarf_supported=1 limit=32 session_retained=0 agent_retained=1 available=31 fits=yes$")

        templated, _ = start(concrete)
        send(f"%attach {templated.pid}")
        output.expect(r"^attached session=")
        rows = preview(
            '%trace preview --include "prepared_concrete::twice*" '
            '--exact "prepared_concrete::Handler::route(int) const"',
            r"matched=3 dwarf_supported=2 .*available=32 fits=yes$",
        )
        assert any("PREPARED_REQUIRED" in row and "Handler::route(int) const" in row for row in rows), rows
        assert any("DWARF_SUPPORTED" in row and "twice<int>(int)" in row for row in rows), rows
        assert any("DWARF_SUPPORTED" in row and "twice<double>(double)" in row for row in rows), rows

        first_lib, second_lib = root / "libfirst.so", root / "libsecond.so"
        shutil.copyfile(library, first_lib)
        shutil.copyfile(library, second_lib)
        duplicate, duplicate_output = start(browser, first_lib, second_lib)
        duplicate_output.expect(r"^loaded$")
        send(f"%attach {duplicate.pid}")
        output.expect(r"^attached session=")
        rows = preview('%trace preview --exact "stack_add(int, int)"',
                       r"matched=2 dwarf_supported=0 .*available=32 fits=yes$")
        assert all(line.startswith("AMBIGUOUS ") for line in rows), rows
        rows = preview('%trace preview --module "libfirst.*" --exact "stack_add(int, int)"',
                       r"matched=1 dwarf_supported=1 .*available=32 fits=yes$")
        assert len(rows) == 1 and rows[0].startswith("DWARF_SUPPORTED "), rows
        send("%quit")
        assert console.wait(timeout=15) == 0, "".join(output.seen)
        assert all(child.poll() is None for child in (names, templated, duplicate))
        print("Bulk preview matched split DWARF, overloads, templates and virtual implementations without changing code.")


if __name__ == "__main__":
    main(*sys.argv[1:])
