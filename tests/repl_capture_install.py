"""A value that automatic capture cannot encode never keeps a probe from installing (issue #182).

The fixture (tests/fixtures/repl_capture_install.*) is built with the GCC 8.5 g++ the tracer SDK
ships (the project compiler without one). `use_all` takes every record that capture leaves out
by name plus a `Sibling` it can encode, `use_contained` takes records the schema accepts and the
generated codec cannot spell, and both return 1 and count their calls. One "g" byte makes the
fixture call every function once, so every count below is exact. Each probe is installed once
and every record is checked on it.

Checked here (what a partial snapshot is has tests/repl_partial_capture.py):
  * a member that is a std::thread::id, std::error_code or std::exception_ptr (also one nested
    two records deep) or an unnamed struct (also `struct { ... } a, b[2];`, which the prepared
    adapter keeps in layout) is reported with its reason, on the direct route (--include) and
    the prepared route (%probe prepare); the sibling is captured whole, the result is 1, each
    call fires exactly one event, and the target keeps running;
  * a record the schema accepts but whose generated codec does not compile (a private nested
    type, a type in an anonymous namespace) is reported UNSUPPORTED, with the compiler's first
    error, and the validation report says that capture was disabled;
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
NOT_COMPILED = "the generated capture code did not compile"

RECORDS = ("ThreadId", "ErrorCode", "ExceptionPtr", "ExceptionPtr", "Nested", "AnonStruct",
           "AnonGroup")
STANDARD_REASON = "unsupported standard type: "
UNNAMED_REASON = "unnamed record type cannot be captured automatically"
# The argument records of use_all in order, and what each snapshot reports on a route: UNSUPPORTED
# with a detail, or PARTIAL with the members left out ((name, reason, number, the record that
# declares it)) and the members captured (field number to value). The direct route reads public
# members only, so it names the pointer a record keeps where the prepared route names the type.
THREAD_ID = ("ThreadId", "PARTIAL",
             [("id", STANDARD_REASON + "std::thread::id", 1, "capi::ThreadId")], {2: 5})
NESTED = ("Nested", "PARTIAL",
          [("id", STANDARD_REASON + "std::thread::id", 1, "capi::Middle")], {1: 1})
ANON_STRUCT = ("AnonStruct", "PARTIAL", [("inner", UNNAMED_REASON, 1, "capi::AnonStruct")], {2: 3})
EXPECTED = {
    "direct": (
        THREAD_ID,
        ("ErrorCode", "UNSUPPORTED", "unsupported pointer/reference field: _M_cat"),
        ("ExceptionPtr", "UNSUPPORTED", "unsupported pointer/reference field: _M_exception_object"),
        ("ExceptionPtr", "UNSUPPORTED", "unsupported pointer/reference field: _M_exception_object"),
        NESTED,
        ANON_STRUCT,
        ("AnonGroup", "PARTIAL",
         [("a", UNNAMED_REASON, 1, "capi::AnonGroup"),
          ("b", "unsupported field type 'capi::AnonGroup::(unnamed)[2]': b", 2, "capi::AnonGroup")],
         {3: 9}),
    ),
    "prepared": (
        THREAD_ID,
        ("ErrorCode", "UNSUPPORTED", STANDARD_REASON + "std::error_code"),
        ("ExceptionPtr", "UNSUPPORTED", STANDARD_REASON + "std::__exception_ptr::exception_ptr"),
        ("ExceptionPtr", "UNSUPPORTED", STANDARD_REASON + "std::__exception_ptr::exception_ptr"),
        NESTED,
        ANON_STRUCT,
        ("AnonGroup", "PARTIAL",
         [("a", UNNAMED_REASON, 1, "capi::AnonGroup"), ("b", UNNAMED_REASON, 2, "capi::AnonGroup")],
         {3: 9}),
    ),
}
# Without partial capture, which a failed capture falls back to, a record that has a member left
# out is not captured at all, on either route.
WHOLE_RECORD = (
    "unsupported standard type: std::thread::id",
    "unsupported pointer/reference field: _M_cat",
    "unsupported pointer/reference field: _M_exception_object",
    "unsupported pointer/reference field: _M_exception_object",
    "unsupported standard type: std::thread::id",
    UNNAMED_REASON,
    UNNAMED_REASON,
)

# The argument records of use_contained: accepted by the schema; whether the generated code
# compiles is only known to the compiler.
CONTAINED = ("Owner", "Local")

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
NOTE = (r'message: "automatic value capture was disabled because the generated capture code '
        r'did not compile"')
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

    def options(self, hook=None):
        return ("--callback", str(hook or self.hook), "--module", self.session.module)


class Direct(Route):
    def install(self, function, hook=None, *extra):
        return self.session.inject(f'"capi::{function}"', *self.options(hook),
                                   "--include", str(self.header), *extra)[0]

    def install_error(self, function, hook=None, *extra):
        return self.session.inject_error(f'"capi::{function}"', *self.options(hook),
                                         "--include", str(self.header), *extra)


class Prepared(Route):
    def install(self, selector):
        package = self.root / "package"
        self.session.command(
            f'%probe prepare "{selector}" --include "{self.header}" '
            f'--compdb "{self.compdb}" --output "{package}"')
        line = self.session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        return self.session.inject(f'"{selector}"', *self.options(), "--prepared", f'"{package}"',
                                   "--prepared-mode", "require")[0]


ALL_SELECTOR = ("capi::use_all(const capi::ThreadId &, const capi::ErrorCode &, "
                "const capi::ExceptionPtr &, const capi::ExceptionPtr &, const capi::Nested &, "
                "const capi::AnonStruct &, const capi::AnonGroup &, const capi::Sibling &)")


def run_passes(target):
    counts = {}
    for _ in range(PASSES):
        counts = run_pass(target)
    return counts


def check_event(event, result):
    """What every event keeps whatever capture did: the phase, the result and the raw arguments."""
    assert event["phase"] == "POST", event
    assert event["result"]["signedValue"] == str(result), event["result"]
    assert event["arguments"] and int(event["monotonicTimeNs"]) > 0, event
    for argument in event["arguments"]:
        assert int(argument["unsignedValue"]) != 0, argument


def check_record(argument, expected):
    """One argument of use_all: what the snapshot reports, what it captured and what it left out."""
    record, status, *rest = expected
    snapshot = argument["snapshot"]
    assert snapshot["status"] == status, (record, snapshot)
    assert snapshot["cppType"] == f"capi::{record}", (record, snapshot)
    if status == "UNSUPPORTED":
        assert snapshot["detail"] == rest[0], (record, snapshot)
        return
    omitted, captured = rest
    owners = dict(entry.split("=", 1) for entry in snapshot["recordTypes"])
    left_out = [(entry["name"], entry["reason"], entry["number"], owners[entry["recordType"]])
                for entry in snapshot["unavailable"]]
    assert left_out == omitted, (record, snapshot["unavailable"])
    for entry in snapshot["unavailable"]:
        assert entry["cppType"] and (entry["cppType"] == "std::thread::id" or
                                     "(unnamed" in entry["cppType"]), entry
    members = {number: zigzag(value) for number, value in
               varints(varints(base64.b64decode(snapshot["payload"]))[0][1])
               if isinstance(value, int)}
    assert members == captured, (record, members)


def check_unsupported(session, route, probe, counts):
    """Every record of use_all is reported with its members left out, the sibling whole."""
    events = session.events(probe, "POST", PASSES, f"{route}_all")
    assert len(events) == PASSES, len(events)
    for event in events:
        check_event(event, 1)
        *records, sibling = event["arguments"]
        assert len(records) == len(RECORDS), event
        for argument, expected in zip(records, EXPECTED[route]):
            check_record(argument, expected)
        decode_sibling(sibling["snapshot"])
    # The function ran once per call, whatever the probe did.
    assert counts["all"] == PASSES, counts
    assert session.target.process.poll() is None
    assert run_pass(session.target)["all"] == PASSES + 1


def check_direct(session, root, compiler):
    """use_all and use_contained from the header; the second is degraded and says so."""
    route = Direct(session, root, compiler)
    all_probe = route.install("use_all")
    contained_probe = route.install("use_contained")
    counts = run_passes(session.target)
    for event in session.events(contained_probe, "POST", PASSES, "direct_contained"):
        check_event(event, 1)
        assert len(event["arguments"]) == 3, event
        # No value is captured when the capture code does not compile.
        for argument, record in zip(event["arguments"], (*CONTAINED, "Sibling")):
            snapshot = argument["snapshot"]
            assert snapshot["status"] == "UNSUPPORTED", (record, snapshot)
            assert snapshot["cppType"] == f"capi::{record}", (record, snapshot)
            assert snapshot["detail"].startswith(NOT_COMPILED), (record, snapshot)
    assert counts["contained"] == PASSES, counts
    session.command(f'%probe validate "capi::use_contained" --callback "{route.hook}" '
                    f'--include "{route.header}" --module {session.module}')
    session.output.expect(r"^success: true$")
    session.output.expect(NOTE)
    check_unsupported(session, "direct", all_probe, counts)


def check_prepared(session, root, compiler):
    """use_all from a prepared package: the adapter keeps the unnamed structs in layout."""
    route = Prepared(session, root, compiler)
    probe = route.install(ALL_SELECTOR)
    check_unsupported(session, "prepared", probe, run_passes(session.target))


def rejection(session, error):
    """Everything printed for a rejected install: the error line and the lines after it."""
    session.command('tracer.print("end-of-error");')
    session.output.expect(r"end-of-error")
    return error


def check_rejections(session, root, compiler):
    """What is wrong with the callback or the header is still reported, never degraded."""
    direct = Direct(session, root, compiler)
    attempts = (
        ("calls_undeclared",
         "struct Interceptor { void pre(const Call&) { undeclared_function(1); } };\n",
         "undeclared_function", ()),
        ("syntax_error",
         "struct Interceptor { void pre(const Call&) { int x = ; } };\n", "expected expression", ()),
        # A header that disagrees with the target's layout.
        ("layout", None, "does not match", ("--compile-option", "-DCAPI_SKEW")),
    )
    for key, source, expected, extra in attempts:
        hook = None
        if source:
            hook = root / f"{key}.cpp"
            hook.write_text(source)
        start_line = len(session.output.seen)
        rejection(session, direct.install_error("use_all", hook, *extra))
        text = "".join(session.output.seen[start_line:])
        assert expected in text, (key, text)
        assert "installed probe" not in text and "capture code" not in text, (key, text)


def check_forced_failure(repl, server, agent, executable, stack, root, compiler):
    """Capture code that fails to compile degrades the install on every route to it: the
    invocation callback, the receiver-import callback and the shared-context callback."""
    os.environ[BREAK_CAPTURE] = "1"
    try:
        session = start(repl, server, agent, executable, stack, root)
        shared = start(repl, server, agent, executable, stack, root)
    finally:
        del os.environ[BREAK_CAPTURE]
    route = Direct(session, root, compiler)
    invocation = route.install("use_all")
    # The receiver is imported from debug data: the callback cannot be built before that, so this
    # takes the receiver-import route, whose fallback cannot name the compiler's error.
    reader = root / "receiver.cpp"
    reader.write_text("class Interceptor { public:\n"
                      "  void pre(const Call &call) { call.field(\"total\", call.self()->total); }\n"
                      "  void post(const Call &call) { call.field(\"returned\", call.result<int>()); }\n"
                      "};\n")
    receiver = session.inject('"capi::Holder::use"', "--callback", str(reader), "--module",
                              session.module, "--prepared-mode", "off")[0]
    counts = run_passes(session.target)
    for event in session.events(invocation, "POST", PASSES, "forced_invocation"):
        check_event(event, 1)
        *records, sibling = event["arguments"]
        for argument, record, detail in zip(records, RECORDS, WHOLE_RECORD):
            assert argument["snapshot"]["status"] == "UNSUPPORTED", argument
            assert argument["snapshot"]["detail"] == detail, argument
        snapshot = sibling["snapshot"]
        assert snapshot["status"] == "UNSUPPORTED", snapshot
        # The first error of the failed attempt is part of the detail.
        assert snapshot["detail"].startswith(NOT_COMPILED + ": "), snapshot
        assert BREAK_CAPTURE in snapshot["detail"], snapshot
    assert counts["all"] == PASSES, counts
    for event in session.events(receiver, "POST", PASSES, "forced_receiver"):
        check_event(event, 4)
        snapshot = event["arguments"][0]["snapshot"]
        assert snapshot["status"] == "UNSUPPORTED", snapshot
        assert snapshot["detail"] == NOT_COMPILED, snapshot
    assert counts["receiver"] == PASSES, counts
    # Only the report tells this route from the first: it keeps no compiler diagnostics.
    session.command(f'%probe validate "capi::Holder::use" --callback "{reader}" '
                    f"--module {session.module} --prepared-mode off")
    session.output.expect(r"^success: true$")
    session.output.expect(NOTE)
    assert session.target.process.poll() is None

    # A shared-context callback publishes events of its own, so the report is what shows that
    # its captured values were reported unsupported.
    hook = root / "shared.cpp"
    hook.write_text(SHARED_HOOK)
    route = Direct(shared, root, compiler)
    route.install("use_all", hook)
    counts = run_passes(shared.target)
    shared.command(f"%collect --name seen --count {PASSES} --timeout 15 --as forced_shared")
    shared.output.expect(rf"received={PASSES} ")
    assert counts["all"] == PASSES, counts
    shared.command(f'%probe validate "capi::use_all" --callback "{hook}" '
                   f'--include "{route.header}" --module {shared.module}')
    shared.output.expect(r"^success: true$")
    shared.output.expect(NOTE)
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
        check_direct(start(repl, server, agent, executable, stack, root), root, compiler)
        check_prepared(start(repl, server, agent, executable, stack, root), root, compiler)
        # The fixture built by the project's own compiler: what is rejected, and what is forced.
        check_rejections(start(repl, server, agent, project_exe, stack, root), root, compiler)
        check_forced_failure(repl, server, agent, project_exe, stack, root, compiler)
    print("automatic capture failures keep the probe installed: direct, prepared")


if __name__ == "__main__":
    main()
