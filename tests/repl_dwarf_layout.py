"""Receiver layouts that headerless DWARF callbacks rebuild (issue #146).

With no header, a callback that read `call.self()->member` failed for any receiver that has a base
class, a bit-field, a record, union or array member or an anonymous member, with a misleading
"receiver layout changed during AST import", and a receiver with declared alignment did not even
resolve. The fixture (tests/fixtures/repl_dwarf_layout.*) has one `get() const` per class, called
once per pass on an object with fixed member values; one "g" byte makes it do one exact pass.

Checked here:
  * a dry run reports every `get() const` as DWARF_SUPPORTED, including the aligned classes;
  * a callback reads the members of each rebuilt layout with the exact values: inherited and
    second-base members, bit-fields (also an unnamed one in the source and a signed one), record,
    union, array, two-dimensional array, anonymous and unnamed-type members, a nested member
    type, and declared alignment of the class and of a member (proven by static_assert in the
    callback);
  * the controls still install: a plain class with a member callback, and every shape that is
    not rebuilt (virtual functions, a virtual base, packed members, an enumeration member, a
    pointer to a class) with a callback that reads no member;
  * a callback that reads a member of such a receiver is refused with an explicit message that
    names the receiver and the reason, not the old import error;
  * classes of the new shapes passed by value (a record member, an array, a base class, a union
    member or a union itself, bit-fields), returned by value (in registers and in memory) and
    passed by reference are observed with no callback and with one that reads the argument's members, and the
    target's own checks show neither the arguments nor the results were disturbed. A GCC-built
    fixture has no DW_AT_calling_convention, so there every by-value case is refused with the
    existing "record ABI is not fully described by DWARF" and only the by-reference case runs;
  * with the class's header (--include) a callback still reads the inherited member;
  * the target keeps running and passing once the probes are gone.
With a GCC path the fixture is rebuilt with it first (skipped, exit 77, when it is not GCC).
"""

from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from repl_cdtor import PASSES, Session, Target, send
from repl_dwarf_narrow import preview, value_of

SKIP = 77
FLAGS = ["-std=c++17", "-g", "-O0", "-fno-inline", "-no-pie", "-Wl,--build-id=sha1"]
FIXTURES = Path(__file__).resolve().parent / "fixtures"

# class: (members the callback publishes as (name, expression), expected value of each)
READS = {
    "Plain": ([("a", "a"), ("b", "b")], [1, 2]),
    "Derived": ([("base_value", "base_value"), ("own", "own")], [11, 22]),
    "Level2": ([("base_value", "base_value"), ("own", "own"), ("level", "level")], [11, 22, 55]),
    "Multi": ([("base_value", "base_value"), ("second_value", "second_value"),
               ("multi_own", "multi_own")], [11, 33, 44]),
    "Bits": ([("a", "a"), ("b", "b"), ("plain", "plain")], [1, 2, 5]),
    "BitGap": ([("a", "a"), ("b", "b"), ("s", "s"), ("flag", "static_cast<int>(call.self()->flag)"),
                ("wide", "wide")], [1, 2, -7, 1, 77]),
    "Holder": ([("in_x", "in.x"), ("y", "y")], [1, 2]),
    "UHolder": ([("u_i", "u.i"), ("z", "z")], [0, 4]),
    "Arr": ([("a0", "a[0]"), ("a3", "a[3]")], [1, 4]),
    "AnonMem": ([("a", "a"), ("x", "x"), ("y", "y"), ("u", "u")], [1, 2, 3, 4]),
    "Deep": ([("item", "items[1].x"), ("hi", "pair.hi"), ("m", "m[1][2]")], [6, 8, 5]),
    "Outer": ([("in_v", "in.v"), ("tail", "tail")], [9, 10]),
    "Aligned": ([("a", "a")], [1]),
    "MemberAligned": ([("wide", "wide")], [66]),
}
# Declared alignment is proven by the callback's own compile-time checks.
ASSERTS = {
    "Aligned": 'static_assert(alignof(tracer::ResolvedTarget) == 16, "");\n'
               'static_assert(sizeof(tracer::ResolvedTarget) == 16, "");\n',
    "MemberAligned": 'static_assert(alignof(tracer::ResolvedTarget) == 16, "");\n'
                     'static_assert(sizeof(tracer::ResolvedTarget) == 32, "");\n',
}
# Shapes that are not rebuilt: (a member the refused callback reads, what the message must name).
UNSUPPORTED = {
    "Poly": ("a", ("layout::Poly", "virtual functions")),
    "VD": ("d", ("layout::VD",)),  # "virtual base class", or "only declares" for a GCC unit
    "Packed": ("c", ("layout::Packed", "could not be rebuilt")),
    "WithEnum": ("i", ("layout::WithEnum", "'c'", "enumeration")),
    "WithPtr": ("i", ("layout::WithPtr", "'p'", "pointer to the class")),
}
# By-value and by-reference users of the new shapes: selector, a member callback body, the values.
USERS = {
    "take_holder": ("layout::take_holder(layout::Holder)",
                    [("x", "in.x"), ("y", "y")], [3, 4]),
    "take_arr": ("layout::take_arr(layout::Arr)",
                 [("a0", "a[0]"), ("a1", "a[1]"), ("a2", "a[2]"), ("a3", "a[3]")], [1, 2, 3, 4]),
    "take_derived": ("layout::take_derived(layout::Derived)",
                     [("base_value", "base_value"), ("own", "own")], [11, 22]),
    "take_uholder": ("layout::take_uholder(layout::UHolder)",
                     [("i", "u.i"), ("z", "z")], [9, 4]),
    "take_bits": ("layout::take_bits(layout::Bits)",
                  [("a", "a"), ("b", "b"), ("plain", "plain")], [1, 2, 5]),
    "take_u": ("layout::take_u(layout::U)", [("i", "i")], [7]),
    "ref_holder": ("layout::ref_holder(const layout::Holder&)",
                   [("x", "in.x"), ("y", "y")], [3, 4]),
}
RETURNERS = {"make_holder": "layout::make_holder(int)", "make_deep": "layout::make_deep()",
             "make_u": "layout::make_u(int)"}
