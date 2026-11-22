"""Probe virtual methods, say why a pure declaration cannot be probed, and observe polymorphic
receivers.

The fixture (tests/fixtures/repl_virtual.*) has a polymorphic hierarchy whose methods are defined
out of line and called through base references, a pure virtual declaration that has no body
anywhere, a pure virtual method and a pure virtual destructor that do have one, an override
reached through an adjusting thunk, and a non-polymorphic control. One "g" byte makes the fixture
do one exact pass, so every count below is exact.

ctest runs this twice: with the fixture built by the project compiler, and (argument 8, a g++)
with the fixture rebuilt by GCC, whose DWARF differs where it matters here: a pure method is
DW_VIRTUALITY_virtual, the in-class declaration of a destructor carries a "D4" linkage name, an
abstract class has a real deleting destructor, and at -O2 a body can be split (DW_AT_ranges).
The GCC run exits 77 (skipped) when that compiler is not GCC.
"""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

from repl_cdtor import Session, Target, send

PASSES = 3
# method -> (selector, result of one call)
METHODS = {
    "plain": ("virt::Shape::plain() const", 7),
    "shape_area": ("virt::Shape::area() const", 1),
    "square_area": ("virt::Square::area() const", 9),
    "square_sides": ("virt::Square::sides() const", 4),
    "plain_sides": ("virt::Plain::sides() const", 3),
    "mixed_sides": ("virt::Mixed::sides() const", 5),
    "mixed_tag": ("virt::Mixed::tag() const", 200),
    # Pure virtual, but defined: Square::rank() calls it once per pass.
    "rank": ("virt::Shape::rank() const", 11),
    "square_rank": ("virt::Square::rank() const", 12),
}
PURE = "virt::Shape::sides() const"
SKIP = 77
FLAGS = ["-std=gnu++17", "-g", "-fno-omit-frame-pointer", "-fcf-protection=none", "-fno-pie"]
CALLBACKS = """template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call &call) { call.field("object_id", call.object_id()); }
  void post(const Call &call) { call.field("returned", call.result<int>()); }
};
"""


SYMBOLS = ("_ZNK4virt5Shape5plainEv", "_ZNK4virt5Shape4areaEv", "_ZNK4virt6Square4areaEv",
           "_ZNK4virt6Square5sidesEv", "_ZNK4virt5Plain5sidesEv", "_ZNK4virt5Mixed5sidesEv",
           "_ZNK4virt5Mixed3tagEv", "_ZN4virt6SquareC1Ei", "_ZN4virt6SquareD1Ev",
           "_ZN4virt7CounterC1Ev", "_ZN4virt7Counter3addEi")


def entry_addresses(fixture):
    symbols = subprocess.run(["nm", str(fixture)], check=True, capture_output=True,
                             text=True).stdout
    found = {}
    for symbol in SYMBOLS:
        match = re.search(rf"^([0-9a-f]+) [TW] {symbol}$", symbols, re.M)
        assert match, symbol
        found[symbol] = int(match.group(1), 16)
    return found


def entry_bytes(session, addresses):
    return {symbol: session.read(address) for symbol, address in addresses.items()}


def results(session, probe, count, name):
    events = session.events(probe, "POST", count, name)
    return [int(event["result"]["signedValue"]) for event in events]


