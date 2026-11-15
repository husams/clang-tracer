"""Exercise offline object-probe packages, method identities, and rollback."""

from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time


class Output:
    def __init__(self, stream, timeout):
        self.lines = queue.Queue()
        self.seen = []
        self.timeout = timeout

        def read():
            for line in stream:
                self.lines.put(line)
            self.lines.put(None)

        threading.Thread(target=read, daemon=True).start()

    def expect(self, pattern):
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                raise AssertionError((pattern, "".join(self.seen[-30:])))
            self.seen.append(line)
            if match := re.search(pattern, line):
                return match


def send(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def run(args):
    with tempfile.TemporaryDirectory(prefix="ct-type-object-probe-") as temporary, ExitStack() as cleanup:
        root = Path(temporary).resolve()
        build = root / "build-context"
        generated = build / "generated"
        generated.mkdir(parents=True)
        source = root / "counter.cpp"
        header = root / "counter.hpp"
        driver = root / "driver.cpp"
        callbacks = root / "callbacks.cpp"
        bad_callbacks = root / "rollback-callbacks.cpp"
        compdb = build / "compile_commands.json"
        library = build / "libcounter.so"
        target = build / "counter-target"
        package = root / "counter.object"
        bad_package = root / "rollback.object"
        fresh_package = root / "fresh.object"
        overlap = root / "overlap.object"
        ordinary_callback = root / "ordinary.cpp"
        ordinary_callback.write_text('extern "C" long long trace_callback(const TraceContext&) { return 7; }\n')
        hidden = root / "hidden-inputs"
        hidden.mkdir()

        (generated / "counter_build_config.hpp").write_text(
            "#pragma once\n#define COUNTER_PAD_BYTES 13\n")
        header.write_text("""#pragma once
#include <cstddef>
#include \"counter_build_config.hpp\"
namespace demo {
class Counter {
public:
  explicit Counter(int initial);
  ~Counter();
  int add(int value);
  int store(int value);
  bool accepted();
  int value() const;
private:
  char padding_[COUNTER_PAD_BYTES];
  int value_;
};
}
""")
        source.write_text("""#include \"counter.hpp\"
namespace demo {
Counter::Counter(int initial) : value_(initial) {}
Counter::~Counter() { value_ = -1; }
int Counter::add(int value) { return value_ + value; }
int Counter::store(int value) { value_ = value; return value_; }
bool Counter::accepted() { return value_ >= 0; }
int Counter::value() const { return value_; }
}
""")
        driver.write_text("""#include \"counter.hpp\"
#include <iostream>
#include <new>
#include <string>
#include <cerrno>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
namespace {
alignas(demo::Counter) unsigned char storage[sizeof(demo::Counter)];
void cycle(int initial) {
  auto* counter = new (storage) demo::Counter(initial);
  counter->store(initial + 1);
  counter->add(2);
  counter->~Counter();
}
}
int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  demo::Counter existing(7);
  std::cout << \"READY\" << std::endl;
  for (;;) {
    char command = 0;
    const auto size = ::read(STDIN_FILENO, &command, 1);
    if (size == 0)
      return 0;
    if (size < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    if (command == 'x') {
      cycle(10);
      cycle(20);
      std::cout << \"CYCLED\" << std::endl;
    } else if (command == 'a') {
      cycle(30);
      std::cout << \"ADDED\" << std::endl;
    } else if (command == 'p') {
      std::cout << \"EXISTING=\" << existing.add(2) << std::endl;
    } else if (command == 'q') {
      return 0;
    }
  }
}
""")
        callbacks.write_text("""template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call& call) {
    call.field(\"probe_id\", call.probe_id());
    call.field(\"call_id\", call.id());
    call.field(\"object_id\", call.object_id());
    call.field(\"object_generation\", call.object_generation());
    call.field(\"identity_verified\", call.identity_verified());
  }
};
""")
        bad_callbacks.write_text("""#include <type_traits>
template <> class Interceptor<tracer::ResolvedSignature> {
  static_assert(!std::is_same_v<tracer::ResolvedSignature, bool()>,
                \"intentional later method compile failure\");
public:
  void post(const Call& call) { call.field(\"callback\", 1); }
};
""")

        compile_args = [str(args.cxx), "-std=c++20", "-g", "-O0", "-fPIC", "-fno-inline",
                        "-fpatchable-function-entry=8",
                        f"-I{generated}", f"-I{root}", "-DCOUNTER_PAD_BYTES=13",
                        "-c", str(source), "-o", str(build / "counter.o")]
        compdb.write_text(json.dumps([{
            "directory": str(build),
            "file": str(source),
            "arguments": compile_args,
        }], indent=2) + "\n")

        def child(argv, *, stdin=subprocess.PIPE, stdout=subprocess.PIPE, env=None):
            process = subprocess.Popen(argv, cwd=root, stdin=stdin, stdout=stdout,
                                       stderr=subprocess.STDOUT, text=True, env=env)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 45)

        env = dict(__import__("os").environ, NO_COLOR="1")
        env.pop("CLANG_TRACER_ENDPOINT", None)
        repl_argv = [str(args.repl), "--server", str(args.server), "--agent", str(args.agent)]

        def quote(value):
            return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'

        def prepare_commands(methods, callback_path, output):
            selected = "".join(f" --method {quote(method)}" for method in methods)
            return (f"%object-probe prepare demo::Counter{selected}"
                    f" --include {quote(header)} --compdb {quote(compdb)}"
                    f" --translation-unit {quote(source)} --callbacks {quote(callback_path)}"
                    f" --output {quote(output)}")

        methods = ("demo::Counter::add(int)",)
        writer = (f" --field value_ --code {quote('demo::Counter::store(int)')}"
                  )
        before_build = (library.exists(), target.exists())
        assert before_build == (False, False), before_build

        # Prepare all source inputs before the target ELF exists.
        repl, out = child(repl_argv, stdout=subprocess.PIPE, env=env)
        send(repl, prepare_commands(("demo::Counter::add(int)", "demo::Counter::accepted()"),
                                   bad_callbacks, bad_package) + writer)
        bad_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not bad_prepared.string.startswith("error:"), bad_prepared.string
        send(repl, prepare_commands(methods, callbacks, package) + writer)
        prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not prepared.string.startswith("error:"), (prepared.string, "".join(out.seen[-30:]))
        send(repl, prepare_commands(methods, callbacks, fresh_package))
        fresh_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not fresh_prepared.string.startswith("error:"), fresh_prepared.string
        send(repl, prepare_commands(("demo::Counter::store(int)",), callbacks, overlap) + writer)
        out.expect(r"^object-probe package=.*overlap.object")
        for path in (package, bad_package, fresh_package, overlap):
            assert path.is_file() and path.read_bytes().startswith(b"CTKOPKG2"), path
        saved = package.read_bytes()
        obsolete = root / "obsolete.object"
        obsolete_payload = b"CTKOPKG1" + saved[8:-32]
        obsolete.write_bytes(obsolete_payload + hashlib.sha256(obsolete_payload).digest())
        wrong_version = root / "wrong-version.object"
        # The first protobuf field encodes the version as a varint.
        assert saved[8:10] == b"\x08\x01"
        wrong_version_payload = saved[:8] + b"\x08\x02" + saved[10:-32]
        wrong_version.write_bytes(wrong_version_payload + hashlib.sha256(wrong_version_payload).digest())
        send(repl, prepare_commands(methods, callbacks, package) + writer)
        out.expect(r"^error: .*package already exists")
        assert package.read_bytes() == saved
        send(repl, "%quit")
        assert repl.wait(timeout=10) == 1, "".join(out.seen[-15:])

        # Compile the ELF only after offline preparation, then hide every source
        # input needed by preparation. Runtime preview must use the package.
        subprocess.run(compile_args, cwd=build, check=True)
        subprocess.run([str(args.cxx), "-shared", str(build / "counter.o"), "-o", str(library)],
                       cwd=build, check=True)
        subprocess.run([str(args.cxx), "-std=c++20", "-g", "-O0", "-fno-inline",
                        "-fpatchable-function-entry=8",
                        f"-I{generated}", f"-I{root}", "-DCOUNTER_PAD_BYTES=13",
                        str(driver), "-L", str(build), "-Wl,-rpath," + str(build),
                        "-lcounter", "-o", str(target)], cwd=build, check=True)
        nm = subprocess.run(["nm", "-D", "-C", "--defined-only", str(library)],
                            check=True, text=True, stdout=subprocess.PIPE).stdout
        for selector in ("demo::Counter::Counter(int)", "demo::Counter::~Counter()",
                         *methods, "demo::Counter::store(int)", "demo::Counter::accepted()"):
            assert selector in nm, (selector, nm)
        dwarf = subprocess.run(["readelf", "--debug-dump=info", "--wide", str(library)],
                               check=True, text=True, stdout=subprocess.PIPE).stdout
        assert "Counter" in dwarf and "value_" in dwarf
        for path in (header, source, callbacks, bad_callbacks, generated / "counter_build_config.hpp"):
            shutil.move(path, hidden / path.name)
            assert not path.exists()

        target_hashes = {path: hashlib.sha256(path.read_bytes()).digest() for path in (target, library)}
        run_env = dict(env, LD_LIBRARY_PATH=str(build))
        target_process, app = child([str(target)], stdin=subprocess.PIPE, env=run_env)
        app.expect(r"^READY$")
        repl, out = child(repl_argv, env=env)
        send(repl, f"%attach {target_process.pid}")
        out.expect(rf"^attached session=\S+ pid={target_process.pid}$")

        binding = f"--package {quote(package)} --module ./build-context/libcounter.so"
        send(repl, f"%object-probe preview --package {quote(obsolete)}")
        out.expect(r"^error: .*unsupported object-probe package format")
        send(repl, f"%object-probe preview --package {quote(wrong_version)}")
        out.expect(r"^error: .*unsupported object-probe package version")
        send(repl, f"%object-probe preview {binding} --first-seen")
        out.expect(r"^error: .*--first-seen is supported for write-only object-probe packages")
        send(repl, f'%probe inject "demo::Counter::value() const" --callback {quote(ordinary_callback)} '
                   '--module libcounter.so')
        ordinary_id = out.expect(r"^installed probe=(\d+)")[1]
        send(repl, f"%object-probe preview {binding}")
        out.expect(r"^error: .*remove the session's native probes")
        send(repl, f"%object-probe start {binding}")
        out.expect(r"^error: .*remove the session's native probes")
        send(repl, f"%probe remove {ordinary_id}")
        out.expect(rf"^removed probe={ordinary_id} restored=true")
        send(repl, f"%object-probe preview --package {quote(overlap)} --module libcounter.so")
        out.expect(r"^error: .*overlaps an object lifecycle or writer")

        # Prove the combined lifecycle, field writer, and typed method path.
        send(repl, f"%object-probe preview {binding}")
        preview = out.expect(r"^(?:object-probe preview|binding=|error:)")
        assert not preview.string.startswith("error:"), preview.string
        out.expect(r"demo::Counter::add")
        send(repl, f"%object-probe start {binding}")
        started = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")
        registration = started[1]
        method_probe = out.expect(r"method_probe=(\d+) active=yes")
        send(repl, f"%object-probe preview {binding}")
        out.expect(r"^showing the verified binding of the active registration$")
        send(repl, "%record counter.db --sqlite --count 10 --after 0 --timeout 10")
        target_process.stdin.write("x\n")
        target_process.stdin.flush()
        app.expect(r"^CYCLED$")
        received = out.expect(r"^received=(\d+) cursor=(\d+) .*complete=true .*recording=counter.db")
        assert int(received[1]) == 10, "expected ten lifecycle, field-write, and method callback events"
        cursor = int(received[2])
        send(repl, f"%object-probe status {registration}")
        out.expect(rf"^object-probe registration={registration} active=yes .*observations=[1-9]\d* ")
        send(repl, f"%object-probe stop {registration}")
        out.expect(rf"^object-probe registration={registration} active=no .*restoration_pending=no")
        send(repl, "%events open counter.db --as counter")
        out.expect(r"^opened events=\d+ retained=\d+ evicted=0$")
        send(repl, "%events export counter --format json --output counter.json")

        # A shared callback source passes the first signature and intentionally
        # fails for the second. A failed group must leave no first-method hook.
        bad_binding = f"--package {quote(bad_package)} --module libcounter.so"
        send(repl, f"%object-probe start {bad_binding}")
        failed = out.expect(r"^(?:error:|object-probe registration=\d+ active=no)")
        diagnostic = failed.string
        if not any(token in diagnostic.lower() for token in ("intentional later", "signature", "static assertion")):
            diagnostic += out.expect(r"(?i)(?:intentional later method compile failure|signature|static assertion)").string
        assert any(token in diagnostic.lower() for token in ("intentional later", "signature", "static assertion")), diagnostic
        send(repl, f"%record failed.db --sqlite --count 1 --after {cursor} --timeout 2")
        target_process.stdin.write("a\n")
        target_process.stdin.flush()
        app.expect(r"^ADDED$")
        failed_record = out.expect(r"^received=(\d+) cursor=(\d+) .*timed_out=true .*recording=failed.db")
        assert int(failed_record[1]) == 0, "callbacks remained active after failed group start"

        # A valid fresh registration immediately after rollback proves recovery
        # on this same PID and emits a callback event filtered by its probe ID.
        fresh_binding = f"--package {quote(fresh_package)}"
        send(repl, f"%object-probe preview {fresh_binding}")
        preview = out.expect(r"^(?:object-probe preview|binding=|error:)")
        assert not preview.string.startswith("error:"), preview.string
        out.expect(r"demo::Counter::add")
        send(repl, f"%object-probe start {fresh_binding}")
        fresh = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")
        fresh_registration = fresh[1]
        fresh_probe = out.expect(r"method_probe=(\d+) active=yes")
        send(repl, f"%record provisional.db --sqlite --probe {fresh_probe[1]} --count 1 "
                   f"--after {failed_record[2]} --timeout 5")
        send(target_process, "p")
        app.expect(r"^EXISTING=9$")
        provisional = out.expect(r"^received=1 cursor=(\d+) .*complete=true .*recording=provisional.db")
        send(repl, "%events open provisional.db --as provisional")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        send(repl, "%events export provisional --format json --output provisional.json")
        send(repl, f"%record fresh.db --sqlite --probe {fresh_probe[1]} --count 1 "
                   f"--after {provisional[1]} --timeout 5")
        target_process.stdin.write("a\n")
        target_process.stdin.flush()
        app.expect(r"^ADDED$")
        fresh_record = out.expect(r"^received=(\d+) cursor=(\d+) .*complete=true .*recording=fresh.db")
        assert int(fresh_record[1]) == 1
        send(repl, "%events open fresh.db --as fresh")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        fresh_json = root / "fresh.json"
        send(repl, f"%events export fresh --format json --output {quote(fresh_json)}")
        send(repl, f"%object-probe stop {fresh_registration}")
        out.expect(rf"^object-probe registration={fresh_registration} active=no .*restoration_pending=no")
        send(repl, f"%object-probe start {fresh_binding}")
        detached_id = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")[1]
        send(repl, "%detach")
        out.expect(r"^detached session=")
        out.expect(rf"^object-probe registration={detached_id} active=no restoration_pending=no .*session detached")

        send(repl, f"%attach {target_process.pid}")
        out.expect(rf"^attached session=\S+ pid={target_process.pid}$")
        send(repl, f"%object-probe start {fresh_binding}")
        exit_id = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")[1]
        send(repl, f"%record exit.db --sqlite --probe {exit_id} --count 1 --timeout 5")
        send(target_process, "q")
        assert target_process.wait(timeout=10) == 0
        out.expect(r"^received=1 .*complete=true .*recording=exit.db")
        send(repl, f"%object-probe status {exit_id}")
        out.expect(rf"^object-probe registration={exit_id} active=no .*restoration_pending=no .*target exited")
        send(repl, "%events open exit.db --as target_exit")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        send(repl, "%events export target_exit --format json --output exit.json")
        send(repl, "%quit")
        repl_status = repl.wait(timeout=10)
        assert repl_status == 1, (repl_status, "".join(out.seen[-15:]))
        closure = json.loads((root / "exit.json").read_text())[0]["objectObservation"]
        assert closure["kind"] == "OBSERVATION_ENDED", closure
        assert closure["closureReason"] == "TARGET_EXITED", closure

        fresh_events = json.loads(fresh_json.read_text())
        assert len(fresh_events) == 1, fresh_events
        fresh_event = fresh_events[0]
        assert str(fresh_event["probeId"]) == fresh_probe[1], fresh_event
        fresh_fields = {field.get("name"): field.get("value")
                        for field in fresh_event.get("objectFields", [])}
        fresh_call_id = fresh_fields["call_id"]
        if isinstance(fresh_call_id, dict):
            fresh_call_id = next((fresh_call_id[key] for key in
                                  ("unsignedValue", "signedValue", "boolValue", "stringValue")
                                  if key in fresh_call_id), None)
        assert str(fresh_call_id) == str(fresh_event["callId"]), fresh_event

        events = json.loads((root / "counter.json").read_text())
        assert all(event.get("objectProbeRegistrationId") == registration for event in events), events
        times = [int(event["monotonicTimeNs"]) for event in events]
        assert times == sorted(times), times
        observations = [event["objectObservation"] for event in events if "objectObservation" in event]
        created = [event for event in events if event.get("objectObservation", {}).get("kind") == "CREATED"]
        ended = [event for event in events if event.get("objectObservation", {}).get("kind") == "TEARDOWN_END"]
        writes = [event["objectObservation"] for event in events
                  if event.get("objectObservation", {}).get("kind") == "FIELD_CHANGED"]
        assert len(created) == len(ended) == 2, observations
        assert len({event["objectAddress"] for event in created}) == 1, created
        identities = {(int(event["objectObservation"]["lifetimeId"]),
                       int(event["objectObservation"]["generation"])) for event in created}
        assert len(identities) == 2, identities
        created_at = {
            (int(event["objectObservation"]["lifetimeId"]),
             int(event["objectObservation"]["generation"])): events.index(event)
            for event in created
        }
        assert [(event["oldInteger"], event["newInteger"]) for event in writes] == [(10, 11), (20, 21)], writes
        calls = [event for event in events if event.get("functionName", "").startswith("demo::Counter::")]
        assert any("demo::Counter::add" in event["functionName"] for event in calls), calls
        assert len({event["probeId"] for event in calls if "demo::Counter::add" in event["functionName"]}) == 1, calls
        def scalar(value):
            if isinstance(value, dict):
                for key in ("unsignedValue", "signedValue", "boolValue", "stringValue"):
                    if key in value:
                        return value[key]
            return value

        def fields(event):
            return {field.get("name"): scalar(field.get("value"))
                    for field in event.get("objectFields", [])}

        provisional_rows = json.loads((root / "provisional.json").read_text())
        assert len(provisional_rows) == 1, provisional_rows
        provisional_fields = fields(provisional_rows[0])
        assert int(provisional_fields["object_id"]) == int(provisional_fields["object_generation"]) == 0
        assert provisional_fields["identity_verified"] in (False, "false", 0, "0")

        callback_rows = [event for event in calls if "demo::Counter::add" in event.get("functionName", "")]
        assert len(callback_rows) >= 2, callback_rows
        for event in callback_rows:
            values = fields(event)
            assert str(values["probe_id"]) == str(event["probeId"]), (event, values)
            assert str(values["call_id"]) == str(event["callId"]), (event, values)
            identity = (int(values["object_id"]), int(values["object_generation"]))
            assert identity in identities, (identity, identities)
            assert created_at[identity] < events.index(event), (identity, events)
            assert values["identity_verified"] in (True, "true", 1, "1"), (event, values)

        for original, destination in ((header, hidden / header.name), (source, hidden / source.name),
                                      (callbacks, hidden / callbacks.name)):
            assert destination.is_file() and not original.exists()
        assert {path: hashlib.sha256(path.read_bytes()).digest() for path in (target, library)} == target_hashes
        print("offline package prepared before ELF; hidden headers; same-address lifetimes; "
              "failed second method rolled back; fresh registration succeeded")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent"):
        parser.add_argument(name, type=lambda path: Path(path).resolve())
    parser.add_argument("cxx")
    run(parser.parse_args())
