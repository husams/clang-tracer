"""Enumerations and member pointers in DWARF-resolved signatures (issue #136).

The fixture (tests/fixtures/repl_dwarf_enum.cpp) has functions and methods that take and return
enumerations (a namespace-scope unscoped enum, scoped enums over long long and unsigned long, and
an enum nested in a class, by value and by reference), and others the tracer must reject with a
diagnostic that names the cause: enums over unsigned int and unsigned char, a pointer to a data
member and a pointer to a member function. One "g" byte makes the fixture do one exact pass, so
every count below is exact.

Checked here:
  * the supported signatures resolve, with the carrier of the enum's underlying type and the
    enumeration named in the metadata;
  * the others fail to resolve and preview as UNSUPPORTED with the enumeration or member pointer,
    its role and its callable named, and never with an empty type name;
  * a typed Interceptor<Signature> naming the enums validates, a mismatching one does not;
  * the callbacks see the exact values, and a bulk apply of the supported callables installs,
    traces and restores the entry bytes.
With a GCC path the fixture is rebuilt with it first (skipped, exit 77, when it is not GCC).
"""

from contextlib import ExitStack
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from repl_cdtor import PASSES, Session, Target, argument, send
from repl_std_headers import parse_textproto

SKIP = 77
FLAGS = ["-std=c++17", "-g", "-O0", "-fno-inline", "-no-pie", "-Wl,--build-id=sha1"]

PLAIN = "enums::Order::plain(int)"
SET_STATE = "enums::Order::set_state(enums::Order::State)"
STATE_OF = "enums::Order::state_of(int)"
BY_REF = "enums::Order::by_ref(const enums::Order::State&)"
FLIP = "enums::flip(enums::Sign)"
WIDEN = "enums::widen(enums::Wide)"
BIG = "enums::big(enums::Big)"
PAINT = "enums::paint(enums::Color)"
NARROW = "enums::narrow(enums::Narrow)"
READ = "enums::Order::read(int enums::Order::*)"
CALL = "enums::call(enums::Order*, int (enums::Order::*)(int))"

SUPPORTED = (SET_STATE, STATE_OF, BY_REF, FLIP, WIDEN, BIG)
UNSUPPORTED = (PAINT, NARROW, READ, CALL)

STATE, SIGN, WIDE, BIG_TYPE = "enums::Order::State", "enums::Sign", "enums::Wide", "enums::Big"


def resolve(session, selector):
    """(parsed spec, None) for a callable that resolves, (None, message) for one that does not."""
    session.command(f'%probe resolve "{selector}"')
    line = session.output.expect(r"^symbol: |^error: (.*)")
    if line.group(1) is not None:
        return None, line.group(1)
    text, closed = [], 0
    while closed < 2:  # `signature {...}` and `debug_info {...}` close at column 0.
        current = session.output.expect(r"^(.*)$")[1]
        text.append(current)
        closed += current == "}"
    return parse_textproto("\n".join(text)), None


def one(message, name):
    values = message.get(name)
    assert values and len(values) == 1, (name, message)
    return values[0]