VIRTUAL_BASE_REASONS = ("virtual base class", "only declares")


def selector(name):
    return f"layout::{name}::get() const"


def write(root, name, text):
    path = root / name
    path.write_text(text)
    return str(path)


def reader(root, name):
    """A callback that publishes the members of one class."""
    members, _ = READS[name]
    body = " ".join(f'call.field("{field}", call.self()->{expression});' if "call.self" not in
                    expression else f'call.field("{field}", {expression});'
                    for field, expression in members)
    return write(root, f"{name}.cpp", ASSERTS.get(name, "") +
                 f"class Interceptor {{ public:\n  void pre(const Call &call) {{ {body} }}\n}};\n")


def empty(root):
    return write(root, "empty.cpp",
                 "class Interceptor { public: void pre(const Call &call) { (void)call; } };\n")


def reading(root, name, member):
    return write(root, f"{name}_member.cpp",
                 "class Interceptor { public:\n"
                 f"  void pre(const Call &call) {{ call.field(\"m\", call.self()->{member}); }}\n"
                 "};\n")


def preview_phase(session):
    rows, _ = preview(session, '%trace preview --include "layout::*"')
    for name in (*READS, *UNSUPPORTED):
        assert rows.get(selector(name)) == ("DWARF_SUPPORTED", ""), (name, rows.get(selector(name)))


def control_phase(session, root):
    """Plain installs with a member callback; every unsupported shape with an empty one, and
    Derived with an empty callback; each member-reading callback on them is refused clearly."""
    session.inject(f'"{selector("Plain")}"', "--callback", reader(root, "Plain"))
    session.inject(f'"{selector("Derived")}"', "--callback", empty(root))
    for name in UNSUPPORTED:
        session.inject(f'"{selector(name)}"', "--callback", empty(root))
    for name, (member, needles) in UNSUPPORTED.items():
        error = session.inject_error(f'"{selector(name)}"', "--callback",
                                     reading(root, name, member))
        assert "layout changed during AST import" not in error, (name, error)
        assert "no members on the headerless DWARF route" in error or "could not be rebuilt" in error, \
            (name, error)
        for needle in needles:
            assert needle in error, (name, needle, error)
        if name == "VD":
            assert any(reason in error for reason in VIRTUAL_BASE_REASONS), error
        print(f"refused {name}: {error[:240]}")
    session.detach()


def values_of(event):
    return {entry["name"]: value_of(entry["value"])[0] for entry in event.get("objectFields", [])}


def inject_phase(session, root):
    """A callback reads the members of every rebuilt layout; the values are exact on every pass."""
    probes = {name: session.inject(f'"{selector(name)}"', "--callback", reader(root, name))[0]
              for name in READS}
    session.run_passes()
    for name, probe in probes.items():
        fields, expected = READS[name]
        events = session.events(probe, "PRE", PASSES, f"pre-{name}")
        assert len(events) == PASSES, (name, events)
        for event in events:
            assert values_of(event) == dict(zip((f for f, _ in fields), expected)), (name, event)
    session.detach()
    session.run_passes(1)
    assert session.target.process.poll() is None


