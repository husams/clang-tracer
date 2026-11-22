"""Narrow and unsigned 32-bit integers in DWARF-resolved signatures (issue #145).

The fixture (tests/fixtures/repl_dwarf_narrow.cpp) has functions that take and return char,
signed char, unsigned char, short, unsigned short, unsigned int, char16_t, char32_t and wchar_t, a
by-value const short, a typedef of unsigned char, a function that mixes six narrow arguments and a
class with narrow methods. The int, long, unsigned long, bool and double functions are controls
that worked before, and __int128 and long double are still unsupported. One "g" byte makes the
fixture do one exact pass, so every count below is exact; the pass also calls the unsigned int
function with garbage in the upper half of the argument register.

Checked here:
  * the supported signatures resolve with the carrier of their signedness (int32 for signed, uint64
    for unsigned) and keep the exact C++ type in the metadata; the unsupported ones fail with the
    type and its role named;
  * a dry run reports the same, and a typed Interceptor<Signature> naming the exact types
    validates while a mismatching one does not;
  * the callbacks see the exact values, signed ones as signed and unsigned ones as unsigned, at
    entry and at return, through call.arg<T>(), call.argument<N>() and call.result(), and the
    target's own checks prove its results are unchanged;
  * a bulk apply of the supported callables installs, traces and restores the entry bytes.
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

# selector: (result carrier, result type, [argument carriers], [argument types])
SUPPORTED = {
    "narrow::t_char(char)": ("INT32", "char", ["INT32"], ["char"]),
    "narrow::t_schar(signed char)": ("INT32", "signed char", ["INT32"], ["signed char"]),
    "narrow::t_uchar(unsigned char)": ("UINT64", "unsigned char", ["UINT64"], ["unsigned char"]),
    "narrow::t_short(short)": ("INT32", "short", ["INT32"], ["short"]),
    "narrow::t_ushort(unsigned short)": ("UINT64", "unsigned short", ["UINT64"],
                                         ["unsigned short"]),
    "narrow::t_uint(unsigned int)": ("UINT64", "unsigned int", ["UINT64"], ["unsigned int"]),
    "narrow::t_c16(char16_t)": ("UINT64", "char16_t", ["UINT64"], ["char16_t"]),
    "narrow::t_c32(char32_t)": ("UINT64", "char32_t", ["UINT64"], ["char32_t"]),
    "narrow::t_wchar(wchar_t)": ("INT32", "wchar_t", ["INT32"], ["wchar_t"]),
    "narrow::t_int(int)": ("INT32", "int", ["INT32"], ["int"]),
    "narrow::t_long(long)": ("INT64", "long", ["INT64"], ["long"]),
    "narrow::t_ulong(unsigned long)": ("UINT64", "unsigned long", ["UINT64"], ["unsigned long"]),
    "narrow::t_bool(bool)": ("BOOL", "bool", ["BOOL"], ["bool"]),
    "narrow::t_double(double)": ("FLOAT64", "double", ["FLOAT64"], ["double"]),
    "narrow::t_cshort(const short)": ("INT32", "int", ["INT32"], ["const short"]),
    "narrow::t_byte(unsigned char)": ("UINT64", "unsigned char", ["UINT64"], ["unsigned char"]),
    "narrow::t_mix(signed char, unsigned char, short, unsigned short, unsigned int, char16_t)": (
        "INT32", "int", ["INT32", "UINT64", "INT32", "UINT64", "UINT64", "UINT64"],
        ["signed char", "unsigned char", "short", "unsigned short", "unsigned int", "char16_t"]),
    "narrow::Meter::mix(unsigned char, short)": (
        "INT32", "short", ["UINT64", "INT32"], ["unsigned char", "short"]),
    "narrow::Meter::widen(unsigned short)": (
        "UINT64", "unsigned short", ["UINT64"], ["unsigned short"]),
}
# selector: (the part of the role, the unsupported type)
UNSUPPORTED = {
    "narrow::t_i128(__int128)": ("parameter 0 'v' of narrow::t_i128", "__int128"),
    "narrow::t_ldouble(long double)": ("the result of narrow::t_ldouble", "long double"),
    "narrow::t_ldarg(long double)": ("parameter 0 'v' of narrow::t_ldarg", "long double"),
}

# The selector of each function by its short name, for the callbacks.
SELECTORS = {selector.split("(")[0].split("::", 1)[1]: selector for selector in SUPPORTED}


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


def resolve_phase(session):
    """Carriers and C++ types of the supported callables; the diagnostics of the others."""
    for selector, (result, result_type, arguments, types) in SUPPORTED.items():
        spec, error = resolve(session, selector)
        assert spec, (selector, error)
        signature = one(spec, "signature")
        assert one(signature, "result") == result, (selector, signature)
        assert signature["arguments"] == arguments, (selector, signature)
        info = one(spec, "debug_info")
        assert one(info, "result_cpp_type") == result_type, (selector, info)
        assert [one(a, "cpp_type") for a in info["arguments"]] == types, (selector, info)
    for selector, (role, unsupported) in UNSUPPORTED.items():
        spec, error = resolve(session, selector)
        assert spec is None and error, selector
        assert f"unsupported debug builtin for {role}: {unsupported}" in error, (selector, error)


def preview(session, command):
    """({selector: (status, reason)}, summary) of one dry run."""
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
    rows, _ = preview(session, '%trace preview --include "narrow::*"')
    for selector in SUPPORTED:
        assert rows.get(selector) == ("DWARF_SUPPORTED", ""), (selector, sorted(rows))
    for selector, (role, unsupported) in UNSUPPORTED.items():
        status, reason = rows[selector]
        assert status == "UNSUPPORTED", (selector, rows[selector])
        assert f"unsupported debug builtin for {role}: {unsupported}" in reason, rows[selector]


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


def interceptor(root, signature, pre="", post=""):
    return write(root, "callback.cpp", f"""template <> class Interceptor<{signature}> {{
public:
  void pre(const Call& call) {{ {pre or "(void)call;"} }}
  void post(const Call& call) {{ {post or "(void)call;"} }}
}};
""")


def validate_phase(session, root):
    """Typed callbacks that name the exact types validate; a signature of other types does not."""
    for selector, (_, result_type, _, types) in SUPPORTED.items():
        # A top-level const on a by-value parameter is not part of the function type.
        signature = f"{result_type}({', '.join(t.removeprefix('const ') for t in types)})"
        error = validate(session, selector, interceptor(root, signature))
        assert error is None, (selector, signature, error)

    callback = interceptor(
        root, "unsigned char(unsigned char)",
        pre="(void)call.arg<unsigned char>(0);", post="(void)call.result<unsigned char>();")
    assert validate(session, SELECTORS["t_uchar"], callback) is None
    callback = interceptor(
        root, "unsigned int(unsigned int)",
        pre="(void)call.arg<unsigned int>(0);", post="(void)call.result<unsigned int>();")
    assert validate(session, SELECTORS["t_uint"], callback) is None

    # A narrow integer is not the wider type its carrier suggests, nor one of another signedness.
    for name, signature in (("t_uchar", "int(int)"), ("t_uchar", "unsigned char(int)"),
                            ("t_uchar", "int(unsigned char)"), ("t_uchar", "unsigned char(char)"),
                            ("t_uint", "int(unsigned long)"), ("t_uint", "unsigned int(int)"),
                            ("t_uint", "unsigned long(unsigned int)"),
                            ("t_short", "short(int)"), ("t_short", "int(int)"),
                            ("t_short", "short(unsigned short)"),
                            ("t_int", "short(int)"), ("t_char", "int(char)"),
                            ("t_c16", "unsigned short(char16_t)"),
                            ("t_c32", "char32_t(unsigned int)")):
        error = validate(session, SELECTORS[name], interceptor(root, signature))
        assert error and "does not match" in error, (name, signature, error)


def value_of(entry):
    """The integer of a trace value, and whether it was carried as signed."""
    if "signedValue" in entry:
        return int(entry["signedValue"]), True
    return int(entry.get("unsignedValue", 0)), False


def inject_phase(session, root):
    """The callbacks see exact values at entry and at return; the target's results are unchanged."""
    def callback(name, signature, pre="", post=""):
        return str(interceptor(root, signature, pre, post))

    plain = ("schar", "ushort", "int", "long", "ulong", "bool", "double", "cshort", "byte")
    probes = {name: session.inject(f'"{SELECTORS["t_" + name]}"')[0] for name in plain}
    probes["widen"] = session.inject(f'"{SELECTORS["Meter::widen"]}"')[0]

    probes["char"] = session.inject(f'"{SELECTORS["t_char"]}"', "--callback", callback(
        "char", "char(char)",
        pre='call.field("arg", call.arg<char>(0));',
        post='call.field("result", call.result<char>());'))[0]
    probes["uchar"] = session.inject(f'"{SELECTORS["t_uchar"]}"', "--callback", callback(
        "uchar", "unsigned char(unsigned char)",
        pre='call.field("arg", call.arg<unsigned char>(0));',
        post='call.field("result", call.result<unsigned char>());'))[0]
    probes["short"] = session.inject(f'"{SELECTORS["t_short"]}"', "--callback", callback(
        "short", "short(short)",
        pre='call.field("arg", call.arg<short>(0));',
        post='call.field("result", call.result<short>());'))[0]
    probes["uint"] = session.inject(f'"{SELECTORS["t_uint"]}"', "--callback", callback(
        "uint", "unsigned int(unsigned int)",
        pre='call.field("arg", call.arg<unsigned int>(0)); '
            'call.field("again", call.argument<0>());',
        post='call.field("result", call.result<unsigned int>()); '
             'call.field("again", call.result());'))[0]
    probes["c16"] = session.inject(f'"{SELECTORS["t_c16"]}"', "--callback", callback(
        "c16", "char16_t(char16_t)",
        pre='call.field("arg", call.arg<char16_t>(0));',
        post='call.field("result", call.result<char16_t>());'))[0]
    probes["c32"] = session.inject(f'"{SELECTORS["t_c32"]}"', "--callback", callback(
        "c32", "char32_t(char32_t)",
        pre='call.field("arg", call.arg<char32_t>(0));',
        post='call.field("result", call.result<char32_t>());'))[0]
    probes["wchar"] = session.inject(f'"{SELECTORS["t_wchar"]}"', "--callback", callback(
        "wchar", "wchar_t(wchar_t)",
        pre='call.field("arg", call.arg<wchar_t>(0));',
        post='call.field("result", call.result<wchar_t>());'))[0]
    probes["mix"] = session.inject(f'"{SELECTORS["t_mix"]}"', "--callback", callback(
        "mix", "int(signed char, unsigned char, short, unsigned short, unsigned int, char16_t)",
        pre='call.field("a", call.arg<signed char>(0)); call.field("b", call.argument<1>()); '
            'call.field("c", call.arg<short>(2)); call.field("d", call.argument<3>()); '
            'call.field("e", call.arg<unsigned int>(4)); call.field("f", call.argument<5>());',
        post='call.field("sum", call.result());'))[0]
    probes["mix_method"] = session.inject(f'"{SELECTORS["Meter::mix"]}"', "--callback", callback(
        "meter", "short(unsigned char, short)",
        pre='call.field("a", call.arg<unsigned char>(0)); call.field("b", call.argument<1>());',
        post='call.field("out", call.result());'))[0]
    session.run_passes()
    n = PASSES

    def post(name, count=n):
        return session.events(probes[name], "POST", count, f"post-{name}")

    def recorded(name, phase, field, count=n):
        found = []
        for event in session.events(probes[name], phase, count, f"{phase.lower()}-{name}-{field}"):
            for entry in event.get("objectFields", []):
                if entry["name"] == field:
                    found.append(value_of(entry["value"]))
        return found

    # (argument, result, argument carried as signed, result carried as signed) of every probed
    # function with a single argument named `v`, as the target passed and got them. The target
    # checks the results itself; the events must say the same.
    expected = {
        "char": (-5, -7, True, True), "schar": (-128, 127, True, True),
        "uchar": (250, 5, False, False), "short": (-3, -32768, True, True),
        "ushort": (65535, 1, False, False), "uint": (4000000000, 3000000000, False, False),
        "c16": (0xFFFE, 0x8001, False, False), "c32": (0x10FFFF, 0xFFFFFFFE, False, False),
        "wchar": (-9, -70000, True, True), "int": (-1, 2, True, True),
        "long": (-(1 << 40), 1 << 41, True, True), "ulong": (1 << 63, 5, False, False),
        "cshort": (-3, 33, True, True), "byte": (250, 6, False, False),
    }
    for name, (value, result, signed_argument, signed_result) in expected.items():
        # The unsigned int function is also called with garbage above its argument.
        count = 2 * n if name == "uint" else n
        events = post(name, count)
        assert len(events) == count, (name, events)
        for event in events:
            carried = value_of(next(a["value"] for a in event["namedArguments"] if a["name"] == "v"))
            assert carried == (value, signed_argument), (name, event)
            assert value_of(event["result"]) == (result, signed_result), (name, event)
    events = post("bool")
    assert all(argument(e, "v") == 0 and value_of(e["result"]) == (1, True) for e in events), events
    events = post("widen")
    assert all(argument(e, "v") == 65535 and value_of(e["result"]) == (2, False)
               for e in events), events

    # What the callbacks read through the typed accessors.
    for name, phase, field, values in (
            ("char", "PRE", "arg", [(-5, True)] * n), ("char", "POST", "result", [(-7, True)] * n),
            ("uchar", "PRE", "arg", [(250, False)] * n), ("uchar", "POST", "result", [(5, False)] * n),
            ("short", "PRE", "arg", [(-3, True)] * n),
            ("short", "POST", "result", [(-32768, True)] * n),
            ("uint", "PRE", "arg", [(4000000000, False)] * (2 * n)),
            ("uint", "PRE", "again", [(4000000000, False)] * (2 * n)),
            ("uint", "POST", "result", [(3000000000, False)] * (2 * n)),
            ("uint", "POST", "again", [(3000000000, False)] * (2 * n)),
            ("c16", "PRE", "arg", [(0xFFFE, False)] * n),
            ("c16", "POST", "result", [(0x8001, False)] * n),
            ("c32", "PRE", "arg", [(0x10FFFF, False)] * n),
            ("c32", "POST", "result", [(0xFFFFFFFE, False)] * n),
            ("wchar", "PRE", "arg", [(-9, True)] * n),
            ("wchar", "POST", "result", [(-70000, True)] * n),
            ("mix", "PRE", "a", [(-1, True)] * n), ("mix", "PRE", "b", [(200, False)] * n),
            ("mix", "PRE", "c", [(-300, True)] * n), ("mix", "PRE", "d", [(60000, False)] * n),
            ("mix", "PRE", "e", [(3000000000, False)] * n),
            ("mix", "PRE", "f", [(0xABCD, False)] * n),
            ("mix", "POST", "sum", [(-1 + 200 - 300 + 60000 + 45776 + 0xABCD, True)] * n),
            ("mix_method", "PRE", "a", [(250, False)] * n),
            ("mix_method", "PRE", "b", [(-3, True)] * n),
            ("mix_method", "POST", "out", [(-53, True)] * n)):
        count = len(values)
        assert recorded(name, phase, field, count) == values, (name, phase, field)
    events = post("mix")
    assert [argument(events[0], name) for name in "abcdef"] == \
        [-1, 200, -300, 60000, 3000000000, 0xABCD], events[0]

    # More passes after the probes are gone.
    session.detach()
    session.run_passes(1)
    assert session.target.process.poll() is None