def resolve_phase(session, root):
    """Carriers and enumerations of the supported callables; the diagnostics of the others."""
    expected = {
        # selector: (result carrier, argument carriers, result enum, argument enums)
        SET_STATE: ("INT32", ["INT32"], None, [STATE]),
        STATE_OF: ("INT32", ["INT32"], STATE, [None]),
        BY_REF: ("INT32", ["POINTER"], None, [STATE]),
        FLIP: ("INT32", ["INT32"], SIGN, [SIGN]),
        WIDEN: ("INT64", ["INT64"], WIDE, [WIDE]),
        BIG: ("INT32", ["UINT64"], None, [BIG_TYPE]),
        PLAIN: ("INT32", ["INT32"], None, [None]),
    }
    for selector, (result, arguments, result_enum, argument_enums) in expected.items():
        spec, error = resolve(session, selector)
        assert spec, (selector, error)
        signature = one(spec, "signature")
        assert one(signature, "result") == result, (selector, signature)
        assert signature["arguments"] == arguments, (selector, signature)
        info = one(spec, "debug_info")
        if result_enum:
            enum = one(info, "result_enum_type")
            assert one(enum, "name") == result_enum and one(enum, "kind") == "ENUM", enum
        else:
            assert "result_enum_type" not in info, (selector, info)
        for argument_info, enum_name in zip(info["arguments"], argument_enums):
            if enum_name:
                enum = one(argument_info, "enum_type")
                assert one(enum, "name") == enum_name and one(enum, "kind") == "ENUM", enum
                assert one(enum, "byte_size") == ("8" if enum_name in (WIDE, BIG_TYPE) else "4")
                scoped = enum_name != SIGN
                assert ("enum_scoped" in enum) == scoped, (selector, enum)
                underlying = one(enum, "element")
                assert one(underlying, "kind") == "BUILTIN", enum
            else:
                assert "enum_type" not in argument_info, (selector, argument_info)
    # The reference parameter is a reference to the enumeration, not an enumeration by value.
    by_ref, _ = resolve(session, BY_REF)
    assert one(one(one(by_ref, "debug_info"), "arguments"), "is_reference") == "true"
    assert one(one(one(by_ref, "debug_info"), "arguments"), "cpp_type") == f"const {STATE}&"

    errors = {}
    for selector in UNSUPPORTED:
        spec, error = resolve(session, selector)
        assert spec is None and error, selector
        assert not re.search(r"unsupported debug type: *$", error), error
        errors[selector] = error
    for selector, enum, size, underlying in ((PAINT, "enums::Color", "4 bytes", "unsigned int"),
                                             (NARROW, "enums::Narrow", "1 byte", "unsigned char")):
        error = errors[selector]
        role = "the result of " + selector.split("(")[0]
        assert f"unsupported debug enumeration {enum} for {role}: underlying type {underlying} " \
            f"({size}) has no exact DWARF trace value type" in error, error
    error = errors[READ]
    assert "unsupported debug type for parameter 0 'field' of enums::Order::read: " \
        "pointer to data member int enums::Order::* (DW_TAG_ptr_to_member_type); " \
        "member pointers are not supported" in error, error
    error = errors[CALL]
    assert "unsupported debug type for parameter 1 'function' of enums::call: " \
        "pointer to member function int (enums::Order::*)(int) (DW_TAG_ptr_to_member_type); " \
        "member pointers are not supported" in error, error


def preview(session, command):
    """{selector: (status, reason)} of one dry run."""
    session.command(command)
    rows = {}
    while True:
        line = session.output.expect(
            r'^(\w+) address=\S+ "([^"]+)" symbol=\S+ module="[^"]*"(?: reason="([^"]*)")?'
            r"|^dry-run (.*)")
        if line.group(4):
            return rows, line.group(4)
        rows[line.group(2)] = (line.group(1), line.group(3) or "")


def preview_phase(session):
    rows, _ = preview(session, '%trace preview --include "enums::*"')
    for selector in SUPPORTED + (PLAIN,):
        assert rows[selector] == ("DWARF_SUPPORTED", ""), (selector, rows[selector])
    for selector in UNSUPPORTED:
        status, reason = rows[selector]
        assert status == "UNSUPPORTED" and reason, (selector, rows[selector])
        assert not reason.rstrip().endswith("unsupported debug type:"), reason
    assert "unsupported debug enumeration enums::Color" in rows[PAINT][1], rows[PAINT]
    assert "unsupported debug enumeration enums::Narrow" in rows[NARROW][1], rows[NARROW]
    assert "pointer to data member" in rows[READ][1] and "parameter 0" in rows[READ][1], rows[READ]
    assert "pointer to member function" in rows[CALL][1] and "parameter 1" in rows[CALL][1], \
        rows[CALL]


