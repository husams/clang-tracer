#!/usr/bin/env python3
"""Exercise original-class field access through Call::self() in the public CLI."""

from __future__ import annotations

import argparse
import json
import re
import shlex
import signal
import subprocess
import tempfile
import time
from pathlib import Path

from class_interceptor_integration import callback_value, wait_binary


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def run(client: Path, endpoint: str, *args: str, ok: bool = True,
        timeout: float = 30) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run([str(client), "--endpoint", endpoint, *args],
                            capture_output=True, timeout=timeout, check=False)
    text = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    require((result.returncode == 0) == ok,
            f"CLI {' '.join(args)} returned {result.returncode}: {text}")
    return result


def text(result: subprocess.CompletedProcess[bytes]) -> str:
    return (result.stdout + result.stderr).decode("utf-8", errors="replace")


def field_value(row: str, name: str) -> int:
    match = re.search(r'object_fields\s*\{\s*name: "' + re.escape(name) +
                      r'"\s*value\s*\{\s*type: INT32(?:\s+signed_value:\s*(-?\d+))?', row)
    require(match is not None, f"missing typed receiver field {name}: {row}")
    return int(match.group(1)) if match.group(1) else 0


def stop(process: subprocess.Popen[bytes] | None) -> None:
    if process is not None and process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)


