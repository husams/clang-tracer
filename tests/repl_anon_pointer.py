"""Probe functions that take or return a pointer to a type in an anonymous namespace.

The fixture (tests/fixtures/anon_pointer/*) has two translation units in one executable and two
shared libraries, each defining its own `Anon` with a different layout, plus a named-type control
and a function-local control. One "g" byte makes it do one exact pass, so every count is exact.
"""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import sys
import tempfile

from repl_cdtor import Session, Target, argument, send

PASSES = 3
ANON = "(anonymous namespace)"

PLAIN = "anonp::take_plain(anonp::Plain*)"
TAKE_A = f"anonp::take_a(anonp::{ANON}::Anon*)"
TAKE_B = f"anonp::take_b(anonp::{ANON}::Anon*)"
MAKE_A = "anonp::make_a(int)"
MAKE_B = "anonp::make_b(int)"
LOCAL = "anonp::take_local"
VALUE = "anonp::take_value"
LIB_TAKE = f"anonl::take(anonl::{ANON}::Anon*)"
LIB_MAKE = "anonl::make(int)"

CALLBACKS = {
    "ab": 'class Interceptor { public: void pre(const Call &call) { const auto &p = '
          'call.argument<0>(); call.field("first", p->a); call.field("second", p->b); } };',
    "xyz": 'class Interceptor { public: void pre(const Call &call) { const auto &p = '
           'call.argument<0>(); call.field("first", p->x); call.field("second", p->y); '
           'call.field("third", p->z); } };',
    "none": "class Interceptor { public: void pre(const Call &call) { (void)call.arg<void*>(0); } };",
}


def expected_sum(index):
    """The result of one pass of the fixture; probes must not change it."""
    unit_a = 1 + (3 + index) * 10 + 4 + (3 + index)
    unit_b = (5 + index) * 100 + 6 * 10 + 7
    library_one = 11 + index + 12
    library_two = 21 + index + 22 + 23
    return unit_a + unit_b + 9 + library_one + library_two


def run_passes(session, count=PASSES, start=0):
    """Run `count` passes and check the target's own result of each one."""
    for index in range(start, start + count):
        send(session.target.process, "g")
        line = session.target.app.expect(rf"^PASS {index} (-?\d+)")
        assert int(line[1]) == expected_sum(index), (index, line[0])


def preview(session, *options):
    """Run `%trace preview` and return its rows as (status, selector, text)."""
    session.command("%trace preview " + " ".join(options))
    rows = []
    while True:
        line = session.output.expect(r"^([A-Z_]+) address=\S+ \"([^\"]+)\".*|^dry-run (.*)")
        if line.group(3):
            return rows
        rows.append((line.group(1), line.group(2), line.group(0)))


def only(rows, selector):
    found = [row for row in rows if row[1] == selector]
    assert len(found) == 1, (selector, found)
    return found[0]


def write_callbacks(root):
    paths = {}
    for name, text in CALLBACKS.items():
        paths[name] = root / f"{name}.cpp"
        paths[name].write_text(text + "\n")
    return paths


def preview_phase(session):
    """Each demonstrated function is one supported row, whichever spelling the compiler's debug
    information gives it; the function that cannot be probed keeps its own reason."""
    rows = preview(session, '--include "anonp::*"')
    for selector in (PLAIN, TAKE_A, TAKE_B, MAKE_A, MAKE_B):
        status, _, text = only(rows, selector)
        assert status == "DWARF_SUPPORTED", text
    # A function-local class was never affected and still has a single row.
    status, _, text = only(rows, only_local(rows))
    assert status == "DWARF_SUPPORTED", text
    # (GCC leaves such a function without a linkage name, so its row has no parameter list)
    value = [row for row in rows if row[1].startswith(VALUE)]
    assert len(value) == 1, value
    status, _, text = value[0]
    assert status == "UNSUPPORTED" and "anonymous-namespace record by value" in text, text
    assert not [r for r in rows if r[0] in ("ALIAS", "AMBIGUOUS")], rows
    for module in ("anon_one", "anon_two"):
        library = preview(session, '--include "anonl::*"', f'--module "lib{module}.*"')
        for selector in (LIB_TAKE, LIB_MAKE):
            status, _, text = only(library, selector)
            assert status == "DWARF_SUPPORTED", text
        assert not [r for r in library if r[0] in ("ALIAS", "AMBIGUOUS")], library
    session.detach()


def only_local(rows):
    names = [row[1] for row in rows if row[1].startswith(LOCAL)]
    assert len(names) == 1, names
    return names[0]


def pointer(event):
    return int(event["result"]["unsignedValue"])


def first_argument(event):
    return int(event["namedArguments"][0]["value"]["unsignedValue"])


def fields(event):
    return {field["name"]: int(field["value"].get("signedValue", 0))
            for field in event.get("objectFields", [])}


