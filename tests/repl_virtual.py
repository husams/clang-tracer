"""Probe virtual methods, say why a pure declaration cannot be probed, and observe polymorphic
receivers.

The fixture (tests/fixtures/repl_virtual.*) has a polymorphic hierarchy whose methods are defined
out of line and called through base references, a pure virtual declaration that has no body
anywhere, an override reached through an adjusting thunk, and a non-polymorphic control. One "g"
byte makes the fixture do one exact pass, so every count below is exact.
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
}
PURE = "virt::Shape::sides() const"
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
    session.command('%trace preview --include "virt::*" --exclude "virt::Counter::*"')
    rows = []
    while True:
        line = session.output.expect(r"^(\w+) address=\S+ \"([^\"]+)\".*|^dry-run (.*)")
        if line.group(3):
            break
        rows.append((line.group(1), line.group(2), line.group(0)))
    assert rows and all(status in ("DWARF_SUPPORTED", "ALIAS") for status, _, _ in rows), rows
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

    probes = {}
    for name in ("shape_area", "square_area", "square_sides", "plain_sides", "mixed_tag"):
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


def header_phase(session, root, header, source, compiler):
    """Route 4: the headers resolve overrides; a pure declaration lists the ones they define."""
    work = root / "header"
    work.mkdir()
    shutil.copy(header, work / "repl_virtual.hpp")
    shutil.copy(source, work / "repl_virtual.cpp")
    (work / "compile_commands.json").write_text(json.dumps([{
        "directory": str(work), "file": "repl_virtual.cpp",
        "arguments": [compiler, "-std=c++17", "-c", "repl_virtual.cpp"]}]))
    base = (f'--include "{work / "repl_virtual.hpp"}" --compdb "{work / "compile_commands.json"}" '
            f'--translation-unit "{work / "repl_virtual.cpp"}"')
    probes = {}
    for name in ("shape_area", "square_area", "square_sides", "plain_sides"):
        selector, _ = METHODS[name]
        session.command(f'%probe prepare "{selector}" {base} --output "{work / name}"')
        line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        probes[name], _ = session.inject(f'"{selector}"', f'--prepared "{work / name}"',
                                         "--prepared-mode require")
    session.command(f'%probe prepare "{PURE}" {base} --output "{work / "pure"}"')
    # The error line and the listed overrides come from two streams, in either order.
    message, listed = None, set()
    while message is None or len(listed) < 3:
        line = session.output.expect(r"^error: (.*)|^\s+(virt::\w+::sides\(\) const)$")
        if line.group(1):
            message = line.group(1)
        else:
            listed.add(line.group(2))
    assert "no concrete implementation entry" in message, message
    # The supplied headers define these overrides; the diagnostic lists them and picks none.
    assert listed == {"virt::Square::sides() const", "virt::Plain::sides() const",
                      "virt::Mixed::sides() const"}, listed
    assert not (work / "pure").exists()
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
    started = session.output.expect(r"^object-probe registration=\d+ active=yes (.*)|^error: (.*)")
    assert started.group(1), started.group(2)
    if provisional is True:
        assert "object identities remain provisional" in started.group(1), started.group(1)
        assert f"polymorphic type {type_name}" in started.group(1), started.group(1)
    elif provisional is False:
        assert "polymorphic type" not in started.group(1), started.group(1)
    session.command("%events --timeout 15000 --count " + str(sum(map(len, expect.values()))) +
                    " --phase POST")
    session.run_passes(1)
    seen = {}
    for _ in range(sum(map(len, expect.values()))):
        line = session.output.expect(r"^cursor=\d+ .* phase=POST .* function=(\S.*?) args=.* "
                                     r"result=(-?\d+)")
        seen.setdefault(line.group(1), []).append(int(line.group(2)))
    assert seen == expect, seen
    session.detach()
    assert entry_bytes(session, entry_addresses(fixture)) == original, "entry bytes were not restored"


def main(repl, server, agent, fixture, header, source, compiler):
    fixture = Path(fixture).resolve()
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    os.environ["TRACER_PREPARE_EXECUTABLE"] = str(
        Path(repl).resolve().parent / "clang-tracer-prepare-injectors-native")
    phases = [
        ("dwarf", lambda s, r: dwarf_phase(s, fixture)),
        ("thunk", lambda s, r: thunk_phase(s, fixture)),
        ("index", lambda s, r: index_phase(s, fixture)),
        ("header", lambda s, r: header_phase(s, r, header, source, compiler)),
    ]
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
    print("virtual methods observed: dwarf, thunk, index, header, polymorphic objects")


if __name__ == "__main__":
    main(*sys.argv[1:])