def by_value_phase(session, root, gcc):
    """Every user and producer of the new shapes is observed, and nothing is disturbed: the target
    checks each argument and result itself and ends with status 1 on a mismatch.

    A by-value record needs the compiler's own evidence of how it is passed (DW_AT_calling_convention),
    which Clang emits and GCC 8 does not. Without it the probe is refused for every class, flat
    ones included, with the existing message; a reference has no such requirement."""
    by_reference = {"ref_holder": USERS["ref_holder"][0]}
    by_value = {**{n: u[0] for n, u in USERS.items() if n not in by_reference}, **RETURNERS}
    probes = {}
    for name, selector in by_reference.items() if gcc else {**by_value, **by_reference}.items():
        probes[name] = session.inject(f'"{selector}"')[0]
    if gcc:
        for name, selector in by_value.items():
            error = session.inject_error(f'"{selector}"')
            assert "record ABI is not fully described by DWARF" in error, (name, error)
    session.run_passes()
    for name, probe in probes.items():
        events = session.events(probe, "POST", PASSES, f"plain-{name}")
        assert len(events) == PASSES, (name, events)
    session.detach()
    session.run_passes(1)
    assert session.target.process.poll() is None


def by_value_read_phase(session, root, gcc):
    """A callback reads the members of a class passed by value, and by reference."""
    probes = {}
    for name, (selector, members, _) in USERS.items():
        if gcc and name != "ref_holder":
            continue
        body = " ".join(f'call.field("{field}", object.{expression});'
                        for field, expression in members)
        callback = write(root, f"{name}.cpp",
                         "class Interceptor { public:\n"
                         f"  void pre(const Call &call) {{ const auto &object = call.argument<0>(); "
                         f"{body} }}\n}};\n")
        probes[name] = session.inject(f'"{selector}"', "--callback", callback)[0]
    session.run_passes()
    for name, probe in probes.items():
        _, members, expected = USERS[name]
        events = session.events(probe, "PRE", PASSES, f"read-{name}")
        assert len(events) == PASSES, (name, events)
        for event in events:
            assert values_of(event) == dict(zip((f for f, _ in members), expected)), (name, event)
    session.detach()
    session.run_passes(1)
    assert session.target.process.poll() is None


def header_phase(session, root):
    """With the class's header the callback still reads the inherited member and the own one."""
    header = FIXTURES / "repl_dwarf_layout.hpp"
    probe = session.inject(f'"{selector("Derived")}"', "--callback", reader(root, "Derived"),
                           "--include", f'"{header}"')[0]
    session.run_passes()
    for event in session.events(probe, "PRE", PASSES, "pre-header"):
        assert values_of(event) == {"base_value": 11, "own": 22}, event
    session.detach()


def build_with_gcc(gcc, source, root):
    fixture = root / "repl-dwarf-layout-gcc-target"
    subprocess.run([gcc, *FLAGS, str(source), "-o", str(fixture)], check=True)
    return fixture


def main(repl, server, agent, fixture, gcc=None):
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    source = FIXTURES / "repl_dwarf_layout.cpp"
    with ExitStack() as cleanup:
        if gcc:
            version = subprocess.run([gcc, "--version"], capture_output=True, text=True).stdout
            if "clang" in version.lower() or "gcc" not in version.lower():
                print("skipped: the GCC variant needs a GCC compiler")
                sys.exit(SKIP)
            build = Path(cleanup.enter_context(tempfile.TemporaryDirectory(prefix="ct-layout-")))
            fixture = build_with_gcc(gcc, source, build)
        fixture = Path(fixture).resolve()
        # A fixture process per phase: an agent keeps a fixed number of probe slots per process.
        for phase in ("preview", "controls", "inject", "by-value", "by-value-read", "header"):
            with tempfile.TemporaryDirectory(prefix="ct-layout-run-") as directory, \
                    ExitStack() as stack:
                root = Path(directory)
                target = Target(str(fixture), stack)
                session = Session(repl, server, agent, target, stack, root)
                if phase == "preview":
                    preview_phase(session)
                    session.detach()
                elif phase == "controls":
                    control_phase(session, root)
                elif phase == "inject":
                    inject_phase(session, root)
                elif phase == "by-value":
                    by_value_phase(session, root, bool(gcc))
                elif phase == "by-value-read":
                    by_value_read_phase(session, root, bool(gcc))
                else:
                    header_phase(session, root)
                send(target.process, "q")
                target.process.wait(timeout=10)
    print("receiver layouts observed: preview, controls, diagnostics, inject, by-value, header")


if __name__ == "__main__":
    main(*sys.argv[1:])
