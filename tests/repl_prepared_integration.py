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
        sysroot = root / "sdk"
        sdk_include = sysroot / "usr" / "include"
        sdk_include.mkdir(parents=True)
        (sdk_include / "prepared_sdk.h").write_text("#define PREPARED_SDK_HEADER 1\n")
        local_header.write_text(
            '#ifndef FIRST_DIRECTORY_ENTRY\n#error wrong compile command\n#endif\n'
            '#include <prepared_sdk.h>\n'
            '#if PREPARED_SDK_HEADER != 1\n#error wrong sysroot\n#endif\n'
            f'#include "{header}"\n')
        default_compdb.write_text(json.dumps([
            {"directory": str(root), "file": source,
             "arguments": ["clang++", "-std=c++17", *flags, "-c", source]}
            for source, flags in (
                ("foreign.cpp", []),
                ("headers/z_first.cpp", ["-DFIRST_DIRECTORY_ENTRY", f"--sysroot={sysroot}"]),
                ("headers/a_second.cpp", []),
            )
        ]))
        package = root / "prepared"
        target_log = (root / "target.log").open("w")
        target = None
        changed = None
        console = None
        try:
            def prepare(include, output):
                command = f'%probe prepare {selector} --include "{include}" --output "{output}"'
                return subprocess.run([args.repl, "--fail-fast"], input=command + "\n%quit\n",
                                      text=True, capture_output=True, cwd=root, env=env, timeout=30)

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

            offline = prepare(local_header, package)
            assert offline.returncode == 0 and "prepared directory=" in offline.stdout, (
                offline.stdout + offline.stderr)

            cl_header = root / "headers" / "cl.hpp"
            cl_header.write_text(
                '#if !defined(_MT) || !defined(PREPARE_CL_MODE)\n#error expected clang-cl mode\n#endif\n'
                '#if __cplusplus < 202002L\n#error expected C++20 from the compile command\n#endif\n'
                '#include <prepared_sdk.h>\n'
                f'#include "{header}"\n')
            source = "headers/cl.cpp"
            cl_flags = ["/TP", "/c", "/Foignored.obj", "/std:c++20", "/DPREPARE_CL_MODE", source]
            for name, arguments in (
                ("cl_linux", ["clang-cl", "--target=x86_64-unknown-linux-gnu",
                              f"--sysroot={sysroot}", *cl_flags]),
                ("cl_windows", ["clang-cl", *cl_flags]),
                ("gnu_x32", ["clang++", "-mx32", "-c", source]),
            ):
                default_compdb.write_text(json.dumps([
                    {"directory": str(root), "file": source, "arguments": arguments},
                ]))
                output = root / name
                result = prepare(cl_header, output)
                text = result.stdout + result.stderr
                if name == "cl_linux":
                    assert result.returncode == 0 and (output / "prepared.pb").is_file(), text
                    assert not (root / "ignored.obj").exists()
                else:
                    assert result.returncode != 0, text
                    assert "compilation database ABI does not match the trace target" in text, text
                    assert not (output / "prepared.pb").exists()

            gcc_header = root / "headers" / "gcc.hpp"
            gcc_header.write_text(
                '#ifdef PREPARE_TEST_GCC_FLAGS\n'
                '#ifndef __CHAR_UNSIGNED__\n#error expected -funsigned-char\n#endif\n'
                '#warning prepare test warning\n#endif\n'
                f'#include "{header}"\n')
            bad_header = root / "headers" / "bad.hpp"
            bad_header.write_text(f'#error prepare test parser error\n#include "{header}"\n')
            source = "headers/gcc.cpp"
            gcc_flags = ["-oignored.o", "-DPREPARE_TEST_GCC_FLAGS", "-MFignored.d", "-funsigned-char"]
            for name, include, flags, diagnostic in (
                ("gcc_flags", gcc_header, gcc_flags, None),
                ("driver_error", gcc_header, ["-fclang-tracer-invalid-test-flag"], "error: unknown argument"),
                ("invalid_standard", gcc_header, ["-std=tracer-invalid-standard"], "error: invalid value"),
                ("parser_error", bad_header, [], "error: prepare test parser error"),
                ("unsafe_flag", gcc_header, ["-Xclang", "-load", "-Xclang", "unused.so"],
                 "header compilation command contains unsupported option: -Xclang"),
                ("unsafe_log", gcc_header, ["-diagnostic-log-file", "ignored.log"],
                 "header compilation command contains unsupported option: -diagnostic-log-file"),
            ):
                default_compdb.write_text(json.dumps([{
                    "directory": str(root), "file": source,
                    "arguments": ["g++", *flags, "-MMD", "-MF", "ignored.d", "-c", source,
                                  "-o", "ignored.o"],
                }]))
                output = root / name
                result = prepare(include, output)
                text = result.stdout + result.stderr
                if diagnostic is None:
                    assert result.returncode == 0 and (output / "prepared.pb").is_file(), text
                    assert "warning: prepare test warning" in text, text
                else:
                    assert result.returncode != 0 and diagnostic in text, text
                    assert not output.exists(), text
                assert not (root / "ignored.d").exists()
                assert not (root / "ignored.o").exists()
                assert not (root / "ignored.log").exists()

            target = subprocess.Popen([args.target], stdout=target_log, stderr=subprocess.STDOUT)
            prepared = f'--prepared "{package}" --prepared-mode require'
            validate = f'%probe validate {selector} --callback "{callback}" {prepared}\n'
            prefix = (
                f'%probe resolve {selector} {prepared}\n'
                f'{validate}'
                f'%probe inject {selector} --callback "{callback}" {prepared}\n'
            )
            suffix = '''%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
bool correct = captured.size() == 4;
for (const auto& e : captured) { correct = correct && e.result_as<int>() == e.argument_as<int>(0) * 3 + 7; }
std::cout << "PREPARED_TYPED_RESULTS=" << correct << std::endl;
%probe remove PROBE_ID
%quit
'''
            # Validation must recognize the original bytes under a live detour
            # even when the target image has no build ID.
            text = interact(prefix, validate + suffix, target)
            assert text.count("prepared_used: true") == 2, text
            assert "format_version: 2" in text and "module_sha256:" not in text, text
            assert "PREPARED_TYPED_RESULTS=1" in text, text
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
            print("Native prepared integration passed: compile database driver, sysroot, and Clang diagnostics, "
                  "ABI rejection, offline generation, ABI-compatible rebuild reuse, pinned-image rejection, "
                  "C++ API, typed results, restoration, and prefer/off.")
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