def bulk_phase(session, root, bytes_at):
    """One preview and one batch install every supported callable; remove restores the bytes."""
    manifest = root / "narrow.textproto"
    original = {name: session.read(address) for name, address in bytes_at.items()}
    exact = " ".join(f'--exact "{selector}"' for selector in SUPPORTED)
    rows, summary = preview(session, f'%trace preview {exact} --save "{manifest}"')
    assert set(rows) == set(SUPPORTED), sorted(rows)
    assert all(status == "DWARF_SUPPORTED" for status, _ in rows.values()), rows
    assert f"dwarf_supported={len(SUPPORTED)}" in summary, summary
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
    assert set(installed) == set(SUPPORTED), installed
    during = {name: session.read(address) for name, address in bytes_at.items()}
    assert any(during[name] != original[name] for name in original), "no entry was patched"

    session.run_passes()
    n = PASSES
    for selector in (SELECTORS["t_uint"], SELECTORS["t_uchar"], SELECTORS["t_mix"],
                     SELECTORS["Meter::mix"]):
        count = 2 * n if selector == SELECTORS["t_uint"] else n
        session.command(f"%collect --probe {installed[selector]} --phase POST --count {count} "
                        "--timeout 10")
        session.output.expect(rf"received={count} ")

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
    fixture = root / "repl-dwarf-narrow-gcc-target"
    subprocess.run([gcc, *FLAGS, str(source), "-o", str(fixture)], check=True)
    return fixture