def validate(session, selector, callback):
    """None when the callback validates; otherwise its diagnostics and the error line."""
    session.command(f'%probe validate "{selector}" --callback "{callback}"')
    seen = []
    while True:
        line = session.output.expect(r"^(.*)$")[1]
        if line == "success: true":
            return None
        seen.append(line)
        if line.startswith("error:"):
            return "\n".join(seen)


def write(root, name, text):
    path = root / name
    path.write_text(text)
    return path


def validate_phase(session, root):
    """Typed callbacks that name the enums validate; a signature of other types does not."""
    typed = {
        SET_STATE: f"int({STATE})",
        STATE_OF: f"{STATE}(int)",
        FLIP: f"{SIGN}({SIGN})",
        WIDEN: f"{WIDE}({WIDE})",
        BIG: f"int({BIG_TYPE})",
        BY_REF: f"int(const {STATE}&)",
    }
    for selector, signature in typed.items():
        callback = write(root, "typed.cpp", f"""template <> class Interceptor<{signature}> {{
public:
  void pre(const Call& call) {{ (void)call; }}
}};
""")
        error = validate(session, selector, callback)
        assert error is None, (selector, error)
    callback = write(root, "arguments.cpp", f"""template <> class Interceptor<int({STATE})> {{
public:
  void pre(const Call& call) {{ (void)call.arg<{STATE}>(0); }}
}};
""")
    assert validate(session, SET_STATE, callback) is None
    callback = write(root, "result.cpp", f"""template <> class Interceptor<{STATE}(int)> {{
public:
  void post(const Call& call) {{ (void)call.result<{STATE}>(); }}
}};
""")
    assert validate(session, STATE_OF, callback) is None

    # The enumeration is not its underlying type, its address or its reference, and the other way
    # round. Only the callable's own enumerations are declared for the callback.
    for selector, signature in ((SET_STATE, "int(int)"), (SET_STATE, f"int({STATE}*)"),
                                (SET_STATE, f"int(const {STATE}&)"), (SET_STATE, f"{STATE}({STATE})"),
                                (STATE_OF, "int(int)"), (STATE_OF, f"{STATE}*(int)"),
                                (FLIP, f"int({SIGN})"), (FLIP, f"{SIGN}(int)"),
                                (BY_REF, f"int({STATE})"), (BY_REF, f"int({STATE}&)"),
                                (WIDEN, f"{WIDE}(long long)"), (BIG, "int(unsigned long)")):
        callback = write(root, "mismatch.cpp", f"""template <> class Interceptor<{signature}> {{
public:
  void pre(const Call& call) {{ (void)call; }}
}};
""")
        error = validate(session, selector, callback)
        assert error and "does not match" in error, (selector, signature, error)