def dwarf_phase(session, fixture):
    """Route 1: debug information admits every virtual method that has a body."""
    original = entry_bytes(session, entry_addresses(fixture))
    session.command('%trace preview --include "virt::*" --exclude "virt::Counter::*" --exclude "virt::Split::*"')
    rows = []
    while True:
        line = session.output.expect(r"^(\w+) address=\S+ \"([^\"]+)\".*|^dry-run (.*)")
        if line.group(3):
            break
        rows.append((line.group(1), line.group(2), line.group(0)))
    unexpected = [row for row in rows if row[0] not in ("DWARF_SUPPORTED", "ALIAS")]
    assert rows and not unexpected, unexpected
    selectors = {selector for _, selector, _ in rows}
    for selector, _ in METHODS.values():
        assert selector in selectors, (selector, selectors)

    probes = {name: session.inject(f'"{selector}"')[0] for name, (selector, _) in METHODS.items()}
    # A pure declaration has no executable body: the error says so and names the way out.
    # Naming the module makes the resolver read its debug information: no ELF symbol lists the
    # declaration, so without a module there is nothing that leads to it.
    error = session.inject_error(f'"{PURE}"', f'--module "{fixture}"')
    assert "no concrete implementation entry" in error and PURE in error, error
    assert "virtual debug methods are unsupported" not in error, error
    # An overload that does not exist is a plain miss, not a bodyless declaration.
    error = session.inject_error('"virt::Shape::sides(short) const"', f'--module "{fixture}"')
    assert "no concrete implementation entry" not in error, error
    assert "no supported matching callable" in error, error
    # Discovery lists what preview and installation admit.
    session.command(f'%symbols --filter "virt::Square::" --module "{fixture.name}"')
    for method in ("area", "rank", "sides"):
        session.output.expect(rf"^virt::Square::{method}\(\) const \[")

    session.run_passes()
    for name, (_, expected) in METHODS.items():
        assert results(session, probes[name], PASSES, name) == [expected] * PASSES, name
    # Every method is called through a base reference or directly, once per pass, and the
    # program still computes what it did before.
    session.run_passes(1)
    assert session.target.process.poll() is None
    patched = entry_bytes(session, entry_addresses(fixture))
    assert patched["_ZNK4virt6Square4areaEv"] != original["_ZNK4virt6Square4areaEv"]
    session.detach()
    assert entry_bytes(session, entry_addresses(fixture)) == original, "entry bytes were not restored"


def thunk_phase(session, fixture):
    """An adjusting thunk has no DWARF subprogram: it is named as such, not as a virtual method."""
    symbols = subprocess.run(["nm", str(fixture)], check=True, capture_output=True,
                             text=True).stdout
    thunk = re.search(r" T (_ZThn\d+_NK4virt5Mixed3tagEv)$", symbols, re.M)
    assert thunk, symbols
    error = session.inject_error(thunk.group(1))
    assert "thunk" in error and "unsupported" in error, error
    # A thunk-shaped name that no module defines is a missing symbol, not a thunk.
    error = session.inject_error("_ZThn4096_NK4virt5Mixed3tagEv")
    assert "thunk" not in error and "no supported matching callable" in error, error
    session.detach()


def split_phase(session, fixture):
    """GCC variant: a virtual method whose body is split has a body. Whether it installs is the
    entry check's business; it must not be reported as a bodyless declaration."""
    dwarf = subprocess.run(["readelf", "--debug-dump=info", str(fixture)], check=True,
                           capture_output=True, text=True).stdout
    session.command(f'%probe inject "virt::Split::post(long)" --module "{fixture}"')
    line = session.output.expect(r"^installed probe=\d+|^error: (.*)")
    if line.group(1):
        assert "no concrete implementation entry" not in line.group(1), line.group(1)
        # Only a split body is refused here, and then for its code range.
        assert "DW_AT_ranges" in dwarf and "contiguous DWARF code range" in line.group(1), \
            line.group(1)
    session.detach()


