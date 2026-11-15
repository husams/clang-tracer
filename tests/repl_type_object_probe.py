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
        hidden_source = root / "hidden-derived.cpp"
        hidden_object = build / "hidden-derived.o"
        unit_source = root / "type-unit.cpp"
        unit_object = build / "type-unit.o"
        package = root / "counter.object"
        local_package = root / "local-base.object"
        unit_package = root / "type-unit-base.object"
        ctor_only_package = root / "ctor-only.object"
        identity_method_package = root / "identity-method-only.object"
        identity_combined_package = root / "identity-combined.object"
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
class LocalBase {
public:
  explicit LocalBase(int initial);
  ~LocalBase();
  int value() const;
private:
  int value_;
};
class UnitBase {
public:
  explicit UnitBase(int initial);
  ~UnitBase();
  int value() const;
private:
  int value_;
};
class CtorOnly {
public:
  explicit CtorOnly(int initial);
  int value() const;
private:
  int value_;
};
class IdentityC1EBase {
public:
  explicit IdentityC1EBase(int initial);
  ~IdentityC1EBase();
  int get() const;
  void set(int value);
private:
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
LocalBase::LocalBase(int initial) : value_(initial) {}
LocalBase::~LocalBase() { value_ = -1; }
int LocalBase::value() const { return value_; }
UnitBase::UnitBase(int initial) : value_(initial) {}
UnitBase::~UnitBase() { value_ = -1; }
int UnitBase::value() const { return value_; }
CtorOnly::CtorOnly(int initial) : value_(initial) {}
int CtorOnly::value() const { return value_; }
IdentityC1EBase::IdentityC1EBase(int initial) : value_(initial) {}
IdentityC1EBase::~IdentityC1EBase() { value_ = -1; }
int IdentityC1EBase::get() const { return value_; }
void IdentityC1EBase::set(int value) { value_ = value; }
}
""")
        hidden_source.write_text("""#include "counter.hpp"
namespace demo {
class HiddenDerived final : public IdentityC1EBase {
public:
  explicit HiddenDerived(int initial) : IdentityC1EBase(initial), hidden_(initial + 1) {}
  ~HiddenDerived() {}
  int hidden_;
};
}
extern "C" __attribute__((visibility("default"), noinline)) int
hidden_derived_roundtrip(int initial) {
  demo::HiddenDerived object(initial);
  object.set(initial + 1);
  return object.get();
}
""")
        unit_source.write_text("""#include "counter.hpp"
