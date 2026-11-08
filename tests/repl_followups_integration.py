"""Verify the local Frida follow-ups through the native REPL."""

from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop


def wait_file(path, pattern):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if path.exists() and (match := re.search(pattern, path.read_text(), re.MULTILINE)):
            return match
        time.sleep(0.025)
    raise AssertionError((pattern, path.read_text() if path.exists() else str(path)))


def verify(repl, server, agent, target, watch_image, preload):
    with tempfile.TemporaryDirectory(prefix="ct-follow-") as name:
        root = Path(name)
        flag, stdout = root / "control", root / "stdout"
        callback = root / "observer.cpp"

        def update(path, content):
            draft = path.with_suffix(".tmp")
            draft.write_text(content)
            os.replace(draft, path)

        def source(value):
            return f"""struct Interceptor {{
  void pre(const Call &call) {{ call.replace_with({value}); }}
  void post(const Call &call) {{ call.field("replacement", {value}); }}
}};"""

        update(callback, source(901))
        console = subprocess.Popen([repl, "--server", server, "--agent", agent],
                                   cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        output = Transcript(console.stdout)
        try:
            def send(line):
                console.stdin.write(line + "\n")
                console.stdin.flush()

            def command(letter):
                update(flag, letter)

            def mapping():
                send("%memory maps")
                token = output.expect(r"^memory mapping=(\S+)$")[1]
                output.expect(r"^memory mappings=\d+$")
                return token

            def values(probe, imported, patched, original):
                before = stdout.read_text().count("values probe=")
                command("v")
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    rows = [line for line in stdout.read_text().splitlines()
                            if line.startswith("values probe=")]
                    if len(rows) > before:
                        assert rows[-1] == (f"values probe={probe} import={imported} "
                                            f"patch={patched} original={original}"), rows
                        return
                    time.sleep(0.025)
                raise AssertionError(stdout.read_text())

            launch = f'%launch "{target}" --arg gate --arg "{flag}" --arg "{watch_image}"'
            send(launch + f' --stdout "{stdout}" --timeout 12')
            glibc = output.expect(r'^target JIT runtime .*glibc_version: "([0-9.]+)"')[1]
            pid = int(output.expect(r"^launched pid=(\d+) session=\S+ paused=true$")[1])
            assert not stdout.read_text()
            send("%call set --arg 17")
            output.expect(r"type=int32 value=117$")
            send("%call read")
            output.expect(r"type=int32 value=117$")
            send("%resume")
            output.expect(rf"^resumed pid={pid}$")
            ready = wait_file(stdout, rf"^ready pid={pid} import=(0x\w+) replacement=(0x\w+) "
                              r"data=(0x\w+) page=(\d+) receiver=117$")
            imported, replacement, data, page = [int(value, 0) for value in ready.groups()]

            send(f'%modules --elf "{target}"')
            output.expect(r'^elf IMPORT .* "ct_followup_import"$')
            output.expect(r'^elf EXPORT .* "ct_followup_replacement"$')
            output.expect(r'^elf SECTION .* "\.text"$')
            send("%modules --watch --count 2 --timeout 10")
            output.expect(r"^module watch ready$")
            command("d")
            output.expect(r'^module loaded .*watch\.so"$')
            command("u")
            output.expect(r'^module unloaded .*watch\.so"$')
            output.expect(r"^module watch complete changes=2$")

            send(f'%probe inject "followup_observed(int)" --callback "{callback}" '
                 "--allow-replacement --reclaimable")
            old = int(output.expect(r"^installed probe=(\d+)")[1])
            values(901, 202, 11, 0)
            send(f'%probe reload {old} --callback "{callback}" --watch --count 2 --timeout 15')
            output.expect(rf"^probe watch ready probe={old}$")
            update(callback, "this is invalid C++\n")
            output.expect(r"^probe watch rejected:")
            values(901, 202, 11, 0)
            update(callback, source(902))
            new = int(output.expect(rf"^reloaded probe=(\d+) replaces={old} ")[1])
            output.expect(rf"^probe watch complete changes=2 probe={new}$")
            values(902, 202, 11, 0)
            send(f"%probe remove {old}")
            output.expect(rf"^removed probe={old} restored=false reclaimed=true$")
            for _ in range(int(os.environ.get("CLANG_TRACER_TEST_RECLAIM_RELOADS", "0"))):
                send(f'%probe reload {new} --callback "{callback}"')
                reload = output.expect(rf"^reloaded probe=(\d+) replaces={new} .*agent_retained=(\d+)$")
                new = int(reload[1])
                assert 1 <= int(reload[2]) < 32

            entries = subprocess.check_output(["nm", "-n", target], text=True)
            elf_address = int(re.search(r"^([0-9a-f]+) T ct_followup_patch$", entries, re.M)[1], 16)
            send(f'%memory patch --module "{target}" --elf-address {elf_address:#x} '
                 f'--mapping {mapping()} --expect "b8 0b 00 00 00" --replace "b8 16 00 00 00"')
            patch = int(output.expect(r"^memory patch=(\d+) address=0x\w+ size=5 restored=false$")[1])
            values(902, 202, 22, 0)
            send(f'%memory import-hook --module "{target}" --symbol ct_followup_import '
                 f'--original {imported:#x} --address {replacement:#x} --mapping {mapping()} '
                 "--abi linux-x86-64-sysv")
            hook = int(output.expect(r"^memory patch=(\d+) address=0x\w+ size=8 restored=false$")[1])
            values(902, 702, 22, 0)
            send(f"%memory restore {patch}")
            output.expect(rf"^memory patch={patch} address=0x\w+ size=5 restored=true$")
            values(902, 702, 11, 0)

            def monitor(seconds):
                send(f"%memory monitor-start --tid {pid} --address {data:#x} "
                     f"--size {2 * page} --mapping {mapping()} --timeout {seconds}")
                output.expect(rf"^page monitor started tid={pid}$")

            monitor(5)
            command("r")
            wait_file(stdout, r"^touched value=7 probe=902$")
            send("%memory monitor-stop")
            output.expect(rf"^page read address={data + 31:#x} pc=0x\w+$")
            output.expect(rf"^page write address={data + page + 53:#x} pc=0x\w+$")
            output.expect(r"^page monitor stopped count=2 expired=false$")
            monitor(1)
            time.sleep(1.2)
            send("%memory monitor-stop")
            output.expect(r"^page monitor stopped count=0 expired=true$")
            monitor(5)
            send("%detach")
            output.expect(r"^detached session=\S+$")
            values(42, 202, 11, 1)
            command("r")
            wait_file(stdout, r"^touched value=7 probe=42$")
            command("q")

            preload_log = root / "preload"
            send(launch.replace("--arg gate", "--arg plain") + f' --preload "{preload}" '
                 f'--stdout "{preload_log}" --supervise --timeout 12')
            preload_pid = int(output.expect(r"^launched pid=(\d+) session=\S+ paused=true$")[1])
            assert not preload_log.read_text()
            update(callback, """struct Interceptor {
  void pre(const Call &call) { call.field("preloaded", 1); }
};""")
            send(f'%probe inject "followup_observed(int)" --callback "{callback}"')
            preload_probe = int(output.expect(r"^installed probe=(\d+)")[1])
            send("%resume")
            output.expect(rf"^resumed pid={preload_pid}$")
            wait_file(preload_log, rf"^ready pid={preload_pid} ")
            command("v")
            wait_file(preload_log, r"^values probe=42 import=202 patch=11 original=1$")
            send(f"%events --probe {preload_probe} --count 1 --timeout 5")
            output.expect(rf"probe={preload_probe} phase=PRE .*preloaded=1")
            output.expect(r"^received=1 .*complete=true")
            command("f")
            child = int(wait_file(preload_log, r"^child pid=(\d+)$")[1])
            deadline = time.monotonic() + 5
            child_stat = Path(f"/proc/{child}/stat")
            while time.monotonic() < deadline:
                try:
                    if child_stat.read_text().rsplit(")", 1)[1].split()[0] == "Z":
                        break
                except FileNotFoundError:
                    break
                time.sleep(0.05)
            else:
                raise AssertionError("supervised descendant survived leader exit")
            send("%quit")
            assert console.wait(timeout=15) == 0, "".join(output.seen)
            print(f"Verified all Frida follow-up paths and restoration on glibc {glibc}")
        finally:
            stop(console)


if __name__ == "__main__":
    verify(*map(Path, sys.argv[1:]))