def index_phase(session, fixture):
    """Route 3: the module index describes a virtual entry like any other and records why a
    pure declaration has none."""
    work = session.root / "index"
    work.mkdir()
    index = work / "fixture.sqlite"
    session.command(f'%module index --file "{fixture}" --output "{index}"')
    session.output.expect(r"^module index=")
    session.command(f"%module symbols --index \"{index}\" --filter 'virt::*' --format json")
    entries = json.loads(session.output.expect(r"^(\{\"entries\":.*)$")[1])["entries"]
    by_selector = {}
    for entry in entries:
        by_selector.setdefault(entry["selector"], []).append(entry)
    for name, (selector, _) in METHODS.items():
        concrete = [e for e in by_selector[selector] if e["kind"] == "function"]
        assert concrete and all(e["diagnostic"] == "" for e in concrete), (name, by_selector[selector])
    declaration = by_selector[PURE]
    assert [e["kind"] for e in declaration] == ["declaration"], declaration
    assert declaration[0]["diagnostic"] == "no_concrete_entry", declaration
    # That is the only declaration row: a destructor (GCC names its in-class declaration "D4")
    # and a pure virtual method that is defined have entries.
    declarations = {e["selector"] for e in entries if e["kind"] == "declaration"}
    assert declarations == {PURE}, declarations
    destructors = by_selector["virt::Abstract::~Abstract()"]
    assert destructors and all(e["kind"] == "function" for e in destructors), destructors
    # A bare name shared by the bodyless declaration and a defined overload selects the overload.
    session.command(f'%module show --index "{index}" --symbol "virt::Shape::sides"')
    shown = session.output.expect(r"^(\{\"address\".*)$|^error: (.*)")
    assert shown.group(1), shown.group(2)
    assert json.loads(shown.group(1))["linkage"] == "_ZNK4virt5Shape5sidesEi", shown.group(1)

    probes = {}
    for name in ("shape_area", "square_area", "square_sides", "plain_sides", "mixed_tag", "rank"):
        selector, _ = METHODS[name]
        session.command(f'%probe prepare "{selector}" --module "{fixture}" '
                        f'--module-index "{index}" --output "{work / name}"')
        line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        probes[name], _ = session.inject(f'"{selector}"', f'--prepared "{work / name}"',
                                         "--prepared-mode require")
    session.command(f'%probe prepare "{PURE}" --module "{fixture}" --module-index "{index}" '
                    f'--output "{work / "pure"}"')
    message = session.output.expect(r"^error: (.*)")[1]
    assert "no_concrete_entry" in message and PURE in message, message
    session.run_passes()
    for name, probe in probes.items():
        assert results(session, probe, PASSES, "i" + name) == [METHODS[name][1]] * PASSES, name
    session.detach()


def header_phase(session, fixture, root, header, source, compiler):
    """Route 4: the headers resolve overrides. A pure declaration is judged by the module: with a
    body it is an ordinary callable, without one the error lists the overrides the headers
    define."""
    work = root / "header"
    work.mkdir()
    shutil.copy(header, work / "repl_virtual.hpp")
    shutil.copy(source, work / "repl_virtual.cpp")
    (work / "compile_commands.json").write_text(json.dumps([{
        "directory": str(work), "file": "repl_virtual.cpp",
        "arguments": [compiler, "-std=c++17", "-c", "repl_virtual.cpp"]}]))
    base = (f'--include "{work / "repl_virtual.hpp"}" --compdb "{work / "compile_commands.json"}" '
            f'--translation-unit "{work / "repl_virtual.cpp"}"')
    module = f'--module "{fixture}"'
    overrides = {"virt::Square::sides() const", "virt::Plain::sides() const",
                 "virt::Mixed::sides() const"}
    probes = {}
    # "rank" is pure virtual and defined: the module has its body, so it prepares like the rest.
    for name in ("shape_area", "square_area", "square_sides", "plain_sides", "rank"):
        selector, _ = METHODS[name]
        session.command(f'%probe prepare "{selector}" {base} {module} --output "{work / name}"')
        line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        probes[name], _ = session.inject(f'"{selector}"', f'--prepared "{work / name}"',
                                         "--prepared-mode require")
    # A pure virtual destructor always has a body.
    session.command(f'%probe prepare "virt::Abstract::~Abstract()" {base} {module} '
                    f'--output "{work / "abstract"}"')
    line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
    assert line.group(1) == "_ZN4virt8AbstractD1Ev", line.group(0)

    def expect_listed(done):
        """Collect the listed overrides until `done` matches; the two streams interleave."""
        found, listed = None, set()
        while found is None or len(listed) < len(overrides):
            line = session.output.expect(done + r"|^\s+(virt::\w+::sides\(\) const)$")
            if line.group(line.re.groups):
                listed.add(line.group(line.re.groups))
            else:
                found = line
        assert listed == overrides, listed
        return found

    # With the module: it defines no body, so nothing is prepared and the overrides are listed.
    session.command(f'%probe prepare "{PURE}" {base} {module} --output "{work / "pure"}"')
    message = expect_listed(r"^error: (.*)")[1]
    assert "no concrete implementation entry" in message and "defines no body" in message, message
    assert not (work / "pure").exists()
    # Without a module nothing says whether a body exists: the package is written with a note,
    # and installing it where no module defines the method fails with the way out.
    session.command(f'%probe prepare "{PURE}" {base} --output "{work / "unpinned"}"')
    expect_listed(r"^symbol=(_ZNK4virt5Shape5sidesEv)$")
    assert "is a pure virtual declaration" in "".join(session.output.seen[-40:])
    error = session.inject_error(f'"{PURE}"', f'--prepared "{work / "unpinned"}"',
                                 "--prepared-mode require")
    assert "concrete override" in error, error
    session.run_passes()
    for name, probe in probes.items():
        assert results(session, probe, PASSES, "h" + name) == [METHODS[name][1]] * PASSES, name
    session.detach()