def inject_phase(session, root):
    """The callbacks see exact enum values at entry and at return."""
    def callback(name, signature, pre="", post=""):
        return write(root, name, f"""template <> class Interceptor<{signature}> {{
public:
  void pre(const Call& call) {{ {pre} }}
  void post(const Call& call) {{ {post} }}
}};
""")

    set_state, _ = session.inject(f'"{SET_STATE}"', "--callback", str(callback(
        "set_state.cpp", f"int({STATE})",
        pre=f'call.field("state", static_cast<int>(call.arg<{STATE}>(0)));')))
    state_of, _ = session.inject(f'"{STATE_OF}"', "--callback", str(callback(
        "state_of.cpp", f"{STATE}(int)",
        post=f'call.field("state", static_cast<int>(call.result<{STATE}>()));')))
    flip, _ = session.inject(f'"{FLIP}"', "--callback", str(callback(
        "flip.cpp", "tracer::ResolvedSignature",
        pre=f'call.field("before", static_cast<int>(call.arg<{SIGN}>(0)));',
        post=f'call.field("after", static_cast<int>(call.result<{SIGN}>()));')))
    widen, _ = session.inject(f'"{WIDEN}"', "--callback", str(callback(
        "widen.cpp", f"{WIDE}({WIDE})",
        pre=f'call.field("before", static_cast<long long>(call.arg<{WIDE}>(0)));',
        post=f'call.field("after", static_cast<long long>(call.result<{WIDE}>()));')))
    big, _ = session.inject(f'"{BIG}"', "--callback", str(callback(
        "big.cpp", f"int({BIG_TYPE})",
        pre=f'call.field("size", static_cast<unsigned long long>(call.arg<{BIG_TYPE}>(0)));')))
    by_ref, _ = session.inject(f'"{BY_REF}"')
    session.run_passes()
    n = PASSES

    def post(probe, count, name):
        return session.events(probe, "POST", count, name)

    # Argument and result values come from the carrier of the underlying type.
    events = post(set_state, n, "set-state")
    assert [argument(e, "s") for e in events] == [1] * n, events
    assert all(int(e["result"]["signedValue"]) == 1 for e in events), events
    events = post(state_of, 2 * n, "state-of")
    assert sorted(argument(e, "v") for e in events) == [0] * n + [1] * n, events
    assert sorted(int(e["result"]["signedValue"]) for e in events) == [0] * n + [1] * n, events
    events = post(flip, 2 * n, "flip")
    assert sorted((argument(e, "s"), int(e["result"]["signedValue"])) for e in events) == \
        [(-1, 1)] * n + [(1, -1)] * n, events
    events = post(widen, 2 * n, "widen")
    pairs = sorted((argument(e, "w"), int(e["result"]["signedValue"])) for e in events)
    assert pairs == [(-5, 1 << 40)] * n + [(1 << 40, -5)] * n, pairs
    events = post(big, 2 * n, "big")
    pairs = sorted((argument(e, "b"), int(e["result"]["signedValue"])) for e in events)
    assert pairs == [(1, 1)] * n + [(1 << 63, 2)] * n, pairs
    assert all("unsignedValue" in e["namedArguments"][0]["value"] for e in events), events
    # A reference to the enumeration is its address.
    events = post(by_ref, n, "by-ref")
    assert all(argument(e, "s") != 0 and int(e["result"]["signedValue"]) == 11 for e in events), \
        events

    # What the callbacks recorded, through the enumerations the callback source declared.
    def recorded(probe, phase, count, name, field):
        found = []
        for event in session.events(probe, phase, count, name):
            for entry in event.get("objectFields", []):
                if entry["name"] == field:
                    found.append(int(entry["value"].get("signedValue",
                                                        entry["value"].get("unsignedValue"))))
        return sorted(found)

    assert recorded(set_state, "PRE", n, "set-state-pre", "state") == [1] * n
    assert recorded(state_of, "POST", 2 * n, "state-of-post", "state") == [0] * n + [1] * n
    assert recorded(flip, "PRE", 2 * n, "flip-pre", "before") == [-1] * n + [1] * n
    assert recorded(flip, "POST", 2 * n, "flip-post", "after") == [-1] * n + [1] * n
    assert recorded(widen, "PRE", 2 * n, "widen-pre", "before") == [-5] * n + [1 << 40] * n
    assert recorded(widen, "POST", 2 * n, "widen-post", "after") == [-5] * n + [1 << 40] * n
    assert recorded(big, "PRE", 2 * n, "big-pre", "size") == [1] * n + [1 << 63] * n
    session.run_passes(1)
    assert session.target.process.poll() is None
    session.detach()


