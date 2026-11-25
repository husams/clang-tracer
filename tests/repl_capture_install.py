"""A value that automatic capture cannot encode never keeps a probe from installing (issue #182).

The fixture (tests/fixtures/repl_capture_install.*) is built with the GCC 8.5 g++ the tracer SDK
ships (the project compiler without one). Every function takes one argument record that capture
cannot encode and a `Sibling` it can, returns 1, and counts its calls. One "g" byte makes the
fixture call every function once, so every count below is exact.

Checked here, on the direct route (--include) and the prepared route (%probe prepare):
  * a std::thread::id, std::error_code or std::exception_ptr member (also one nested two records
    deep), and a member that is an unnamed struct, are reported UNSUPPORTED with the type and the
    member path, and the probe installs;
  * `struct { ... } a, b[2];` keeps its layout in the prepared adapter, so the members after it
    still match the header;
  * the sibling argument is captured whole, the result is 1, each call fires exactly one event and
    the target keeps running;
  * a record the schema accepts but whose generated codec does not compile (a private nested
    type, a type in an anonymous namespace) is reported UNSUPPORTED too, with the compiler's
    first error, and the validation report says that capture was disabled;
  * so is capture code that fails to compile for any other reason (TRACER_TEST_BREAK_CAPTURE
    forces that), for an invocation callback, for a callback that reads the receiver of a
    method the agent rebuilds from debug data, and for a shared-context callback;
  * a probe whose own callback does not compile, or whose header does not match the target, is
    still rejected.
"""

import base64
from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from repl_cdtor import send
from repl_std_headers import start, varints, zigzag

PASSES = 3
FIXTURES = ("repl_capture_install.hpp", "repl_capture_install.cpp", "repl_capture_install_main.cpp")
HEADER = "repl_capture_install.hpp"
STANDARD = "unsupported standard type: "
UNNAMED = "unsupported unnamed struct type"
NOT_COMPILED = "the generated capture code did not compile"

# (function suffix, argument record, the detail its snapshot reports)
UNSUPPORTED = (
    ("thread_id", "ThreadId", STANDARD + "std::thread::id (member id)"),
    ("error_code", "ErrorCode", STANDARD + "std::error_code (member code)"),
    ("exception_ptr_empty", "ExceptionPtr",
     STANDARD + "std::__exception_ptr::exception_ptr (member error)"),
    ("exception_ptr_set", "ExceptionPtr",
     STANDARD + "std::__exception_ptr::exception_ptr (member error)"),
    ("nested", "Nested", STANDARD + "std::thread::id (member middle.id)"),
    ("anon_struct", "AnonStruct", UNNAMED + " (member inner)"),
    ("anon_group", "AnonGroup", UNNAMED + " (member a)"),
)
# Accepted by the schema; whether the generated code compiles is only known to the compiler.
CONTAINED = (
    ("contain_private", "Owner", NOT_COMPILED),
    ("contain_anonymous", "Local", NOT_COMPILED),
)

# Makes the agent's generated capture code fail to compile, whatever the type.
BREAK_CAPTURE = "TRACER_TEST_BREAK_CAPTURE"

SHARED_HOOK = """struct Scope {};
struct Counter : shared_state<Scope> { int n = 0; };
struct Interceptor {
  Interceptor(State<Counter>);
  void pre(const Call&, Counter& state) { state.n++; }
  void post(const Call& call) { call.context().event("seen"); }
};
"""
HOOK = "struct Interceptor { void pre(const Call&) {} void post(const Call&) {} };\n"


def build(compiler, link_flags, root, *options):
    """Compile the fixture into `root`/target; returns the executable."""
    out = root / "target"
    sources = [str(root / "src" / f) for f in FIXTURES if f.endswith(".cpp")]
    command = [compiler, "-g", "-O0", "-std=gnu++17", "-fno-inline", "-fno-omit-frame-pointer",
               "-fcf-protection=none", "-fno-pie", "-no-pie", f"-I{root / 'src'}", *options,
               *sources, "-o", str(out), "-Wl,--build-id=sha1", *link_flags]
    subprocess.run(command, check=True, capture_output=True, text=True)
    return out


def run_pass(target):
    """One pass of the fixture; the number of calls each function has had so far."""
    send(target.process, "g")
    line = target.app.expect(rf"^PASS {target.passes} (.*)$")
    target.passes += 1
    return {name: int(count) for name, count in re.findall(r"(\w+)=(\d+)", line.group(1))}


def decode_sibling(snapshot):
    assert snapshot["status"] == "COMPLETE", snapshot
    fields = varints(base64.b64decode(snapshot["payload"]))
    assert [number for number, _ in fields] == [1], fields
    members = dict(varints(fields[0][1]))
    assert zigzag(members[1]) == 7 and members[2] == b"ada", members