def object_phase(session, fixture, root, header, source, compiler, type_name, methods, expect,
                 provisional):
    """Polymorphic object probes: registration starts and reports how sure its identities are."""
    original = entry_bytes(session, entry_addresses(fixture))
    work = root / "object"
    work.mkdir()
    shutil.copy(header, work / "repl_virtual.hpp")
    shutil.copy(source, work / "repl_virtual.cpp")
    (work / "callbacks.cpp").write_text(CALLBACKS)
    (work / "compile_commands.json").write_text(json.dumps([{
        "directory": str(work), "file": "repl_virtual.cpp",
        "arguments": [compiler, "-std=c++17", "-c", "repl_virtual.cpp"]}]))
    selected = "".join(f' --method "{method}"' for method in methods)
    package = work / "package"
    session.command(
        f'%object-probe prepare {type_name}{selected} --include "{work / "repl_virtual.hpp"}" '
        f'--compdb "{work / "compile_commands.json"}" '
        f'--translation-unit "{work / "repl_virtual.cpp"}" --callbacks "{work / "callbacks.cpp"}" '
        f'--output "{package}"')
    line = session.output.expect(r"^object-probe package=|^error: (.*)")
    assert not line.group(1), line.group(1)
    session.command(f'%object-probe start --package "{package}"')
    started = session.output.expect(
        r"^object-probe registration=(\d+) active=yes (.*)|^error: (.*)")
    assert started.group(1), (type_name, started.group(3))
    registration, status = started.group(1), started.group(2)
    if provisional is True:
        assert "object identities remain provisional" in status, status
        assert f"polymorphic type {type_name}" in status, status
    elif provisional is False:
        # Nothing derives from the class: the complete-object proof holds.
        assert "provisional" not in status, status

    # First pass: the lifetime events of the registration.
    if provisional is not None:
        database, export = work / "life.db", work / "life.json"
        session.command(f'%record "{database}" --sqlite --probe {registration} --count 3 '
                        "--timeout 20")
        session.run_passes(1)
        session.output.expect(r"^received=3 .*complete=true")
        session.command(f'%events open "{database}" --as life')
        session.output.expect(r"^opened events=3 ")
        session.command(f'%events export life --format json --output "{export}"')
        deadline = time.monotonic() + 15
        while True:
            try:
                lifetime = [event["objectObservation"] for event in json.loads(export.read_text())]
                break
            except (OSError, ValueError):
                assert time.monotonic() < deadline, "lifetime export did not appear"
                time.sleep(0.1)
        kinds = [event["kind"] for event in lifetime]
        if provisional:
            # A base subobject is never reported as a complete object of the base.
            assert kinds[0] == "LIFETIME_UNCERTAIN" and "CREATED" not in kinds, lifetime
        else:
            assert kinds == ["CREATED", "DESTROY_BEGIN", "TEARDOWN_END"], lifetime
            assert len({event["lifetimeId"] for event in lifetime}) == 1, lifetime

    # Next passes: the results of the method callbacks, then the identity they saw at entry.
    # Each collection names a phase, which also keeps the lifetime events out of the count.
    calls = sum(map(len, expect.values()))
    session.command(f"%events --timeout 15000 --count {calls} --phase POST")
    session.run_passes(1)
    seen = {}
    for _ in range(calls):
        line = session.output.expect(r"^cursor=\d+ .* phase=POST .* function=(\S.*?) args=.* "
                                     r"result=(-?\d+)")
        seen.setdefault(line.group(1), []).append(int(line.group(2)))
    assert seen == expect, seen
    session.output.expect(rf"^received={calls} ")
    session.command(f"%events --timeout 15000 --count {calls} --phase PRE")
    session.run_passes(1)
    identities = [int(session.output.expect(r"^cursor=\d+ .* phase=PRE .* object_id=(\d+)")[1])
                  for _ in range(calls)]
    if provisional is True:
        assert identities == [0] * calls, identities
    elif provisional is False:
        # The object was constructed after the registration started, so it has an identity.
        assert all(identity > 0 for identity in identities), identities
    session.detach()
    assert entry_bytes(session, entry_addresses(fixture)) == original, "entry bytes were not restored"