def entry_addresses(fixture):
    """The entry address of every supported function, from the symbol table."""
    nm = subprocess.run(["nm", "-C", str(fixture)], check=True, capture_output=True, text=True)
    entries = {}
    for selector in SUPPORTED:
        # The symbol is the demangled function type, which has no top-level const.
        symbol = selector.replace("const ", "")
        match = re.search(rf"^([0-9a-f]+) [TW] {re.escape(symbol)}$", nm.stdout, re.M)
        assert match, selector
        entries[selector] = int(match.group(1), 16)
    return entries


def main(repl, server, agent, fixture, gcc=None):
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    source = Path(__file__).resolve().parent / "fixtures" / "repl_dwarf_narrow.cpp"
    with ExitStack() as cleanup:
        if gcc:
            version = subprocess.run([gcc, "--version"], capture_output=True, text=True).stdout
            if "clang" in version.lower() or "gcc" not in version.lower():
                print("skipped: the GCC variant needs a GCC compiler")
                sys.exit(SKIP)
            build = Path(cleanup.enter_context(tempfile.TemporaryDirectory(prefix="ct-narrow-")))
            fixture = build_with_gcc(gcc, source, build)
        fixture = Path(fixture).resolve()
        entries = entry_addresses(fixture)
        # A fixture process per phase: an agent keeps a fixed number of probe slots per process.
        for phase in ("resolve", "inject", "bulk"):
            with tempfile.TemporaryDirectory(prefix="ct-narrow-run-") as directory, \
                    ExitStack() as stack:
                root = Path(directory)
                target = Target(str(fixture), stack)
                session = Session(repl, server, agent, target, stack, root)
                if phase == "resolve":
                    resolve_phase(session)
                    preview_phase(session)
                    validate_phase(session, root)
                    session.detach()
                elif phase == "inject":
                    inject_phase(session, root)
                else:
                    bulk_phase(session, root, entries)
                send(target.process, "q")
                target.process.wait(timeout=10)
    print("narrow integers observed: resolve, preview, validate, inject, bulk")


if __name__ == "__main__":
    main(*sys.argv[1:])