class Route:
    """How a probe is installed: from the header, or from a prepared package."""

    def __init__(self, session, root, compiler):
        self.session = session
        self.root = root
        self.header = root / "src" / HEADER
        self.compdb = root / "compile_commands.json"
        self.hook = root / "hook.cpp"
        self.hook.write_text(HOOK)
        source = root / "src" / "repl_capture_install.cpp"
        self.compdb.write_text(json.dumps([{
            "directory": str(root / "src"), "file": str(source),
            "arguments": [compiler, "-g", "-std=gnu++17", "-c", str(source)]}]))

    @staticmethod
    def selector(name, record):
        return f"capi::use_{name}(const capi::{record} &, const capi::Sibling &)"

    def options(self, hook=None):
        return ("--callback", str(hook or self.hook), "--module", self.session.module)


class Direct(Route):
    name = "direct"

    def install(self, name, record, hook=None, *extra):
        return self.session.inject(f'"capi::use_{name}"', *self.options(hook),
                                   "--include", str(self.header), *extra)[0]

    def install_error(self, name, record, hook=None, *extra):
        return self.session.inject_error(f'"capi::use_{name}"', *self.options(hook),
                                         "--include", str(self.header), *extra)


class Prepared(Route):
    name = "prepared"

    def prepare(self, name, record, *extra):
        package = self.root / f"package-{name}"
        self.session.command(
            f'%probe prepare "{self.selector(name, record)}" --include "{self.header}" '
            f'--compdb "{self.compdb}" --output "{package}" {" ".join(extra)}')
        line = self.session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        return package

    def install(self, name, record, hook=None, *extra):
        package = self.prepare(name, record)
        return self.session.inject(f'"{self.selector(name, record)}"', *self.options(hook),
                                   "--prepared", f'"{package}"', "--prepared-mode", "require",
                                   *extra)[0]


def check_route(route, cases):
    """Every case installs, reports the case record unsupported and the sibling whole."""
    session = route.session
    probes = {name: route.install(name, record) for name, record, _ in cases}
    counts = {}
    for _ in range(PASSES):
        counts = run_pass(session.target)
    for name, record, detail in cases:
        events = session.events(probes[name], "POST", PASSES, f"{route.name}_{name}")
        assert len(events) == PASSES, (name, len(events))
        for event in events:
            assert event["phase"] == "POST", event
            assert event["result"]["signedValue"] == "1", (name, event["result"])
            record_argument, sibling_argument = event["arguments"]
            snapshot = record_argument["snapshot"]
            assert snapshot["status"] == "UNSUPPORTED", (name, snapshot)
            assert snapshot["cppType"] == f"capi::{record}", (name, snapshot)
            if detail == NOT_COMPILED:
                assert snapshot["detail"].startswith(detail), (name, snapshot)
            else:
                assert snapshot["detail"] == detail, (name, snapshot)
            # Timing and the raw values of both arguments are kept.
            assert int(record_argument["unsignedValue"]) != 0, (name, record_argument)
            assert int(sibling_argument["unsignedValue"]) != 0, (name, sibling_argument)
            if detail == NOT_COMPILED:
                # No value is captured when the capture code does not compile.
                assert sibling_argument["snapshot"]["status"] == "UNSUPPORTED", sibling_argument
                assert sibling_argument["snapshot"]["detail"].startswith(detail), sibling_argument
            else:
                decode_sibling(sibling_argument["snapshot"])
            assert int(event["monotonicTimeNs"]) > 0, event
        # The function ran once per call, whatever the probe did.
        assert counts[name] == PASSES, (name, counts)
    assert counts and set(counts.values()) == {PASSES}, counts
    assert session.target.process.poll() is None
    assert set(run_pass(session.target).values()) == {PASSES + 1}


def rejection(session, error):
    """Everything printed for a rejected install: the error line and the lines after it."""
    session.command('tracer.print("end-of-error");')
    session.output.expect(r"end-of-error")
    return error


def check_rejections(session, root, compiler):
    """What is wrong with the callback or the header is still reported, never degraded."""
    direct = Direct(session, root, compiler)
    sources = (
        ("calls_undeclared",
         "struct Interceptor { void pre(const Call&) { undeclared_function(1); } };\n",
         "undeclared_function"),
        ("syntax_error",
         "struct Interceptor { void pre(const Call&) { int x = ; } };\n", "expected expression"),
    )
    # A record whose capture is unsupported by name, and one that only fails to compile.
    for name, record in (("anon_struct", "AnonStruct"), ("contain_private", "Owner")):
        for key, source, expected in sources:
            hook = root / f"{key}_{name}.cpp"
            hook.write_text(source)
            start_line = len(session.output.seen)
            rejection(session, direct.install_error(name, record, hook))
            text = "".join(session.output.seen[start_line:])
            assert expected in text, (key, name, text)
            assert "installed probe" not in text and "capture code" not in text, (key, text)
        # A header that disagrees with the target's layout.
        start_line = len(session.output.seen)
        rejection(session, direct.install_error(name, record, None, "--compile-option", "-DCAPI_SKEW"))
        text = "".join(session.output.seen[start_line:])
        assert "does not match" in text, (name, text)
        assert "installed probe" not in text and "capture code" not in text, (name, text)


