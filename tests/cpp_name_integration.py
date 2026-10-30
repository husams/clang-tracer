#!/usr/bin/env python3
"""Exercise C++ declaration resolution through the public tracing CLI."""

from __future__ import annotations

import argparse
import json
import re
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from client_integration import decode_file, require, target_rows, wait_for_rows, write_text
from friendly_client_integration import FriendlyClient, check_events, has_field, save_output


def read_probe_id(text: str) -> int:
    match = re.search(r"(?i)\bprobe\s*=\s*(\d+)", text)
    require(match is not None, f"trace output lacks a probe ID:\n{text}")
    return int(match.group(1))


def trace(client: FriendlyClient, pid: int, declaration: str, callback: Path,
          *options: str, timeout: float = 30) -> subprocess.CompletedProcess[bytes]:
    return client.run("trace", str(pid), declaration, "--callback", str(callback),
                      *options, timeout=timeout)


def require_original_overloads(path: Path, pid: int) -> None:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    rows = []
    for line in lines:
        match = re.search(r"TARGET pid=(\d+).*?arg=(-?\d+).*?method=(-?\d+)\s+long_method=(-?\d+)",
                          line)
        if match:
            row_pid, argument, method, long_method = (int(value) for value in match.groups())
            require(row_pid == pid, f"target log reports unexpected PID: {line}")
            require(method == argument + 11 and long_method == argument * 2,
                    f"target returned incorrect original overload results: {line}")
            rows.append((argument, method, long_method))
    require(rows, "target log contains no original overload results")


def ensure_pre_rpc_rejection(client: FriendlyClient, pid: int, declaration: str,
                             callback: Path, options: tuple[str, ...], label: str,
                             output_dir: Path) -> str:
    unreachable = FriendlyClient(client.client, "unix:///no/such/clang-tracer.sock")
    result = unreachable.run("trace", str(pid), declaration, "--callback", str(callback),
                             *options, expected_ok=False, timeout=30)
    text = save_output(output_dir / f"reject-{label}.txt", result)
    require(text.strip(), f"{label} produced an empty error")
    require(not re.search(r"(?i)connection refused|unavailable|deadline exceeded|gRPC code",
                          text), f"{label} reached RPC before rejecting the declaration:\n{text}")
    return text


