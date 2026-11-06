"""Exercise prepared generation and injection through the native C++ REPL."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "target", "changed-target", "example", "compdb"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = str(args.repl.parent / "clang-tracer-prepare-injectors-native")
    selector = '"prepared_demo::compute(int)"'
    with tempfile.TemporaryDirectory(prefix="native-prepared-test-") as directory:
        root = Path(directory)
        header = args.example / "calculator.hpp"
        donor = args.example / "calculator.cpp"
        callback = args.example / "interceptor.cpp"
        original_callback = callback.read_bytes()
        default_compdb = root / "compile_commands.json"
        local_header = root / "headers" / "calculator.hpp"
        local_header.parent.mkdir()
        local_header.write_text(
            '#ifndef FIRST_DIRECTORY_ENTRY\n#error wrong compile command\n#endif\n'
            f'#include "{header}"\n')
        default_compdb.write_text(json.dumps([
            {"directory": str(root), "file": source,
             "arguments": ["clang++", "-std=c++17", *flags, "-c", source]}
            for source, flags in (
                ("foreign.cpp", []),
                ("headers/z_first.cpp", ["-DFIRST_DIRECTORY_ENTRY"]),
                ("headers/a_second.cpp", []),
            )
        ]))
        package = root / "prepared"
        target_log = (root / "target.log").open("w")
        target = None
        changed = None
        console = None
        try:
            def launch(pid):
                return [str(args.repl), "--server", str(args.server), "--agent", str(args.agent),
                        "--attach", str(pid), "--timeout", "60", "--fail-fast"]

            def interact(prefix, suffix, process):
                nonlocal console
                console = subprocess.Popen(launch(process.pid), cwd=root, env=env,
                                           stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT)
                console.stdin.write(prefix.encode())
                console.stdin.flush()
                transcript = b""
                deadline = time.monotonic() + 100
                while not (match := re.search(rb"installed probe=(\d+)", transcript)):
                    remaining = deadline - time.monotonic()
                    assert remaining > 0, transcript.decode(errors="replace")
                    assert select.select([console.stdout], [], [], remaining)[0], transcript
                    data = os.read(console.stdout.fileno(), 65536)
                    assert data, transcript.decode(errors="replace")
                    transcript += data
                probe = match.group(1).decode()
                stdout, _ = console.communicate(suffix.replace("PROBE_ID", probe).encode(), timeout=100)
                text = (transcript + stdout).decode()
                assert console.returncode == 0, text
                assert "restored=true" in text and "detached session=" in text, text
                assert process.poll() is None, "tracing stopped the target"
                console = None
                return text

            preparation = f'%probe prepare {selector} --include "{local_header}" --output "{package}"'
            offline = subprocess.run([args.repl, "--fail-fast"], input=preparation + "\n%quit\n",
                                     text=True, capture_output=True, cwd=root, env=env, timeout=30)
            assert offline.returncode == 0 and "prepared directory=" in offline.stdout, (
                offline.stdout + offline.stderr)

            target = subprocess.Popen([args.target], stdout=target_log, stderr=subprocess.STDOUT)
            prefix = f'''%probe resolve {selector} --prepared "{package}" --prepared-mode require
%probe validate {selector} --callback "{callback}" --prepared "{package}" --prepared-mode require
%probe inject {selector} --callback "{callback}" --prepared "{package}" --prepared-mode require
'''
            suffix = '''%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
bool correct = captured.size() == 4;
for (const auto& e : captured) { correct = correct && e.result_as<int>() == e.argument_as<int>(0) * 3 + 7; }
std::cout << "PREPARED_TYPED_RESULTS=" << correct << std::endl;
%probe remove PROBE_ID
%quit
'''
            text = interact(prefix, suffix, target)
            assert "format_version: 2" in text and "module_sha256:" not in text, text
            assert "prepared_used: true" in text and "PREPARED_TYPED_RESULTS=1" in text, text
            subprocess.run([args.compiler, "-std=c++17", "-DFIRST_DIRECTORY_ENTRY", "-fsyntax-only",
                            package / "generated/adapter.cpp"], check=True)
            assert not (package / "interceptor.cpp").exists()

            default_compdb.write_text("invalid compilation database\n")
            cpp_package = root / "prepared_cpp"
            cpp_source = root / "prepared_cpp_source"
            cpp = f'''%probe resolve {selector}
tracer.prepare({selector}, "{header}", "{cpp_source}", "{args.compdb}", "{donor}");
tracer.prepare({selector}, "{header}", "{args.compdb}", "{donor}", "{args.target}", "{cpp_package}");
%probe validate {selector} --callback "{callback}"
auto hook = tracer.inject(session, {selector}, "{callback}");
'''
            text = interact(cpp, suffix, target)
            assert 'symbol: "_ZN13prepared_demo7computeEi"' in text, text
            assert "prepared_used: true" in text and "PREPARED_TYPED_RESULTS=1" in text, text
            assert (cpp_source / "prepared.pb").is_file()

            changed = subprocess.Popen([args.changed_target], stdout=target_log, stderr=subprocess.STDOUT)
            text = interact(prefix, suffix.replace("* 3 + 7", "* 3 + 8"), changed)
            assert "prepared_used: true" in text and "PREPARED_TYPED_RESULTS=1" in text, text
            for mode in ("require", "prefer", "off", None):
                mode_option = f" --prepared-mode {mode}" if mode else ""
                command = f'%probe resolve {selector} --prepared "{cpp_package}"{mode_option}\n%quit\n'
                result = subprocess.run(launch(changed.pid), input=command, text=True,
                                        capture_output=True, cwd=root, env=env, timeout=100)
                text = result.stdout + result.stderr
                if mode == "require":
                    assert result.returncode != 0 and "PREPARED_MODULE_MISMATCH" in text, text
                else:
                    assert result.returncode == 0 and 'symbol: "_ZN13prepared_demo7computeEi"' in text, text
                    assert ("falling back to DWARF" in text) == (mode != "off"), text
            assert callback.read_bytes() == original_callback, "generation changed the user callback"
            print("Native prepared integration passed: offline generation, ABI-compatible rebuild reuse, "
                  "pinned-image rejection, C++ API, typed results, restoration, and prefer/off.")
        finally:
            if console is not None and console.poll() is None:
                console.terminate()
                console.communicate(timeout=20)
            for process in (target, changed):
                if process is not None:
                    process.terminate()
                    process.wait(timeout=20)
            target_log.close()


if __name__ == "__main__":
    main()