def inject_phase(session, addresses):
    """Pointer parameters and results install, report the pointer and read members from the
    layout of the right type; the target's results never change."""
    callbacks = write_callbacks(session.root)

    def probe(selector, callback, *options):
        return session.inject(f'"{selector}"', "--callback", f'"{callbacks[callback]}"', *options)[0]

    take_a = probe(TAKE_A, "ab")
    take_b = probe(TAKE_B, "xyz")
    make_a, make_b = (session.inject(f'"{MAKE_A}"')[0], session.inject(f'"{MAKE_B}"')[0])
    plain = session.inject(f'"{PLAIN}"')[0]
    local = session.inject(f'"{only_local_selector(session)}"')[0]
    one_take = probe(LIB_TAKE, "ab", "--module", "libanon_one")
    two_take = probe(LIB_TAKE, "xyz", "--module", "libanon_two")
    one_make = session.inject(f'"{LIB_MAKE}"', "--module", "libanon_one")[0]
    two_make = session.inject(f'"{LIB_MAKE}"', "--module", "libanon_two")[0]

    error = session.inject_error(f'"{VALUE}(anonp::{ANON}::Anon)"')
    assert "anonymous-namespace record by value is unsupported" in error, error

    run_passes(session)
    n = PASSES

    def pre(name, count, callback_probe):
        return session.events(callback_probe, "PRE", count, name)

    def post(name, probe_id):
        return session.events(probe_id, "POST", n, name)

    # Address-only capture reports the right pointer, and the result of make_*() is that pointer.
    for take, make, name in ((take_a, make_a, "a"), (take_b, make_b, "b")):
        arguments = pre("take-" + name, n, take)
        results = post("make-" + name, make)
        assert {first_argument(e) for e in arguments} == {addresses[name]}, arguments
        assert {pointer(e) for e in results} == {addresses[name]}, results
    # Each type's members come from its own layout.
    assert [fields(e) for e in pre("fields-a", n, take_a)] == [
        {"first": 3 + i, "second": 4} for i in range(n)]
    assert [fields(e) for e in pre("fields-b", n, take_b)] == [
        {"first": 5 + i, "second": 6, "third": 7} for i in range(n)]
    one = pre("fields-one", n, one_take)
    two = pre("fields-two", n, two_take)
    assert [fields(e) for e in one] == [{"first": 11 + i, "second": 12} for i in range(n)], one
    assert [fields(e) for e in two] == [
        {"first": 21 + i, "second": 22, "third": 23} for i in range(n)], two
    # The pointer a library's make() returns is the one its take() receives.
    for make, take, name in ((one_make, one_take, "one"), (two_make, two_take, "two")):
        results = {pointer(e) for e in post("make-" + name, make)}
        arguments = {first_argument(e) for e in pre("take-" + name, n, take)}
        assert len(results) == 1 and results == arguments and results != {0}, (results, arguments)
    assert {pointer(e) for e in post("make-one", one_make)} != \
        {pointer(e) for e in post("make-two", two_make)}
    # Controls: the named type and the function-local class.
    assert len(post("plain", plain)) == n and len(post("local", local)) == n

    # The target keeps running unchanged.
    run_passes(session, 1, n)
    assert session.target.process.poll() is None
    session.detach()


def only_local_selector(session):
    return only_local(preview(session, '--include "anonp::take_local*"'))


def bulk_phase(session, addresses):
    """One preview and one batch install the anonymous-pointer cases; removal restores them."""
    manifest = session.root / "anon.textproto"
    rows = preview(session, '--include "anonp::take_a*"', '--include "anonp::take_b*"',
                   '--include "anonp::make_a*"', '--include "anonp::make_b*"',
                   f'--save "{manifest}"')
    session.output.expect(r"^manifest=")
    assert sorted(row[1] for row in rows) == sorted([TAKE_A, TAKE_B, MAKE_A, MAKE_B]), rows
    assert all(row[0] == "DWARF_SUPPORTED" for row in rows), rows

    session.command(f'%trace apply --manifest "{manifest}"')
    batch = session.output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
    installed = {}
    while True:
        line = session.output.expect(
            rf'^INSTALLED "([^"]+)".*probe=(\d+) restored=false'
            rf'|^bulk apply batch={batch} complete=(\w+)')
        if line.group(3):
            assert line.group(3) == "true", "".join(session.output.seen)
            break
        installed[line.group(1)] = line.group(2)
    assert sorted(installed) == sorted([TAKE_A, TAKE_B, MAKE_A, MAKE_B]), installed

    run_passes(session)
    n = PASSES
    arguments = session.events(installed[TAKE_B], "PRE", n, "bulk-take-b")
    assert {first_argument(e) for e in arguments} == {addresses["b"]}, arguments
    results = session.events(installed[MAKE_A], "POST", n, "bulk-make-a")
    assert {pointer(e) for e in results} == {addresses["a"]}, results

    session.command(f"%trace remove {batch}")
    removed = 0
    while True:
        line = session.output.expect(r"^REMOVED .*restored=true|^bulk remove batch=\S+ complete=(\w+)")
        if line.group(1):
            assert line.group(1) == "true"
            break
        removed += 1
    assert removed == len(installed), removed
    run_passes(session, 1, n)
    session.detach()


def main(repl, server, agent, fixture):
    fixture = Path(fixture).resolve()
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    # A fixture process per phase: an agent keeps a fixed number of probe slots per process.
    for phase in (preview_phase, inject_phase, bulk_phase):
        with tempfile.TemporaryDirectory(prefix="ct-anon-") as directory, ExitStack() as stack:
            target = Target(str(fixture), stack)
            address = re.search(r"a=(0x[0-9a-f]+) b=(0x[0-9a-f]+)", "".join(target.app.seen))
            assert address, target.app.seen
            addresses = {"a": int(address[1], 16), "b": int(address[2], 16)}
            session = Session(repl, server, agent, target, stack, Path(directory))
            if phase is preview_phase:
                phase(session)
            else:
                phase(session, addresses)
            send(target.process, "q")
            target.process.wait(timeout=10)
    print("anonymous-namespace pointers: preview, inject, bulk")


if __name__ == "__main__":
    main(*sys.argv[1:])