def ensure_resolution_reaches_rpc(client: FriendlyClient, pid: int, declaration: str,
                                  callback: Path, options: tuple[str, ...], label: str,
                                  output_dir: Path) -> str:
    unreachable = FriendlyClient(client.client, "unix:///no/such/clang-tracer.sock")
    result = unreachable.run("trace", str(pid), declaration, "--callback", str(callback),
                             *options, expected_ok=False, timeout=30)
    text = save_output(output_dir / f"resolve-{label}.txt", result)
    require(re.search(r"(?i)connection refused|unavailable|deadline exceeded|gRPC code", text),
            f"{label} did not resolve successfully before the expected RPC failure:\n{text}")
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--client", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--agent", required=True, type=Path)
    parser.add_argument("--protoc", required=True, type=Path)
    parser.add_argument("--proto", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--compdb", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--callback", required=True, type=Path)
    args = parser.parse_args()
    for name in ("server", "client", "target", "agent", "protoc", "proto", "compdb", "source",
                 "callback"):
        path = getattr(args, name).resolve()
        require(path.exists(), f"{name} does not exist: {path}")
        setattr(args, name, path)
    compdb_file = args.compdb / "compile_commands.json" if args.compdb.is_dir() else args.compdb
    require(compdb_file.is_file(), f"compile database file missing: {compdb_file}")

    repo = args.source.parent.parent
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    target_log_path = output_dir / "target.log"
    server_log_path = output_dir / "server.log"
    report_path = output_dir / "report.txt"
    report: list[str] = []
    target = None
    server = None
    target_log = None
    server_log = None

    try:
        with tempfile.TemporaryDirectory(prefix="clang-tracer-cpp-name-") as temp:
            temp_dir = Path(temp)
            endpoint = f"unix://{temp_dir}/server.sock"
            target_log = target_log_path.open("wb")
            target = subprocess.Popen([str(args.target)], cwd=repo,
                                      stdout=target_log, stderr=target_log)
            wait_for_rows(target_log_path, target.pid, 1)
            server_log = server_log_path.open("wb")
            server = subprocess.Popen(
                [str(args.server), "--agent", str(args.agent), "--listen", endpoint,
                 "--timeout", "10"], cwd=repo, stdout=server_log, stderr=server_log)
            client = FriendlyClient(args.client, endpoint)

            ready = False
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(server.poll() is None, "server exited before becoming ready")
                result = client.run("list", expected_ok=None, timeout=2)
                if result.returncode == 0:
                    ready = True
                    break
                time.sleep(0.1)
            require(ready, "could not connect to the owned tracing server")

            attach_text = save_output(output_dir / "attach.txt",
                                      client.run("attach", str(target.pid)))
            require(has_field(attach_text, "pid", target.pid) and
                    re.search(r"(?i)state=ATTACHED", attach_text),
                    f"attach did not report the owned target as attached:\n{attach_text}")
            callback = args.callback

            # The ordinary mode relies on the CLI's cwd/build compile database discovery.
            autodiscovery_dir = temp_dir / "autodiscovery"
            (autodiscovery_dir / "build").mkdir(parents=True)
            (autodiscovery_dir / "build" / "compile_commands.json").symlink_to(
                compdb_file
            )
            resolved_int = trace(
                FriendlyClient(args.client, endpoint), target.pid,
                "Fixture::overloaded(int)", callback,
                "--compdb", str(args.compdb), "--source", str(args.source), timeout=45)
            int_text = save_output(output_dir / "trace-overloaded-int.txt", resolved_int)
            int_probe = read_probe_id(int_text)
            require("Fixture::overloaded(int)" in int_text,
                    f"int overload output lacks its resolved declaration:\n{int_text}")
            symbol_match = re.search(r"\bsymbol=([^\s]+)", int_text)
            require(symbol_match is not None and symbol_match.group(1),
                    f"resolved trace did not report its native symbol:\n{int_text}")
            report.append("C++ declaration trace resolved the int overload from the compilation database")

            # An omitted --compdb must use cwd/build/compile_commands.json.
            auto_client = FriendlyClient(args.client, endpoint)
            auto_env_cwd = autodiscovery_dir
            auto_result = subprocess.run(
                [str(args.client), "--endpoint", endpoint, "trace", str(target.pid),
                 "Fixture::overloaded(long)", "--callback", str(callback), "--source",
                 str(args.source)], cwd=auto_env_cwd, capture_output=True, timeout=45,
                check=False)
            require(auto_result.returncode == 0,
                    "automatic cwd/build compilation database lookup failed: " +
                    auto_result.stderr.decode(errors="replace"))
            long_text = (auto_result.stdout + auto_result.stderr).decode("utf-8", errors="replace")
            save_output(output_dir / "trace-overloaded-long.txt",
                        subprocess.CompletedProcess([], 0, long_text.encode(), b""))
            long_probe = read_probe_id(long_text)
            require("Fixture::overloaded(long)" in long_text,
                    f"long overload was not resolved:\n{long_text}")
            require("symbol=" in long_text and symbol_match.group(1) not in long_text,
                    "overload resolution reused the int overload symbol:\n" + long_text)
            require_original_overloads(target_log_path, target.pid)

            double_callback = output_dir / "double-callback.cpp"
            write_text(double_callback,
                       'extern "C" long long trace_callback(const TraceContext& context) {\n'
                       '  return static_cast<long long>(context.arguments[0].real_value * 100 + 17);\n'
                       '}\n')
            inspect_result = trace(client, target.pid, "Fixture::inspect(double) const",
                                   double_callback, "--compdb", str(args.compdb), "--source",
                                   str(args.source), timeout=45)
            inspect_text = save_output(output_dir / "trace-inspect-const.txt", inspect_result)
            inspect_probe = read_probe_id(inspect_text)
            require("Fixture::inspect(double) const" in inspect_text,
                    f"const method output lacks its resolved declaration:\n{inspect_text}")

            # Rejections use an unreachable endpoint so any gRPC error proves resolution got too far.
            ambiguous = ensure_pre_rpc_rejection(
                client, target.pid, "Fixture::overloaded", callback,
                ("--compdb", str(args.compdb), "--source", str(args.source)),
                "ambiguous", output_dir)
            require("overloaded(int)" in ambiguous and "overloaded(long)" in ambiguous,
                    f"ambiguous overload error did not list candidates:\n{ambiguous}")
            missing = ensure_pre_rpc_rejection(
                client, target.pid, "Fixture::not_a_method(int)", callback,
                ("--compdb", str(args.compdb), "--source", str(args.source)),
                "missing-name", output_dir)
            require("not_a_method" in missing, f"missing-name error lacks the requested name:\n{missing}")
            mismatched = ensure_pre_rpc_rejection(
                client, target.pid, "Fixture::overloaded(int, int)", callback,
                ("--compdb", str(args.compdb), "--source", str(args.source)),
                "mismatched-parameters", output_dir)
            require("overloaded" in mismatched, "mismatched-parameter error lacks declaration context")
            bad_db = ensure_pre_rpc_rejection(
                client, target.pid, "Fixture::overloaded(int)", callback,
                ("--compdb", str(temp_dir / "missing-db"), "--source", str(args.source)),
                "missing-compdb", output_dir)
            require("compilation" in bad_db.lower() or "compile" in bad_db.lower() or
                    "database" in bad_db.lower(), f"missing database error is unclear:\n{bad_db}")

            # These synthetic translation units share the real compiler arguments from the
            # build database, but are never linked into or loaded by the traced target.
            db_entries = json.loads(compdb_file.read_text(encoding="utf-8"))
            template = next((entry for entry in db_entries
                             if Path(entry["file"]).resolve() == args.source), None)
            require(template is not None, f"compile database has no command for {args.source}")
            negative_source = repo / "tests" / "cpp_name_fixture.cpp"
            require(negative_source.is_file(), f"unsupported-type fixture missing: {negative_source}")
            compiler = (template.get("arguments") or ["clang++"])[0]
            command = [compiler, "-std=c++20", "-x", "c++", "-fsyntax-only",
                       str(negative_source)]
            entry = {"directory": str(repo), "file": str(negative_source),
                     "arguments": command}
            negative_db = temp_dir / "negative-compile-commands.json"
            write_text(negative_db, json.dumps([entry]))
            unsupported = ensure_pre_rpc_rejection(
                client, target.pid, "UnsupportedFixture::unsupported(UnsupportedType)", callback,
                ("--compdb", str(negative_db), "--source", str(negative_source)),
                "unsupported-type", output_dir)
            require("unsupported" in unsupported.lower() or "type" in unsupported.lower(),
                    f"unsupported type error lacks type context:\n{unsupported}")

            fixture_options = ("--compdb", str(negative_db), "--source", str(negative_source))
            ambiguous_virtual = ensure_pre_rpc_rejection(
                client, target.pid, "X::f", callback, fixture_options,
                "virtual-overload-ambiguous", output_dir)
            require("X::f(int)" in ambiguous_virtual and "X::f(long)" in ambiguous_virtual,
                    f"virtual overload candidate list is incomplete:\n{ambiguous_virtual}")
            virtual_exact = ensure_pre_rpc_rejection(
                client, target.pid, "X::f(int)", callback, fixture_options,
                "virtual-exact-unsupported", output_dir)
            require("virtual methods" in virtual_exact.lower() or "virtual" in virtual_exact.lower(),
                    f"exact virtual method was not rejected as unsupported:\n{virtual_exact}")
            volatile_mismatch = ensure_pre_rpc_rejection(
                client, target.pid, "VolatileOnly::v(int)", callback, fixture_options,
                "volatile-mismatch", output_dir)
            require("no callable matched" in volatile_mismatch.lower(),
                    f"non-volatile selector matched a volatile-only method:\n{volatile_mismatch}")
            ensure_resolution_reaches_rpc(
                client, target.pid, "VolatileOnly::v(int) volatile", callback,
                fixture_options, "volatile-exact", output_dir)
            unsigned_spelling = ensure_pre_rpc_rejection(
                client, target.pid, "UnsignedOnly::u(unsignedint)", callback, fixture_options,
                "unsigned-spelling", output_dir)
            require("no callable matched" in unsigned_spelling.lower(),
                    f"unsignedint spelling unexpectedly matched unsigned int:\n{unsigned_spelling}")

            # The x32 triple selects a 32-bit pointer ABI despite its x86_64 architecture.
            x32_entry = dict(entry)
            x32_entry["arguments"] = [*entry["arguments"], "-target",
                                      "x86_64-unknown-linux-gnux32"]
            x32_db = temp_dir / "x32-compile-commands.json"
            write_text(x32_db, json.dumps([x32_entry]))
            x32_error = ensure_pre_rpc_rejection(
                client, target.pid, "UnsignedOnly::u(unsigned int)", callback,
                ("--compdb", str(x32_db), "--source", str(negative_source)),
                "x32-pointer-abi", output_dir)
            require("linux x86_64" in x32_error.lower() or "32" in x32_error,
                    f"x32 pointer ABI rejection lacks target context:\n{x32_error}")

            malformed_source = temp_dir / "invalid.cpp"
            write_text(malformed_source, "class Fixture { void broken( ; };\n")
            malformed_entry = dict(entry)
            malformed_entry["file"] = str(malformed_source)
            if "arguments" in malformed_entry:
                malformed_entry["arguments"] = [str(malformed_source) if x == str(negative_source) else x
                                                 for x in malformed_entry["arguments"]]
            else:
                malformed_entry["command"] = malformed_entry["command"].replace(
                    str(negative_source), str(malformed_source))
            malformed_db = temp_dir / "invalid-compile-commands.json"
            write_text(malformed_db, json.dumps([malformed_entry]))
            parse_error = ensure_pre_rpc_rejection(
                client, target.pid, "Fixture::broken", callback,
                ("--compdb", str(malformed_db), "--source", str(malformed_source)),
                "parse-error", output_dir)
            require("error" in parse_error.lower() or "expected" in parse_error.lower(),
                    f"invalid syntax did not produce a parser diagnostic:\n{parse_error}")
            report.append("ambiguous, missing, mismatched, unsupported, missing-database and parse errors were rejected before RPC")
            report.append("virtual ambiguity/rejection, volatile qualification, unsigned spelling and x32 ABI regressions passed")

            # Use the existing real callback with signed integral arguments for both overloads.
            event_client = FriendlyClient(args.client, endpoint)
            observed: dict[int, str] = {}
            cursor = 0
            deadline = time.monotonic() + 20
            while len(observed) < 3 and time.monotonic() < deadline:
                remaining = max(1, min(5, int(deadline - time.monotonic())))
                result = event_client.run("events", str(target.pid), "--count", "1",
                                          "--after", str(cursor), "--timeout", str(remaining),
                                          timeout=remaining + 3)
                batch = (result.stdout + result.stderr).decode("utf-8", errors="replace")
                for line in batch.splitlines():
                    cursor_match = re.search(r"\bcursor=(\d+)", line)
                    if cursor_match:
                        cursor = max(cursor, int(cursor_match.group(1)))
                    probe = re.search(r"\bprobe=(\d+)", line)
                    if probe and int(probe.group(1)) in {int_probe, long_probe, inspect_probe}:
                        observed[int(probe.group(1))] = line
            require({int_probe, long_probe, inspect_probe} <= observed.keys(),
                    f"did not receive events for all name-resolved probes: {observed}")
            int_line = observed[int_probe]
            long_line = observed[long_probe]
            inspect_line = observed[inspect_probe]
            check_events(int_line, target.pid, {int_probe})
            require(has_field(long_line, "pid", target.pid) and
                    has_field(long_line, "probe", long_probe),
                    f"long overload event has the wrong PID or probe:\n{long_line}")
            long_argument = re.search(r"args=\[int64:(-?\d+)", long_line)
            long_callback = re.search(r"\bcallback=(-?\d+)", long_line)
            require(long_argument is not None and long_callback is not None,
                    f"long overload event lacks int64 argument/callback:\n{long_line}")
            require(int(long_callback.group(1)) == int(long_argument.group(1)) * 100 + 17,
                    f"long overload callback disagrees with its argument:\n{long_line}")
            require(re.search(r"\bthis=0x(?!0+$)[0-9a-f]+", int_line, re.I),
                    f"int member event lacks an object address: {int_line}")
            require(re.search(r"\bthis=0x(?!0+$)[0-9a-f]+", long_line, re.I),
                    f"long member event lacks an object address: {long_line}")
            require(re.search(r"\bthis=0x(?!0+$)[0-9a-f]+", inspect_line, re.I),
                    f"const member event lacks an object address: {inspect_line}")
            require("args=[float64:" in inspect_line.lower(),
                    f"const method event does not contain its inferred double argument:\n{inspect_line}")
            double_argument = re.search(r"args=\[float64:([-+0-9.eE]+)", inspect_line)
            double_callback_value = re.search(r"\bcallback=(-?\d+)", inspect_line)
            require(double_argument is not None and double_callback_value is not None,
                    f"const method event lacks its argument or callback value:\n{inspect_line}")
            require(int(double_callback_value.group(1)) == int(float(double_argument.group(1)) * 100 + 17),
                    f"double callback did not use the inferred argument:\n{inspect_line}")
            report.append("readable events carried typed arguments, callback values and live object addresses")

            # Reinstall in binary-output mode only after restoring the first probes.
            for probe_id in (int_probe, long_probe, inspect_probe):
                removed = save_output(output_dir / f"remove-{probe_id}.txt",
                                      client.run("remove", str(target.pid), str(probe_id)))
                require(has_field(removed, "restored", "true"),
                        f"remove did not restore probe {probe_id}:\n{removed}")
            binary_probes: set[int] = set()
            signature_cases = (
                ("overloaded-int", "Fixture::overloaded(int)", callback, "INT32", "INT32"),
                ("overloaded-long", "Fixture::overloaded(long)", callback, "INT32", "INT64"),
                ("inspect-const", "Fixture::inspect(double) const", double_callback,
                 "FLOAT64", "FLOAT64"),
            )
            for label, declaration, callback_file, result_type, argument_type in signature_cases:
                binary_path = output_dir / f"trace-{label}.pb"
                binary_result = subprocess.run(
                    [str(args.client), "--endpoint", endpoint, "--format", "protobuf",
                     "--output", str(binary_path), "trace", str(target.pid), declaration,
                     "--callback", str(callback_file), "--compdb", str(args.compdb),
                     "--source", str(args.source)], cwd=repo,
                    capture_output=True, timeout=45, check=False)
                require(binary_result.returncode == 0,
                        f"protobuf trace for {label} failed: " +
                        binary_result.stderr.decode(errors="replace"))
                decoded = decode_file(args.protoc, args.proto, "Probe", binary_path)[0]
                require("kind: METHOD" in decoded and f"result: {result_type}" in decoded and
                        f"arguments: {argument_type}" in decoded,
                        f"binary Probe response for {label} has the wrong inferred signature:\n{decoded}")
                binary_probe = re.search(r"^id:\s*(\d+)\s*$", decoded, re.M)
                require(binary_probe is not None, f"binary Probe for {label} lacks ID:\n{decoded}")
                binary_probes.add(int(binary_probe.group(1)))
            report.append("decoded protobuf Probes confirmed METHOD signatures for int32, int64 and float64 arguments")

            # Remove one probe; detach must restore the remaining active methods.
            for probe_id in sorted(binary_probes)[:1]:
                removed = save_output(
                    output_dir / f"remove-{probe_id}.txt",
                    client.run("remove", str(target.pid), str(probe_id)))
                require(has_field(removed, "probe", probe_id) and
                        has_field(removed, "restored", "true"),
                        f"remove did not restore probe {probe_id}:\n{removed}")
            detached = save_output(output_dir / "detach.txt",
                                   client.run("detach", str(target.pid)))
            require(has_field(detached, "pid", target.pid),
                    f"detach output lacks target PID:\n{detached}")
            for probe_id in binary_probes:
                require(f"{probe_id}:restored=true" in detached,
                        f"detach did not restore probe {probe_id}:\n{detached}")
            previous = len(target_rows(target_log_path, target.pid))
            rows = wait_for_rows(target_log_path, target.pid, previous + 2)
            require(target.poll() is None and rows[-1][1] == rows[-1][0] * 3 + 7,
                    "target stopped or did not return its original free-function result after detach")
            require_original_overloads(target_log_path, target.pid)
            report.append("remove and detach restored patched code; target kept returning original results")

            server.send_signal(signal.SIGTERM)
            require(server.wait(timeout=15) == 0, "owned server did not exit cleanly")
            report.append("C++ name-resolution integration passed")
            return 0
    finally:
        if server is not None and server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=3)
        if target is not None and target.poll() is None:
            target.send_signal(signal.SIGTERM)
            try:
                target.wait(timeout=3)
            except subprocess.TimeoutExpired:
                target.kill()
                target.wait(timeout=3)
        if target_log is not None:
            target_log.close()
        if server_log is not None:
            server_log.close()
        write_text(report_path, "\n".join(report) + ("\n" if report else ""))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"C++ name-resolution integration failed: {error}", file=sys.stderr)
        sys.exit(1)