def short_wchar_compdb(compdb: Path, source: Path, output_dir: Path) -> Path:
    database = compdb / "compile_commands.json"
    require(database.is_file(), f"missing compile database: {database}")
    entries = json.loads(database.read_text(encoding="utf-8"))
    source = source.resolve()
    selected = next((entry for entry in entries
                     if (Path(entry["directory"]) / entry["file"]).resolve() == source), None)
    require(selected is not None, f"compile database has no entry for {source}")
    adjusted = dict(selected)
    if "arguments" in selected:
        adjusted["arguments"] = [*selected["arguments"], "-fshort-wchar"]
    elif "command" in selected:
        adjusted["command"] = shlex.join([*shlex.split(selected["command"]), "-fshort-wchar"])
    else:
        raise RuntimeError(f"compile database entry has no command or arguments: {selected}")
    short_dir = output_dir / "short-wchar-compdb"
    short_dir.mkdir(parents=True, exist_ok=True)
    (short_dir / "compile_commands.json").write_text(
        json.dumps([adjusted], indent=2) + "\n", encoding="utf-8")
    return short_dir


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("server", "client", "agent", "target", "unsupported-target",
                 "compdb", "source", "unsupported-source", "callback", "output-dir",
                 "protoc", "proto"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    for name in ("server", "client", "agent", "target", "unsupported_target", "compdb",
                 "source", "unsupported_source", "callback", "protoc", "proto"):
        path = getattr(args, name).resolve()
        require(path.exists(), f"missing {name}: {path}")
        setattr(args, name, path)
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    short_compdb = short_wchar_compdb(args.compdb, args.unsupported_source, output_dir)

    target_log_path = output_dir / "target.log"
    server_log_path = output_dir / "server.log"
    target_log = target_log_path.open("wb")
    server_log = server_log_path.open("wb")
    target = server = None
    try:
        with tempfile.TemporaryDirectory(prefix="clang-tracer-typed-this-") as socket_dir:
            endpoint = f"unix://{socket_dir}/server.sock"
            target = subprocess.Popen([str(args.target)], stdout=target_log,
                                      stderr=target_log)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if "TARGET pid=" in target_log_path.read_text(errors="replace"):
                    break
                require(target.poll() is None, "typed-this target exited during startup")
                time.sleep(.05)
            else:
                raise RuntimeError("typed-this target did not start")

            server = subprocess.Popen([str(args.server), "--agent", str(args.agent),
                                       "--listen", endpoint, "--timeout", "10"],
                                      stdout=server_log, stderr=server_log)
            ready = False
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                require(server.poll() is None, "server exited during startup")
                probe = subprocess.run([str(args.client), "--endpoint", endpoint, "list"],
                                       capture_output=True, timeout=3, check=False)
                if probe.returncode == 0:
                    ready = True
                    break
                time.sleep(.1)
            require(ready, "server did not become ready")

            attach = text(run(args.client, endpoint, "attach", str(target.pid)))
            require(re.search(r"state=ATTACHED", attach, re.I),
                    f"could not attach to target: {attach}")
            probe = text(run(args.client, endpoint, "trace", str(target.pid),
                             "Calculator::multiply(int, int) const", "--callback",
                             str(args.callback), timeout=45))
            match = re.search(r"\bprobe\s*=\s*(\d+)", probe)
            require(match is not None, f"trace output lacks probe ID: {probe}")
            probe_id = int(match.group(1))

            events, cursor = wait_binary(args.client, endpoint, target.pid, probe_id,
                                         output_dir, args.protoc, args.proto, 2, 0)

            by_call: dict[str, dict[str, str]] = {}
            for line in events:
                phase = re.search(r"^phase: (PRE|POST)$", line, re.M)
                call_id = re.search(r"^call_id:\s*(\d+)", line, re.M)
                require(phase and call_id, f"event lacks phase or call ID: {line}")
                by_call.setdefault(call_id.group(1), {})[phase.group(1).lower()] = line
            pair = next((entry for entry in by_call.values()
                         if "pre" in entry and "post" in entry), None)
            require(pair is not None, f"PRE and POST did not share a call ID: {events}")

            pre, post = pair["pre"], pair["post"]
            pre_args = re.findall(r"^arguments\s*\{\s*type: INT32\s+signed_value:\s*(-?\d+)", pre, re.M)
            post_args = re.findall(r"^arguments\s*\{\s*type: INT32\s+signed_value:\s*(-?\d+)", post, re.M)
            result = re.search(r"^result\s*\{\s*type: INT32\s+signed_value:\s*(-?\d+)", post, re.M)
            require(len(pre_args) == len(post_args) == 2 and result,
                    f"typed events lack arguments, callback values, or result: {pair}")
            value, factor = map(int, pre_args)
            require(tuple(map(int, post_args)) == (value, factor),
                    f"PRE and POST arguments differ: {pair}")
            original_result = int(result.group(1))
            calls_after = original_result - value * factor - 7
            require(calls_after > 0 and original_result == value * factor + 7 + calls_after,
                    f"original result does not reflect Calculator fields: {post}")
            require(field_value(pre, "bias") == 7 and field_value(pre, "calls") == calls_after - 1,
                    f"PRE did not read bias/calls and explicit arguments: {pre}")
            require(field_value(post, "bias") == 7 and field_value(post, "calls") == calls_after,
                    f"POST did not read bias/calls and the original result: {post}")
            (output_dir / "typed-this-events.log").write_text(
                f"PRE {pre}\nPOST {post}\n", encoding="utf-8")

            # Clear the valid method probe so rejection checks reach callback validation.
            run(args.client, endpoint, "remove", str(target.pid), str(probe_id))
            rejection_diagnostics: list[str] = []

            # Private fields must retain ordinary C++ access checking.
            private_hook = output_dir / "private-field.cpp"
            private_hook.write_text(
                "class Interceptor { public: void pre(const Call &call) { "
                "(void)call.self()->hidden; } };\n", encoding="utf-8")
            failed = run(args.client, endpoint, "trace", str(target.pid),
                         "Calculator::multiply(int, int) const", "--callback",
                         str(private_hook), ok=False, timeout=45)
            private_diagnostic = text(failed)
            rejection_diagnostics.append(f"private-field: {private_diagnostic}")
            require("hidden" in private_diagnostic and "private" in private_diagnostic.lower(),
                    f"private-field error did not identify the inaccessible field: {text(failed)}")

            # A callback may inspect self, but cannot invoke a target method or copy self.
            invalid_hooks = (
                ("mutate", "call.self()->bias = 123;"),
                ("invoke", "(void)call.self()->multiply(1, 2);"),
                ("copy", "auto copy = *call.self(); (void)copy;"),
                ("aggregate", "struct Holder { tracer::ResolvedTarget member; }; "
                            "Holder holder; (void)holder;"),
            )
            for label, body in invalid_hooks:
                hook = output_dir / f"{label}-self.cpp"
                hook.write_text(f"class Interceptor {{ public: void pre(const Call &call) {{ {body} }} }};\n",
                                encoding="utf-8")
                diagnostic = text(run(args.client, endpoint, "trace", str(target.pid),
                                      "Calculator::multiply(int, int) const", "--callback",
                                      str(hook), ok=False, timeout=45))
                rejection_diagnostics.append(f"{label}: {diagnostic}")
                if label == "mutate":
                    expected = re.search(r"read.only|const|assignment of read-only", diagnostic, re.I)
                elif label == "invoke":
                    expected = re.search(r"cannot call methods on.*receiver|no member named 'multiply'|"
                                         r"cannot call or take the address of imported receiver methods",
                                         diagnostic, re.I)
                elif label == "aggregate":
                    expected = re.search(r"cannot construct objects of the imported receiver type|"
                                         r"implicitly-deleted default constructor of 'Holder'",
                                         diagnostic, re.I)
                else:
                    expected = re.search(r"cannot construct objects of the imported receiver type",
                                         diagnostic, re.I)
                require(expected is not None,
                        f"{label} of self failed for an unexpected reason: {diagnostic}")

            # A free-function Call has no original class object.
            free_hook = output_dir / "free-self.cpp"
            free_hook.write_text(
                "class Interceptor { public: void pre(const Call &call) { "
                "(void)call.self(); } };\n", encoding="utf-8")
            free_error = text(run(args.client, endpoint, "trace", str(target.pid),
                                  "add(int, int)", "--callback", str(free_hook),
                                  ok=False, timeout=45))
            rejection_diagnostics.append(f"free-self: {free_error}")
            require("typed self() requires a debug-selected supported non-static method" in
                    free_error, f"free-function self failed for an unexpected reason: {free_error}")

            time.sleep(.3)
            target_text = target_log_path.read_text(encoding="utf-8", errors="replace")
            require(target_text.count("INITIALIZER fired") == 1,
                    "target translation-unit initializer ran more than once")
            rows = re.findall(r"^CALL init=(\d+) .*?bias=(\d+) calls=(\d+) .*?multiply=(-?\d+)$",
                              target_text, re.M)
            require(rows and all(init == "1" and bias == "7" for init, bias, _, _ in rows),
                    f"original initialization or target fields changed: {rows}")
            require(target.poll() is None, "target exited after callback validation")

            # A source fixture with a base class and a separate pointer field are rejected.
            inherited_log = (output_dir / "inheritance-target.log").open("wb")
            inherited = subprocess.Popen([str(args.unsupported_target)], stdout=inherited_log,
                                         stderr=inherited_log)
            try:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    inheritance_log = (output_dir / "inheritance-target.log").read_text(
                        encoding="utf-8", errors="replace")
                    if "TYPED_THIS_FIXTURE pid=" in inheritance_log:
                        break
                    require(inherited.poll() is None,
                            "inheritance fixture exited during startup")
                    time.sleep(.05)
                else:
                    raise RuntimeError("inheritance fixture did not start")
                attached = text(run(args.client, endpoint, "attach", str(inherited.pid)))
                require(re.search(r"state=ATTACHED", attached, re.I),
                        f"could not attach to inheritance fixture: {attached}")
                inheritance_error = text(run(
                    args.client, endpoint, "trace", str(inherited.pid),
                    "InheritedCalculator::read(int) const", "--callback", str(args.callback),
                    ok=False, timeout=45))
                rejection_diagnostics.append(f"inheritance: {inheritance_error}")
                require(re.search(r"debug receiver inheritance is unsupported|unsupported.*(?:receiver|inheritance|layout)|"
                                  r"receiver layout changed during AST import: receiver size or alignment changed since source selection|"
                                  r"typed self\(\) requires "
                                  r"a debug-selected supported non-static method",
                                  inheritance_error, re.I),
                        f"inheritance rejection was not clear: {inheritance_error}")

                # Reading the pointer through imported self must coexist with
                # standard declarations used by the target method body.
                pointer_hook = output_dir / "pointer-field.cpp"
                pointer_hook.write_text(
                    "#include <ctime>\n"
                    "class Interceptor { public: long long pre(const Call &call) { "
                    "return *call.self()->text; } };\n",
                    encoding="utf-8")
                pointer_probe_text = text(run(
                    args.client, endpoint, "trace", str(inherited.pid),
                    "PointerCalculator::read(int) const", "--callback", str(pointer_hook),
                    timeout=45))
                pointer_probe_match = re.search(r"\bprobe\s*=\s*(\d+)", pointer_probe_text)
                require(pointer_probe_match is not None,
                        f"pointer receiver probe did not install: {pointer_probe_text}")
                pointer_probe_id = int(pointer_probe_match.group(1))
                pointer_events, _ = wait_binary(args.client, endpoint, inherited.pid,
                                                pointer_probe_id, output_dir,
                                                args.protoc, args.proto, 1, 0)
                pointer_pre = next((line for line in pointer_events
                                    if re.search(r"^phase: PRE$", line, re.M)), None)
                require(pointer_pre is not None and
                        callback_value(pointer_pre) == 84,
                        f"pointer self callback did not read the original wchar_t value: "
                        f"{pointer_events}")
                run(args.client, endpoint, "remove", str(inherited.pid), str(pointer_probe_id))

                # Optional headers must preserve the actual DWARF ABI. The
                # donor source supplies flags only and is never parsed.
                pointer_header = output_dir / "pointer-layout.hpp"
                pointer_header.write_text(
                    "class PointerCalculator { public: wchar_t* text; int bias; "
                    "int read(int) const; };\n", encoding="utf-8")
                pointer_hook.write_text(
                    "class Interceptor { public: void pre(const Call &call) { "
                    "auto *self = call.self(); (void)self->text; } };\n",
                    encoding="utf-8")
                pointer_error = text(run(
                    args.client, endpoint, "trace", str(inherited.pid),
                    "PointerCalculator::read(int) const", "--callback", str(pointer_hook),
                    "--compdb", str(short_compdb), "--include", str(pointer_header),
                    ok=False, timeout=45))
                rejection_diagnostics.append(f"short-wchar-pointer: {pointer_error}")
                require("receiver field type ABI does not match DWARF" in pointer_error or
                        "header compiler command contains unsupported option: -fshort-wchar" in
                        pointer_error,
                        f"pointer field ABI mismatch failed for an unexpected reason: {pointer_error}")
                run(args.client, endpoint, "detach", str(inherited.pid), timeout=20)
                require(inherited.poll() is None,
                        "inheritance target did not remain alive after rejection/detach")
            finally:
                stop(inherited)
                inherited_log.close()

            # Reinstall a valid typed receiver probe and detach it while calls continue.
            final_probe = text(run(args.client, endpoint, "trace", str(target.pid),
                                   "Calculator::multiply(int, int) const", "--callback",
                                   str(args.callback), timeout=45))
            final_match = re.search(r"\bprobe\s*=\s*(\d+)", final_probe)
            require(final_match is not None, f"final trace output lacks probe ID: {final_probe}")
            final_probe_id = int(final_match.group(1))
            final_events = text(run(args.client, endpoint, "events", str(target.pid),
                                    "--count", "8", "--after", str(cursor), "--timeout", "5",
                                    timeout=8))
            require(re.search(rf"\bprobe={final_probe_id}\b.*\bphase=(pre|post)", final_events),
                    f"reinstalled typed-this probe emitted no events: {final_events}")
            before_detach = len(re.findall(r"^CALL ", target_log_path.read_text(
                encoding="utf-8", errors="replace"), re.M))
            run(args.client, endpoint, "detach", str(target.pid), timeout=20)
            time.sleep(.35)
            after_detach_text = target_log_path.read_text(encoding="utf-8", errors="replace")
            after_detach = len(re.findall(r"^CALL ", after_detach_text, re.M))
            require(target.poll() is None and after_detach > before_detach,
                    "target did not remain alive and make progress after typed-probe detach")
            (output_dir / "typed-this-rejections.txt").write_text(
                "\n---\n".join(rejection_diagnostics) + "\n", encoding="utf-8")
            print("typed this PRE/POST fields, privacy, callback restrictions, and live detach passed")
    finally:
        stop(target)
        stop(server)
        target_log.close()
        server_log.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