namespace demo {
struct UnitDerived : UnitBase { int extra; };
int use_type_unit() {
  UnitDerived object{UnitBase(3), 4};
  return object.value() + object.extra;
}
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
void exercise_local_derived() {
  struct LocalDerived final : demo::LocalBase {
    explicit LocalDerived(int initial) : LocalBase(initial) {}
    ~LocalDerived() {}
  } object(5);
  (void)object.value();
}
demo::IdentityC1EBase *unobserved_identity = nullptr;
}
extern "C" __attribute__((noinline)) void
identity_set_through_c(demo::IdentityC1EBase *object, int value) {
  object->set(value);
}
extern "C" __attribute__((noinline)) int
identity_get_through_c(demo::IdentityC1EBase *object) {
  return object->get();
}
extern "C" __attribute__((noinline)) int
identity_main_roundtrip(int initial) {
  demo::IdentityC1EBase object(initial);
  const int before = identity_get_through_c(&object);
  identity_set_through_c(&object, before + 2);
  return identity_get_through_c(&object);
}
extern "C" int hidden_derived_roundtrip(int);
int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  demo::Counter existing(7);
  demo::CtorOnly ctor_only(9);
  unobserved_identity = new demo::IdentityC1EBase(6);
  exercise_local_derived();
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
    } else if (command == 'c') {
      std::cout << \"CTOR_ONLY=\" << ctor_only.value() << std::endl;
    } else if (command == 'm') {
      int result = 0;
      {
        demo::IdentityC1EBase object(40);
        identity_set_through_c(&object, identity_get_through_c(&object) + 2);
        result = identity_get_through_c(&object);
      }
      std::cout << \"IDENTITY_MAIN=\" << result
                << \" EXTERN_C=\" << identity_main_roundtrip(60) << std::endl;
    } else if (command == 'h') {
      std::cout << \"HIDDEN_DERIVED=\" << hidden_derived_roundtrip(50) << std::endl;
    } else if (command == 'u') {
      delete unobserved_identity;
      unobserved_identity = nullptr;
      std::cout << \"UNOBSERVED_DESTROYED\" << std::endl;
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

        def prepare_commands(methods, callback_path, output, type_name="demo::Counter"):
            selected = "".join(f" --method {quote(method)}" for method in methods)
            return (f"%object-probe prepare {type_name}{selected}"
                    f" --include {quote(header)} --compdb {quote(compdb)}"
                    f" --translation-unit {quote(source)} --callbacks {quote(callback_path)}"
                    f" --output {quote(output)}")

        methods = ("demo::Counter::add(int)",)
        writer = (f" --field value_ --code {quote('demo::Counter::store(int)')}"
                  )
        identity_methods = ("demo::IdentityC1EBase::get() const",)
        identity_writer = (f" --field value_ --code {quote('demo::IdentityC1EBase::set(int)')}"
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
        send(repl, prepare_commands(("demo::LocalBase::value() const",), callbacks,
                                   local_package, "demo::LocalBase"))
        local_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not local_prepared.string.startswith("error:"), local_prepared.string
        send(repl, prepare_commands(("demo::UnitBase::value() const",), callbacks,
                                   unit_package, "demo::UnitBase"))
        out.expect(r"^object-probe package=.*type-unit-base.object")
        send(repl, prepare_commands(("demo::CtorOnly::value() const",), callbacks,
                                   ctor_only_package, "demo::CtorOnly"))
        ctor_only_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not ctor_only_prepared.string.startswith("error:"), ctor_only_prepared.string
        send(repl, prepare_commands(identity_methods, callbacks, identity_method_package,
                                    "demo::IdentityC1EBase"))
        identity_method_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not identity_method_prepared.string.startswith("error:"), identity_method_prepared.string
        send(repl, prepare_commands(identity_methods, callbacks, identity_combined_package,
                                    "demo::IdentityC1EBase") + identity_writer)
        identity_combined_prepared = out.expect(r"^(?:object-probe package=|prepared .*package|error:)")
        assert not identity_combined_prepared.string.startswith("error:"), identity_combined_prepared.string
        for path in (package, bad_package, fresh_package, overlap, local_package, unit_package,
                     ctor_only_package, identity_method_package, identity_combined_package):
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
        subprocess.run([str(args.cxx), "-std=c++20", "-g0", "-O0", "-fPIC", "-fno-inline",
                        "-fpatchable-function-entry=8", f"-I{generated}", f"-I{root}",
                        "-DCOUNTER_PAD_BYTES=13", "-c", str(hidden_source),
                        "-o", str(hidden_object)], cwd=build, check=True)
        subprocess.run([*compile_args[:-4], "-gdwarf-4", "-fdebug-types-section",
                        "-c", str(unit_source), "-o", str(unit_object)], cwd=build, check=True)
        subprocess.run([str(args.cxx), "-shared", str(build / "counter.o"),
                        str(hidden_object), str(unit_object), "-o", str(library)],
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
        assert "demo::CtorOnly::CtorOnly(int)" in nm, nm
        assert "demo::CtorOnly::~CtorOnly()" not in nm, nm
        assert "demo::IdentityC1EBase::IdentityC1EBase(int)" in nm, nm
        assert "demo::IdentityC1EBase::get() const" in nm, nm
        dwarf = subprocess.run(["readelf", "--debug-dump=info", "--wide", str(library)],
                               check=True, text=True, stdout=subprocess.PIPE).stdout
        assert "Counter" in dwarf and "value_" in dwarf
        assert "Contents of the .debug_types section" in dwarf and "UnitDerived" in dwarf, dwarf
        hidden_dwarf = subprocess.run(["readelf", "--debug-dump=info", "--wide",
                                       str(hidden_object)], check=True, text=True,
                                      stdout=subprocess.PIPE).stdout
        assert not re.search(r"DW_TAG_(?:class_type|structure_type)", hidden_dwarf), hidden_dwarf
        assert "HiddenDerived" not in dwarf, dwarf
        target_relocations = subprocess.run(["readelf", "--relocs", "--wide", str(target)],
                                            check=True, text=True,
                                            stdout=subprocess.PIPE).stdout
        c1_symbol = "_ZN4demo15IdentityC1EBaseC1Ei"
        counter_c1_symbol = "_ZN4demo7CounterC1Ei"
        assert c1_symbol in target_relocations and counter_c1_symbol in target_relocations, target_relocations
        hidden_relocations = subprocess.run(
            ["readelf", "--relocs", "--wide", str(hidden_object)], check=True,
            text=True, stdout=subprocess.PIPE).stdout
        assert "_ZN4demo15IdentityC1EBaseC2Ei" in hidden_relocations, hidden_relocations
        target_dwarf = subprocess.run(["readelf", "--debug-dump=info", "--wide", str(target)],
                                      check=True, text=True, stdout=subprocess.PIPE).stdout
        for caller in ("identity_main_roundtrip", "identity_set_through_c",
                       "cycle",
                       "identity_get_through_c"):
            assert caller in target_dwarf, (caller, target_dwarf)
        for path in (header, source, hidden_source, unit_source, callbacks, bad_callbacks,
                     generated / "counter_build_config.hpp"):
            shutil.move(path, hidden / path.name)
            assert not path.exists()

        target_hashes = {path: hashlib.sha256(path.read_bytes()).digest() for path in (target, library)}
        run_env = dict(env, LD_LIBRARY_PATH=str(build))
        target_process, app = child([str(target)], stdin=subprocess.PIPE, env=run_env)
        app.expect(r"^READY$")
        repl, out = child(repl_argv, env=env)
        send(repl, f"%attach {target_process.pid}")
        out.expect(rf"^attached session=\S+ pid={target_process.pid}$")

        send(repl, f"%object-probe preview --package {quote(local_package)} --module libcounter.so")
        local_refusal = out.expect(r"^error: .*")
        assert "known derived construction of demo::LocalBase" in local_refusal.string, local_refusal.string
        send(repl, f"%object-probe preview --package {quote(unit_package)} --module libcounter.so")
        out.expect(r"^error: .*known derived construction of demo::UnitBase")

        ctor_only_binding = f"--package {quote(ctor_only_package)} --module libcounter.so"
        send(repl, f"%object-probe preview {ctor_only_binding}")
        ctor_only_preview = out.expect(r"^(?:object-probe preview|binding=|error:)")
        assert not ctor_only_preview.string.startswith("error:"), ctor_only_preview.string
        out.expect(r"construct \(provisional: incomplete lifetime coverage\)")
        send(repl, f"%object-probe start {ctor_only_binding}")
        ctor_only_started = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")
        ctor_only_registration = ctor_only_started[1]
        ctor_only_probe = out.expect(r"method_probe=(\d+) active=yes")[1]
        send(repl, f"%record ctor-only.db --sqlite --probe {ctor_only_probe} --count 1 --timeout 5")
        target_process.stdin.write("c\n")
        target_process.stdin.flush()
        app.expect(r"^CTOR_ONLY=9$")
        ctor_only_record = out.expect(r"^received=1 cursor=(\d+) .*complete=true .*recording=ctor-only.db")
        send(repl, "%events open ctor-only.db --as ctor_only")
        out.expect(r"^opened events=1 retained=1 evicted=0$")
        send(repl, "%events export ctor_only --format json --output ctor-only.json")
        send(repl, f"%object-probe stop {ctor_only_registration}")
        out.expect(rf"^object-probe registration={ctor_only_registration} active=no .*restoration_pending=no")
        ctor_only_event = json.loads((root / "ctor-only.json").read_text())[0]
        ctor_only_fields = {field.get("name"): field.get("value")
                            for field in ctor_only_event.get("objectFields", [])}

        def scalar(value):
            if isinstance(value, dict):
                for key in ("unsignedValue", "signedValue", "boolValue", "stringValue"):
                    if key in value:
                        return value[key]
            return value

        assert int(scalar(ctor_only_fields["object_id"])) == 0, ctor_only_event
        assert scalar(ctor_only_fields["identity_verified"]) in (False, "false", 0, "0"), ctor_only_event

        def event_fields(event):
            return {field.get("name"): scalar(field.get("value"))
                    for field in event.get("objectFields", [])}

        # Both main and a nonmember extern-C function construct IdentityC1EBase
        # via its C1 PLT relocation, which is checked above at build time.
        identity_binding = (f"--package {quote(identity_method_package)} "
                            "--module libcounter.so")
        send(repl, f"%object-probe preview {identity_binding}")
        identity_preview = out.expect(r"^(?:object-probe preview|binding=|error:)")
        assert not identity_preview.string.startswith("error:"), identity_preview.string
        send(repl, f"%object-probe start {identity_binding}")
        identity_started = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")
        identity_registration = identity_started[1]
        identity_probe = out.expect(r"method_probe=(\d+) active=yes")[1]
        send(repl, f"%record unobserved-dtor.db --sqlite --timeout 2 --after {ctor_only_record[1]}")
        send(target_process, "u")
        app.expect(r"^UNOBSERVED_DESTROYED$")
        unobserved_record = out.expect(
            r"^received=0 cursor=(\d+) .*timed_out=true .*recording=unobserved-dtor.db")
        identity_after = int(unobserved_record[1])
        send(repl, f"%record identity-main.db --sqlite --after {identity_after} --timeout 2")
        send(target_process, "m")
        app.expect(r"^IDENTITY_MAIN=42 EXTERN_C=62$")
        identity_record = out.expect(
            r"^received=10 cursor=(\d+) .*timed_out=true .*recording=identity-main.db")
        send(repl, "%events open identity-main.db --as identity_main")
        out.expect(r"^opened events=10 retained=10 evicted=0$")
        send(repl, "%events export identity_main --format json --output identity-main.json")
        send(repl, f"%object-probe stop {identity_registration}")
        out.expect(rf"^object-probe registration={identity_registration} active=no .*restoration_pending=no")
        identity_events = json.loads((root / "identity-main.json").read_text())
        assert len(identity_events) == 10, identity_events
        assert all(event.get("objectProbeRegistrationId") == identity_registration
                   for event in identity_events), identity_events
        identity_times = [int(event["monotonicTimeNs"]) for event in identity_events]
        assert identity_times == sorted(identity_times), identity_events
        identity_created = [event for event in identity_events
                            if event.get("objectObservation", {}).get("kind") == "CREATED"]
        identity_ended = [event for event in identity_events
                          if event.get("objectObservation", {}).get("kind") == "TEARDOWN_END"]
        identity_calls = [event for event in identity_events if event.get("probeId") == identity_probe]
        assert len(identity_created) == len(identity_ended) == 2 and len(identity_calls) == 4, identity_events

        def recorded_identity(event):
            observation = event["objectObservation"]
            return int(observation["lifetimeId"]), int(observation["generation"])

        identities = {recorded_identity(event): identity_events.index(event) for event in identity_created}
        assert len(identities) == 2 and {recorded_identity(event) for event in identity_ended} == set(identities)
        assert all(object_id > 0 and generation > 0 for object_id, generation in identities)
        call_counts = dict.fromkeys(identities, 0)
        for event in identity_calls:
            values = event_fields(event)
            identity = int(values["object_id"]), int(values["object_generation"])
            assert identity in identities and identities[identity] < identity_events.index(event), event
            assert values["identity_verified"] in (True, "true", 1, "1")
            call_counts[identity] += 1
        assert all(count == 2 for count in call_counts.values()), identity_events

        # A derived class compiled without DWARF has only a Base subobject
        # constructor call. Combined method+writer observation must leave it
        # provisional and must not publish a false complete-object CREATED.
        hidden_binding = (f"--package {quote(identity_combined_package)} "
                         "--module libcounter.so")
        send(repl, f"%object-probe preview {hidden_binding}")
        hidden_preview = out.expect(r"^(?:object-probe preview|binding=|error:)")
        assert not hidden_preview.string.startswith("error:"), hidden_preview.string
        send(repl, f"%object-probe start {hidden_binding}")
        hidden_started = out.expect(r"^object-probe registration=(\d+) active=yes methods=1")
        hidden_registration = hidden_started[1]
        hidden_probe = out.expect(r"method_probe=(\d+) active=yes")[1]
        send(repl, f"%record hidden-derived.db --sqlite --timeout 2 --after {identity_record[1]}")
        send(target_process, "h")
        app.expect(r"^HIDDEN_DERIVED=51$")
        hidden_record = out.expect(
            r"^received=(\d+) cursor=(\d+) .*timed_out=true .*recording=hidden-derived.db")
        assert int(hidden_record[1]) > 0, "hidden derived call produced no method event"
        send(repl, "%events open hidden-derived.db --as hidden_derived")
        opened_hidden = out.expect(r"^opened events=(\d+) retained=\d+ evicted=0$")
        send(repl, "%events export hidden_derived --format json --output hidden-derived.json")
        send(repl, f"%object-probe stop {hidden_registration}")
        out.expect(rf"^object-probe registration={hidden_registration} active=no .*restoration_pending=no")
        hidden_events = json.loads((root / "hidden-derived.json").read_text())
        assert len(hidden_events) == int(opened_hidden[1]), hidden_events
        assert all(event.get("objectProbeRegistrationId") == hidden_registration
                   for event in hidden_events), hidden_events
        hidden_times = [int(event["monotonicTimeNs"]) for event in hidden_events]
        assert hidden_times == sorted(hidden_times), hidden_events
        hidden_observations = [event.get("objectObservation", {}) for event in hidden_events]
        assert not any(observation.get("kind") == "CREATED" for observation in hidden_observations), hidden_events
        hidden_destroy_begin = [event for event in hidden_events
                                if event.get("objectObservation", {}).get("kind") == "DESTROY_BEGIN"]
        hidden_teardown_end = [event for event in hidden_events
                               if event.get("objectObservation", {}).get("kind") == "TEARDOWN_END"]
        assert len(hidden_destroy_begin) == len(hidden_teardown_end) == 1, hidden_events
        destroy_begin_observation = hidden_destroy_begin[0]["objectObservation"]
        teardown_observation = hidden_teardown_end[0]["objectObservation"]
        assert hidden_destroy_begin[0]["objectAddress"] == hidden_teardown_end[0]["objectAddress"], hidden_events
        for observation in (destroy_begin_observation, teardown_observation):
            assert int(observation.get("lifetimeId", 0)) == 0, observation
            assert int(observation.get("generation", 0)) == 0, observation
            assert observation.get("diagnostic") == (
                "provisional destructor boundary with unverified complete-object identity"), observation
        assert teardown_observation.get("closureReason") == "DESTRUCTOR_RETURN", teardown_observation
        hidden_calls = [event for event in hidden_events
                        if event.get("probeId") == hidden_probe and
                        event.get("functionName", "").endswith("IdentityC1EBase::get() const")]
        assert hidden_calls, hidden_events
        for event in hidden_calls:
            values = event_fields(event)
            assert int(values["object_id"]) == int(values["object_generation"]) == 0, event
            assert values["identity_verified"] in (False, "false", 0, "0"), event
        assert all(not observation.get("lifetimeId") and not observation.get("generation")
                   for observation in hidden_observations), hidden_events

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
        send(repl, f"%record counter.db --sqlite --count 10 --after {hidden_record[2]} --timeout 10")
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
        counter_times = [int(event["monotonicTimeNs"]) for event in events]
        assert counter_times == sorted(counter_times), events
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
        ended_at = {
            (int(event["objectObservation"]["lifetimeId"]),
             int(event["objectObservation"]["generation"])): events.index(event)
            for event in ended
        }
        assert [(event["oldInteger"], event["newInteger"]) for event in writes] == [(10, 11), (20, 21)], writes
        calls = [event for event in events if event.get("functionName", "").startswith("demo::Counter::")]
        assert any("demo::Counter::add" in event["functionName"] for event in calls), calls
        assert len({event["probeId"] for event in calls if "demo::Counter::add" in event["functionName"]}) == 1, calls
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
            assert events.index(event) < ended_at[identity], (identity, events)
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