def bulk_phase(session, root, bytes_at):
    """One preview and one batch install every supported callable; remove restores the bytes."""
    manifest = root / "enums.textproto"
    original = {name: session.read(address) for name, address in bytes_at.items()}
    exact = " ".join(f'--exact "{selector}"' for selector in SUPPORTED + (PLAIN,))
    rows, summary = preview(session, f'%trace preview {exact} --save "{manifest}"')
    assert set(rows) == set(SUPPORTED + (PLAIN,)), rows
    assert all(status == "DWARF_SUPPORTED" for status, _ in rows.values()), rows
    assert "dwarf_supported=7" in summary, summary
    session.output.expect(r"^manifest=")

    session.command(f'%trace apply --manifest "{manifest}"')
    batch = session.output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
    installed = {}
    while True:
        line = session.output.expect(
            r'^INSTALLED "([^"]+)".*probe=(\d+) restored=false'
            rf"|^bulk apply batch={batch} complete=(\w+)")
        if line.group(3):
            assert line.group(3) == "true", "".join(session.output.seen)
            break
        installed[line.group(1)] = line.group(2)
    assert set(installed) == set(SUPPORTED + (PLAIN,)), installed
    during = {name: session.read(address) for name, address in bytes_at.items()}
    assert any(during[name] != original[name] for name in original), "no entry was patched"

    session.run_passes()
    n = PASSES
    session.command(f"%collect --probe {installed[BIG]} --phase POST --count {2 * n} --timeout 10")
    session.output.expect(rf"received={2 * n} ")
    session.command(f"%collect --probe {installed[SET_STATE]} --phase POST --count {n} --timeout 10")
    session.output.expect(rf"received={n} ")

    session.command(f"%trace remove {batch}")
    removed = 0
    while True:
        line = session.output.expect(
            r"^REMOVED .*restored=true|^bulk remove batch=\S+ complete=(\w+)")
        if line.group(1):
            assert line.group(1) == "true"
            break
        removed += 1
    assert removed == len(installed), (removed, installed)
    assert {name: session.read(address) for name, address in bytes_at.items()} == original, \
        "entry bytes were not restored"
    # The target keeps passing, with the exact values, once its entries are restored.
    session.run_passes(1)
    assert session.target.process.poll() is None
    session.detach()


def build_with_gcc(gcc, source, root):
    fixture = root / "repl-dwarf-enum-gcc-target"
    subprocess.run([gcc, *FLAGS, str(source), "-o", str(fixture)], check=True)
    return fixture


def main(repl, server, agent, fixture, gcc=None):
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    source = Path(__file__).resolve().parent / "fixtures" / "repl_dwarf_enum.cpp"
    with ExitStack() as cleanup:
        if gcc:
            version = subprocess.run([gcc, "--version"], capture_output=True, text=True).stdout
            if "clang" in version.lower() or "gcc" not in version.lower():
                print("skipped: the GCC variant needs a GCC compiler")
                sys.exit(SKIP)
            build = Path(cleanup.enter_context(tempfile.TemporaryDirectory(prefix="ct-enum-")))
            fixture = build_with_gcc(gcc, source, build)
        fixture = Path(fixture).resolve()
        nm = subprocess.run(["nm", str(fixture)], check=True, capture_output=True, text=True)
        entries = {}
        for name, symbol in (("set_state", "_ZN5enums5Order9set_stateENS0_5StateE"),
                             ("state_of", "_ZN5enums5Order8state_ofEi"),
                             ("flip", "_ZN5enums4flipENS_4SignE"),
                             ("widen", "_ZN5enums5widenENS_4WideE"),
                             ("big", "_ZN5enums3bigENS_3BigE"),
                             ("plain", "_ZN5enums5Order5plainEi")):
            match = re.search(rf"^([0-9a-f]+) [TW] {symbol}$", nm.stdout, re.M)
            assert match, symbol
            entries[name] = int(match.group(1), 16)
        # A fixture process per phase: an agent keeps a fixed number of probe slots per process.
        for phase in ("resolve", "inject", "bulk"):
            with tempfile.TemporaryDirectory(prefix="ct-enum-run-") as directory, \
                    ExitStack() as stack:
                root = Path(directory)
                target = Target(str(fixture), stack)
                session = Session(repl, server, agent, target, stack, root)
                if phase == "resolve":
                    resolve_phase(session, root)
                    preview_phase(session)
                    validate_phase(session, root)
                    session.detach()
                elif phase == "inject":
                    inject_phase(session, root)
                else:
                    bulk_phase(session, root, entries)
                send(target.process, "q")
                target.process.wait(timeout=10)
    print("enumerations observed: resolve, preview, validate, inject, bulk")


if __name__ == "__main__":
    main(*sys.argv[1:])