def build_with_gcc(gcc, header, source, root):
    """Rebuild the fixture with GCC, plus a unit at -O2 whose virtual method has a split body."""
    fixtures = Path(header).resolve().parent
    objects = []
    for name, optimization in ((Path(source).name, ["-O0", "-fno-inline"]),
                               ("repl_virtual_main.cpp", ["-O0", "-fno-inline"]),
                               ("repl_virtual_split.cpp", ["-O2"])):
        objects.append(str(root / (Path(name).stem + ".o")))
        subprocess.run([gcc, *FLAGS, *optimization, "-I", str(fixtures), "-c",
                        str(fixtures / name), "-o", objects[-1]], check=True)
    fixture = root / "repl-virtual-gcc-target"
    subprocess.run([gcc, *objects, "-no-pie", "-Wl,--build-id=sha1", "-o", str(fixture)],
                   check=True)
    return fixture


def main(repl, server, agent, fixture, header, source, compiler, gcc=None):
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    os.environ["TRACER_PREPARE_EXECUTABLE"] = str(
        Path(repl).resolve().parent / "clang-tracer-prepare-injectors-native")
    with ExitStack() as cleanup:
        if gcc:
            version = subprocess.run([gcc, "--version"], capture_output=True, text=True).stdout
            if "clang" in version.lower() or "gcc" not in version.lower():
                print("skipped: the GCC variant needs a GCC compiler")
                sys.exit(SKIP)
            build = Path(cleanup.enter_context(tempfile.TemporaryDirectory(prefix="ct-virtual-")))
            fixture, compiler = build_with_gcc(gcc, header, source, build), gcc
        run(repl, server, agent, Path(fixture).resolve(), header, source, compiler, bool(gcc))


def run(repl, server, agent, fixture, header, source, compiler, gcc_built):
    phases = [
        ("dwarf", lambda s, r: dwarf_phase(s, fixture)),
        ("thunk", lambda s, r: thunk_phase(s, fixture)),
        ("index", lambda s, r: index_phase(s, fixture)),
        ("header", lambda s, r: header_phase(s, fixture, r, header, source, compiler)),
    ]
    if gcc_built:
        phases.append(("split", lambda s, r: split_phase(s, fixture)))
    objects = [
        # A class nothing derives from keeps the complete-object proof.
        ("square", "virt::Square",
         ("virt::Square::area() const", "virt::Square::sides() const"),
         {"virt::Square::area() const": [9], "virt::Square::sides() const": [4]}, False),
        # A polymorphic base with derived classes in debug information cannot prove it.
        ("shape", "virt::Shape", ("virt::Shape::plain() const", "virt::Shape::area() const"),
         {"virt::Shape::plain() const": [7], "virt::Shape::area() const": [1]}, True),
        ("counter", "virt::Counter", ("virt::Counter::add(int)",),
         {"virt::Counter::add(int)": [1, 3]}, None),
    ]
    for _, body in phases:
        with tempfile.TemporaryDirectory(prefix="ct-virtual-") as directory, ExitStack() as stack:
            target = Target(fixture, stack)
            session = Session(repl, server, agent, target, stack, Path(directory))
            body(session, Path(directory))
            send(target.process, "q")
            target.process.wait(timeout=10)
    for _, type_name, methods, expect, provisional in objects:
        with tempfile.TemporaryDirectory(prefix="ct-virtual-") as directory, ExitStack() as stack:
            target = Target(fixture, stack)
            session = Session(repl, server, agent, target, stack, Path(directory))
            object_phase(session, fixture, Path(directory), header, source, compiler, type_name, methods,
                         expect, provisional)
            send(target.process, "q")
            target.process.wait(timeout=10)
    print("virtual methods observed: dwarf, thunk, index, header, polymorphic objects" +
          (" (GCC-built fixture)" if gcc_built else ""))


if __name__ == "__main__":
    main(*sys.argv[1:])