def check_validation(session, root, compiler):
    """A degraded install says so in the validation report."""
    direct = Direct(session, root, compiler)
    session.command(f'%probe validate "capi::use_contain_private" --callback "{direct.hook}" '
                    f'--include "{direct.header}" --module {session.module}')
    session.output.expect(r"^success: true$")
    session.output.expect(
        r'message: "automatic value capture was disabled because the generated capture code '
        r'did not compile"')


def check_forced_failure(repl, server, agent, executable, stack, root, compiler):
    """Capture code that fails to compile degrades the install on every route to it: the
    invocation callback and the shared-context callback."""
    os.environ[BREAK_CAPTURE] = "1"
    try:
        invocation = start(repl, server, agent, executable, stack, root)
        receiver = start(repl, server, agent, executable, stack, root)
        shared = start(repl, server, agent, executable, stack, root)
    finally:
        del os.environ[BREAK_CAPTURE]
    route = Direct(invocation, root, compiler)
    probe = route.install("anon_struct", "AnonStruct")
    for _ in range(PASSES):
        counts = run_pass(invocation.target)
    for event in invocation.events(probe, "POST", PASSES, "forced_invocation"):
        record_argument, sibling_argument = event["arguments"]
        assert record_argument["snapshot"]["detail"] == UNNAMED + " (member inner)", record_argument
        snapshot = sibling_argument["snapshot"]
        assert snapshot["status"] == "UNSUPPORTED", snapshot
        # The first error of the failed attempt is part of the detail.
        assert snapshot["detail"].startswith(NOT_COMPILED + ": "), snapshot
        assert BREAK_CAPTURE in snapshot["detail"], snapshot
        assert event["result"]["signedValue"] == "1", event
    assert counts["anon_struct"] == PASSES, counts

    # The receiver is imported from debug data: the callback cannot be built before that, so this
    # takes the receiver-import route, whose fallback cannot name the compiler's error.
    reader = root / "receiver.cpp"
    reader.write_text("class Interceptor { public:\n"
                      "  void pre(const Call &call) { call.field(\"total\", call.self()->total); }\n"
                      "  void post(const Call &call) { call.field(\"returned\", call.result<int>()); }\n"
                      "};\n")
    probe = receiver.inject('"capi::Holder::use"', "--callback", str(reader), "--module",
                            receiver.module, "--prepared-mode", "off")[0]
    for _ in range(PASSES):
        counts = run_pass(receiver.target)
    for event in receiver.events(probe, "POST", PASSES, "forced_receiver"):
        snapshot = event["arguments"][0]["snapshot"]
        assert snapshot["status"] == "UNSUPPORTED", snapshot
        assert snapshot["detail"] == NOT_COMPILED, snapshot
        assert event["result"]["signedValue"] == "4", event
    assert counts["receiver"] == PASSES, counts
    receiver.command(f'%probe validate "capi::Holder::use" --callback "{reader}" '
                     f"--module {receiver.module} --prepared-mode off")
    receiver.output.expect(r"^success: true$")
    receiver.output.expect(
        r'message: "automatic value capture was disabled because the generated capture code '
        r'did not compile"')
    assert receiver.target.process.poll() is None

    hook = root / "shared.cpp"
    hook.write_text(SHARED_HOOK)
    route = Direct(shared, root, compiler)
    route.install("anon_struct", "AnonStruct", hook)
    for _ in range(PASSES):
        counts = run_pass(shared.target)
    shared.command(f"%collect --name seen --count {PASSES} --timeout 15 --as forced_shared")
    shared.output.expect(rf"received={PASSES} ")
    assert counts["anon_struct"] == PASSES, counts
    shared.command(f'%probe validate "capi::use_anon_struct" --callback "{hook}" '
                   f'--include "{route.header}" --module {shared.module}')
    shared.output.expect(r"^success: true$")
    shared.output.expect(
        r'message: "automatic value capture was disabled because the generated capture code '
        r'did not compile"')
    assert shared.target.process.poll() is None


def main():
    repl, server, agent, project_exe, fixtures, compiler, link_flags = sys.argv[1:8]
    link_flags = link_flags.split() if link_flags != "-" else []
    fixtures = Path(fixtures)
    with ExitStack() as stack:
        root = Path(tempfile.mkdtemp(prefix="repl-capture-install-"))
        stack.callback(shutil.rmtree, root, ignore_errors=True)
        (root / "src").mkdir()
        for name in FIXTURES:
            shutil.copy(fixtures / name, root / "src" / name)
        # The fixture built by the GCC the tracer SDK ships.
        executable = build(compiler, link_flags, root)
        session = start(repl, server, agent, executable, stack, root)
        check_route(Direct(session, root, compiler), UNSUPPORTED + CONTAINED)
        session = start(repl, server, agent, executable, stack, root)
        check_route(Prepared(session, root, compiler), UNSUPPORTED)
        # The fixture built by the project's own compiler: what is rejected, and what is forced.
        session = start(repl, server, agent, project_exe, stack, root)
        check_validation(session, root, compiler)
        check_rejections(session, root, compiler)
        check_forced_failure(repl, server, agent, project_exe, stack, root, compiler)
    print("automatic capture failures keep the probe installed: direct, prepared")


if __name__ == "__main__":
    main()
