"""Standard-library records resolve their real headers automatically and carry DWARF template
arguments (issue #140).

The fixture (tests/fixtures/repl_std_headers.*) is built with the GCC 8.5 g++ the tracer SDK
ships (the test falls back to the project compiler without one) in its two libstdc++ string ABIs.
It returns and takes std::string, std::vector<Person>, std::array<int, 4>, a map with a
non-default comparator and nested specializations, and has a program-defined template that uses
every template-argument kind DWARF has. It also has the argument types whose DWARF spelling is
not the compiler's (long, std::size_t), a std::string_view-only signature, <chrono> types,
program enumerations and an opaque program record inside standard containers, a signed
constant above 127, an array of unknown bound, a multi-dimensional array and a pointer to
function. One "g" byte makes the fixture do one exact pass.

Checked here:
  * probes install with no header given, and their results are the target's values;
  * a program type the route cannot declare is named, with the header route as the remedy;
  * the explicit header route still works, through --include, a callback #include and a forced
    include, and the headers it names are used as before;
  * an unmapped record names the exact --std-header entry that maps it, and that entry works;
  * both libstdc++ string ABIs are selected from the target's DWARF, an option that disagrees
    with the target is reported, and so is a header option that selects a different type;
  * the template arguments in the resolved metadata are the DWARF template tags, by kind.
"""

import base64
from contextlib import ExitStack
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

from repl_cdtor import Session, Target

PASSES = 3
FIXTURES = ("repl_std_headers.hpp", "repl_std_headers.cpp", "repl_std_headers_main.cpp")


def varints(payload):
    """The (field, value) pairs of a protobuf message; fixed-width values stay as bytes."""
    fields, index = [], 0
    while index < len(payload):
        key, index = read_varint(payload, index)
        if key & 7 == 0:
            value, index = read_varint(payload, index)
        elif key & 7 == 2:
            length, index = read_varint(payload, index)
            value, index = payload[index:index + length], index + length
        elif key & 7 in (1, 5):
            length = 8 if key & 7 == 1 else 4
            value, index = payload[index:index + length], index + length
        else:
            raise AssertionError(f"unexpected wire type in {payload!r}")
        fields.append((key >> 3, value))
    return fields


def read_varint(data, index):
    value = shift = 0
    while True:
        byte = data[index]
        index += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return value, index
        shift += 7


def zigzag(value):
    return (value >> 1) ^ -(value & 1)


def payload(event):
    snapshot = event["result"]["snapshot"]
    assert snapshot["status"] == "COMPLETE", snapshot
    return base64.b64decode(snapshot["payload"])


def build(compiler, link_flags, root, name, *options):
    """Compile the fixture into `root`/`name`; returns the executable."""
    out = root / name
    sources = [str(root / "src" / f) for f in FIXTURES if f.endswith(".cpp")]
    command = [compiler, "-g", "-O0", "-std=gnu++17", "-fno-inline", "-fno-omit-frame-pointer",
               "-fcf-protection=none", "-fno-pie", "-no-pie", f"-I{root / 'src'}", *options,
               *sources, "-o", str(out), "-Wl,--build-id=sha1", *link_flags]
    subprocess.run(command, check=True, capture_output=True, text=True)
    return out


def start(repl, server, agent, executable, stack, root):
    target = Target(str(executable), stack)
    # Each session exports into a directory of its own: an export never overwrites a file, so
    # a name reused by a later session must not find an earlier session's events.
    exports = Path(tempfile.mkdtemp(prefix="session-", dir=root))
    session = Session(repl, server, agent, target, stack, exports)
    # The fixture exports no dynamic symbols, so the resolver is told which image to search.
    session.module = str(executable)
    return session


def inject(session, selector, *options):
    return session.inject(selector, "--module", session.module, *options)


def inject_error(session, selector, *options):
    return session.inject_error(selector, "--module", session.module, *options)


def automatic_phase(session):
    """No header is given: every callable installs and returns the target's values."""
    probes = {}
    for key, selector in (
            ("label", '"stdh::Record::label() const"'), ("make_label", '"stdh::make_label(int)"'),
            ("people", '"stdh::make_people(int)"'), ("sum", '"stdh::sum_people(std::vector<stdh::Person, std::allocator<stdh::Person> > const&)"'),
            ("array", '"stdh::make_array(int)"'), ("ordered", '"stdh::make_ordered(int)"'),
            ("pairs", '"stdh::make_pairs(int)"')):
        try:
            probes[key], _ = inject(session, selector)
        except AssertionError as error:
            raise AssertionError(f"{selector}: {error}") from None
    session.run_passes()

    def post(key):
        return session.events(probes[key], "POST", PASSES, key)

    assert {payload_text(e) for e in post("label")} == {"rec7"}
    assert sorted(payload_text(e) for e in post("make_label")) == [
        f"label{i}" for i in range(PASSES)]
    for event in post("array"):
        base = int(event["arguments"][0]["signedValue"])
        values = [zigzag(v) for field, v in varints(payload(event)) if field == 1]
        assert values == [base + i for i in range(4)], (base, values)
    for event in post("people"):
        # One element message per Person: id (signed) and score (a double).
        people = [dict(varints(value)) for field, value in varints(payload(event)) if field == 1]
        assert [zigzag(p[1]) for p in people] == [1, 2, 3], people
        assert [struct.unpack("<d", p[2])[0] for p in people] == [0.0, 1.5, 3.0], people
    # The nested specialization is recovered whole; whether its value can also be decoded is the
    # snapshot encoder's business, so only the type identity is checked.
    for event in post("pairs"):
        snapshot = event["result"]["snapshot"]
        assert snapshot["cppType"] == "std::vector<std::pair<int, std::basic_string<char>>>", snapshot
    # A non-default comparator is part of the recovered type: the snapshot refuses to read the
    # map as if it had the default one instead of silently decoding it wrongly.
    for event in post("ordered"):
        snapshot = event["result"]["snapshot"]
        assert snapshot["cppType"] == "std::map<int, int, std::greater<int>>", snapshot
        assert snapshot["status"] == "UNSUPPORTED" and "comparator" in snapshot["detail"], snapshot
    sums = post("sum")
    assert all(e["result"]["signedValue"] == "6" for e in sums), sums
    session.detach()


def scalar(event):
    result = event["result"]
    return int(result.get("signedValue", result.get("unsignedValue", 0)))


def spelling_phase(session):
    """Types whose DWARF spelling is not the compiler's, and arguments beyond plain types."""
    probes = {}
    for key, selector in (
            ("longs", '"stdh::make_longs(int)"'), ("sizes", '"stdh::make_sizes(int)"'),
            ("view", '"stdh::view_size"'), ("delay", '"stdh::make_delay(int)"'),
            ("stamp", '"stdh::make_stamp(int)"'), ("twice", '"stdh::call_twice"'),
            ("modes", '"stdh::make_modes(int)"'), ("level", '"stdh::make_level(int)"'),
            ("named", '"stdh::make_named(int)"'), ("ratio", '"stdh::ratio_den"'),
            ("unbound", '"stdh::sum_array"'), ("grid", '"stdh::grid_sum"'),
            ("apply", '"stdh::apply_all"')):
        try:
            probes[key], _ = inject(session, selector)
        except AssertionError as error:
            raise AssertionError(f"{selector}: {error}") from None
    session.run_passes()

    def post(key):
        return session.events(probes[key], "POST", PASSES, key)

    # long and std::size_t elements: DWARF says "long int" and "long unsigned int".
    for event in post("longs"):
        values = [zigzag(v) for field, v in varints(payload(event)) if field == 1]
        assert values == [1000, 2000, 3000], values
    for event in post("sizes"):
        assert [v for field, v in varints(payload(event)) if field == 1] == [7, 8], payload(event)
    # A std::string_view and nothing else: not a statement about the std::string ABI.
    assert [scalar(e) for e in post("view")] == [4] * PASSES
    # <chrono> supplies std::ratio and the clock its own types name.
    for key, name in (("delay", "std::chrono::duration<"), ("stamp", "std::chrono::time_point<")):
        for event in post(key):
            assert event["result"]["snapshot"]["cppType"].startswith(name), event["result"]
    assert sorted(scalar(e) for e in post("twice")) == [2, 3, 4]
    # Program enumerations are declared from DWARF: scoped with a fixed type, and unscoped.
    for event in post("modes"):
        assert event["result"]["snapshot"]["cppType"] == "std::vector<stdh::Mode>", event["result"]
        assert payload(event)
    for event in post("level"):
        assert event["result"]["snapshot"]["cppType"] == "std::optional<stdh::Level>", event["result"]
        assert payload(event)
    # A vector only points at its elements, so an opaque program record is enough for it.
    assert len(post("named")) == PASSES
    # A signed constant above 127, T[], T[2][3] and a pointer to function as arguments.
    assert [scalar(e) for e in post("ratio")] == [200] * PASSES
    assert [scalar(e) for e in post("unbound")] == [6] * PASSES
    assert [scalar(e) for e in post("grid")] == [21] * PASSES
    assert sorted(scalar(e) for e in post("apply")) == [5, 7, 9]
    # Held by value, the opaque record must be complete: the probe says which type it needs.
    error = inject_error(session, '"stdh::find_named(int)"')
    assert "std::optional<stdh::Named>" in error and "template argument stdh::Named" in error, error
    assert "opaque" in error and "--include" in error, error
    session.detach()


def payload_text(event):
    fields = varints(payload(event))
    assert len(fields) == 1 and fields[0][0] == 1, fields
    return fields[0][1].decode()


def unmapped_phase(session):
    """A record with no mapping names the entry that maps it; the entry then works."""
    selector = '"stdh::midpoint(std::uniform_real_distribution<double> const&)"'
    error = inject_error(session, selector)
    assert "std::uniform_real_distribution" in error and "argument 0" in error, error
    assert "--std-header std::uniform_real_distribution=<header>" in error, error
    assert "--include" in error, error
    session.command(f"%probe inject {selector} --module {session.module} "
                    "--std-header std::uniform_real_distribution=random")
    error = session.output.expect(r"^error: (.*)")[1]
    assert "must be <name> or an absolute path" in error, error
    probe, _ = inject(session, selector, "--std-header", "std::uniform_real_distribution=<random>")
    session.run_passes(1)
    events = session.events(probe, "POST", 1, "midpoint")
    assert events and events[0]["result"]["realValue"] == 4.0, events
    session.detach()


def explicit_header_phase(session, root, compiler, link_flags):
    """The supported manual route is unchanged: real headers and the donor compile command."""
    src = root / "src"
    (root / "compile_commands.json").write_text(json.dumps([{
        "directory": str(src), "file": str(src / "repl_std_headers.cpp"),
        "arguments": [compiler, "-g", "-std=gnu++17", "-c", str(src / "repl_std_headers.cpp")]}]))
    header = src / "repl_std_headers.hpp"
    options = ("--include", str(header), "--compdb", str(root / "compile_commands.json"),
               "--translation-unit", str(src / "repl_std_headers.cpp"))
    label, _ = inject(session, '"stdh::Record::label() const"', *options)
    make_label, _ = inject(session, '"stdh::make_label(int)"', *options)
    session.run_passes()
    assert {payload_text(e) for e in session.events(label, "POST", PASSES, "xl")} == {"rec7"}
    assert len(session.events(make_label, "POST", PASSES, "xm")) == PASSES
    # A signed constant above 127 and an array of unknown bound are the header's own types.
    ratio, _ = inject(session, '"stdh::ratio_den"', *options)
    unbound, _ = inject(session, '"stdh::sum_array"', *options)
    longs, _ = inject(session, '"stdh::make_longs(int)"', *options)
    # The same headers named by the callback itself, and forced on the command line.
    hook = root / "hook.cpp"
    hook.write_text(f'#include "{header}"\n'
                    "struct Interceptor { void pre(const Call&) {} void post(const Call&) {} };\n")
    # The grammar takes --callback directly after the selector.
    included, _ = session.inject('"stdh::view_size"', "--callback", str(hook),
                                 "--module", session.module)
    forced, _ = inject(session, '"stdh::make_delay(int)"', "--compile-option", "-include",
                       "--compile-option", str(header))
    session.run_passes()
    assert [scalar(e) for e in session.events(ratio, "POST", PASSES, "xr")] == [200] * PASSES
    assert [scalar(e) for e in session.events(unbound, "POST", PASSES, "xu")] == [6] * PASSES
    assert len(session.events(longs, "POST", PASSES, "xg")) == PASSES
    assert [scalar(e) for e in session.events(included, "POST", PASSES, "xi")] == [4] * PASSES
    assert len(session.events(forced, "POST", PASSES, "xf")) == PASSES
    # Selecting the other string ABI is reported against what the target's DWARF says.
    error = inject_error(session, '"stdh::make_label(int)"', *options,
                                 "--compile-option", "-D_GLIBCXX_USE_CXX11_ABI=0")
    assert "libstdc++ ABI mismatch" in error and "_GLIBCXX_USE_CXX11_ABI=0" in error, error
    # A header mapping belongs to the automatic route only: it is not silently ignored.
    error = inject_error(session, '"stdh::make_label(int)"', *options,
                                 "--std-header", "std::basic_string=<string>")
    assert "--std-header" in error and "without headers of its own" in error, error
    session.detach()


def mismatch_phase(session):
    """An option that disagrees with the target, or selects a different type, is reported."""
    error = inject_error(session, '"stdh::Record::label() const"',
                                 "--compile-option", "-D_GLIBCXX_USE_CXX11_ABI=0")
    assert "libstdc++ ABI mismatch" in error, error
    assert "C++11 (std::__cxx11)" in error and "-D_GLIBCXX_USE_CXX11_ABI=1" in error, error
    error = inject_error(session, '"stdh::make_array(int)"', "--compile-option", "-D_GLIBCXX_DEBUG")
    assert "standard-library ABI mismatch" in error and "std::array<int, 4>" in error, error
    assert "headers chosen automatically: <array>" in error, error
    assert "libstdc++ release" in error and "target built by 'GNU C++17" in error, error
    assert "--std-header" in error, error
    # Only the macro itself counts: the split -D form sets it, a longer name does not, and -U
    # leaves the choice to the target's DWARF again.
    error = inject_error(session, '"stdh::make_label(int)"', "--compile-option", "-D",
                         "--compile-option", "_GLIBCXX_USE_CXX11_ABI=0")
    assert "libstdc++ ABI mismatch" in error, error
    inject(session, '"stdh::make_label(int)"', "--compile-option",
           "-D_GLIBCXX_USE_CXX11_ABI_OTHER=0")
    inject(session, '"stdh::Record::label() const"', "--compile-option",
           "-D_GLIBCXX_USE_CXX11_ABI=0", "--compile-option", "-U_GLIBCXX_USE_CXX11_ABI")
    session.detach()


def old_abi_phase(session):
    """A target built with _GLIBCXX_USE_CXX11_ABI=0 selects the same ABI for its headers."""
    label, _ = inject(session, '"stdh::Record::label() const"')
    session.run_passes()
    assert {payload_text(e) for e in session.events(label, "POST", PASSES, "ol")} == {"rec7"}
    error = inject_error(session, '"stdh::make_label(int)"',
                                 "--compile-option", "-D_GLIBCXX_USE_CXX11_ABI=1")
    assert "libstdc++ ABI mismatch" in error and "pre-C++11" in error, error
    assert "-D_GLIBCXX_USE_CXX11_ABI=0" in error, error
    # The option the selection added for this target is part of what a validation reports.
    hook = session.root / "hook.cpp"
    hook.write_text("struct Interceptor { void pre(const Call&) {} void post(const Call&) {} };\n")
    session.command(f'%probe validate "stdh::make_label(int)" --callback {hook} '
                    f"--module {session.module}")
    session.output.expect(r"^success: true$")
    session.output.expect(r'^effective_options: "-D_GLIBCXX_USE_CXX11_ABI=0"$')
    session.detach()


# --- DWARF metadata ----------------------------------------------------------------------

def parse_textproto(text):
    """Protobuf text format into dicts; a repeated field is a list under its name."""
    tokens = re.findall(r'"(?:[^"\\]|\\.)*"|[{}:]|[^\s{}:]+', text)
    position = 0

    def message():
        nonlocal position
        result = {}
        while position < len(tokens) and tokens[position] != "}":
            name = tokens[position]
            position += 1
            if tokens[position] == ":":
                position += 1
                value = tokens[position]
                position += 1
                if value.startswith('"'):
                    value = bytes(value[1:-1], "utf-8").decode("unicode_escape")
            else:
                assert tokens[position] == "{", (name, tokens[position])
                position += 1
                value = message()
                position += 1
            result.setdefault(name, []).append(value)
        return result

    return message()


ENCODINGS = {"DW_ATE_boolean": 2, "DW_ATE_float": 4, "DW_ATE_signed": 5, "DW_ATE_signed_char": 6,
             "DW_ATE_unsigned": 7, "DW_ATE_unsigned_char": 8}


class Dwarf:
    """The DIE tree of one binary, from llvm-dwarfdump."""

    def __init__(self, dwarfdump, binary):
        text = subprocess.run([dwarfdump, "--debug-info", str(binary)], check=True,
                              capture_output=True, text=True).stdout
        self.dies, stack = {}, []
        for line in text.splitlines():
            head = re.match(r"^(0x[0-9a-f]+):(\s+)(DW_TAG_\w+|NULL)", line)
            if head:
                depth = (len(head.group(2)) - 1) // 2
                del stack[depth:]
                if head.group(3) == "NULL":
                    continue
                die = {"offset": int(head.group(1), 16), "tag": head.group(3), "attrs": {},
                       "children": [], "parent": stack[-1] if stack else None}
                if stack:
                    stack[-1]["children"].append(die)
                self.dies[die["offset"]] = die
                stack.append(die)
                continue
            attribute = re.match(r"^\s+(DW_AT_\w+)\s+\((.*)\)$", line)
            if attribute and stack:
                stack[-1]["attrs"][attribute.group(1)] = attribute.group(2)

    @staticmethod
    def text(die, attribute):
        value = die["attrs"].get(attribute)
        return value[1:-1] if value and value.startswith('"') else None

    def ref(self, die, attribute="DW_AT_type"):
        value = die["attrs"].get(attribute)
        return self.dies[int(value.split()[0].strip("("), 16)] if value else None

    def qualified(self, die):
        parts = []
        while die:
            if die["tag"] in ("DW_TAG_namespace", "DW_TAG_structure_type", "DW_TAG_class_type",
                              "DW_TAG_union_type", "DW_TAG_enumeration_type"):
                parts.append(self.text(die, "DW_AT_name"))
            die = die["parent"]
        return "::".join(reversed(parts))

    def number(self, die, attribute):
        value = die["attrs"].get(attribute)
        return int(value, 0) if value else 0

    def type_of(self, die):
        """DWARF type DIE -> the same tree the resolver reports in DebugTemplateType."""
        if die is None:
            return {"kind": "VOID"}
        tag = die["tag"]
        if tag == "DW_TAG_typedef":
            return self.type_of(self.ref(die))
        if tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
            inner = self.type_of(self.ref(die))
            inner["const" if tag == "DW_TAG_const_type" else "volatile"] = True
            return inner
        if tag == "DW_TAG_pointer_type":
            return {"kind": "POINTER", "element": self.type_of(self.ref(die))}
        if tag == "DW_TAG_base_type":
            return {"kind": "BUILTIN", "name": self.text(die, "DW_AT_name"),
                    "size": self.number(die, "DW_AT_byte_size"),
                    "encoding": ENCODINGS[die["attrs"]["DW_AT_encoding"]]}
        if tag == "DW_TAG_enumeration_type":
            return {"kind": "ENUM", "name": self.qualified(die)}
        if tag == "DW_TAG_array_type":
            # One subrange per dimension, outermost first; none of them bounded for T[].
            result = self.type_of(self.ref(die))
            for bound in reversed([c for c in die["children"]
                                   if c["tag"] == "DW_TAG_subrange_type"]):
                result = {"kind": "ARRAY", "element": result}
                if "DW_AT_count" in bound["attrs"]:
                    result["count"] = self.number(bound, "DW_AT_count")
                elif "DW_AT_upper_bound" in bound["attrs"]:
                    result["count"] = self.number(bound, "DW_AT_upper_bound") + 1
            return result
        if tag == "DW_TAG_subroutine_type":
            return {"kind": "FUNCTION", "element": self.type_of(self.ref(die)),
                    "parameters": [self.type_of(self.ref(c)) for c in die["children"]
                                   if c["tag"] == "DW_TAG_formal_parameter"]}
        if tag in ("DW_TAG_structure_type", "DW_TAG_class_type", "DW_TAG_union_type"):
            return {"kind": "RECORD", "name": self.qualified(die)}
        raise AssertionError(f"unmodeled DWARF type {tag}")

    def argument(self, parameter):
        tag = parameter["tag"]
        name = self.text(parameter, "DW_AT_name") or ""
        if tag == "DW_TAG_template_type_parameter":
            return {"kind": "TYPE", "name": name, "type": self.type_of(self.ref(parameter))}
        if tag == "DW_TAG_template_value_parameter":
            type_die = self.ref(parameter)
            kind = self.type_of(type_die)
            raw = parameter["attrs"]["DW_AT_const_value"]
            value = int(raw, 0)
            size = kind.get("size") or 1
            signed = kind["kind"] == "ENUM" or kind.get("encoding") in (5, 6)
            if raw.startswith("0x") and signed and value >= 1 << (size * 8 - 1) and kind["kind"] != "ENUM":
                value -= 1 << (size * 8)
            return {"kind": "VALUE", "name": name, "type": kind, "value": str(value)}
        if tag == "DW_TAG_GNU_template_template_param":
            return {"kind": "TEMPLATE", "name": name,
                    "template": self.text(parameter, "DW_AT_GNU_template_name")}
        assert tag == "DW_TAG_GNU_template_parameter_pack", tag
        return {"kind": "PACK", "name": name,
                "elements": [self.argument(c) for c in parameter["children"]]}

    def template_arguments(self, record):
        return [self.argument(c) for c in record["children"] if c["tag"] in (
            "DW_TAG_template_type_parameter", "DW_TAG_template_value_parameter",
            "DW_TAG_GNU_template_template_param", "DW_TAG_GNU_template_parameter_pack")]

    def record_named(self, qualified_name):
        found = [d for d in self.dies.values()
                 if d["tag"] in ("DW_TAG_structure_type", "DW_TAG_class_type")
                 and any(c["tag"].startswith("DW_TAG_template") or c["tag"].startswith("DW_TAG_GNU")
                         for c in d["children"])
                 and self.qualified(d) == qualified_name]
        assert found, qualified_name
        return found[0]


def wire_type(message):
    """The resolver's DebugTemplateType (parsed text) -> the tree Dwarf.type_of builds."""
    kind = message.get("kind", ["UNSPECIFIED"])[0]
    result = {"kind": kind}
    if message.get("is_const"):
        result["const"] = True
    if message.get("is_volatile"):
        result["volatile"] = True
    if kind == "BUILTIN":
        result.update(name=message["name"][0], size=int(message["byte_size"][0]),
                      encoding=int(message["dwarf_encoding"][0]))
    elif kind in ("ENUM", "RECORD"):
        result["name"] = message["name"][0]
    elif kind == "POINTER":
        result["element"] = wire_type(message["element"][0])
    elif kind == "ARRAY":
        result["element"] = wire_type(message["element"][0])
        if message.get("has_array_count") == ["true"]:
            result["count"] = int(message.get("array_count", ["0"])[0])
    elif kind == "FUNCTION":
        result["element"] = wire_type(message["element"][0])
        result["parameters"] = [wire_type(p) for p in message.get("parameters", [])]
    return result


def wire_argument(message):
    kind = message["kind"][0]
    result = {"kind": kind, "name": message.get("parameter_name", [""])[0]}
    if kind == "TYPE":
        result["type"] = wire_type(message["type"][0])
    elif kind == "VALUE":
        result["type"] = wire_type(message["type"][0])
        assert message["has_constant_value"] == ["true"], message
        result["value"] = message["constant_value"][0]
    elif kind == "TEMPLATE":
        result["template"] = message["template_name"][0]
    else:
        result["elements"] = [wire_argument(e) for e in message.get("pack_elements", [])]
    return result


def metadata_phase(session, dwarfdump, executable, selector, in_result):
    """The template arguments the resolver reports are the record's DWARF template tags."""
    session.command(f'%probe resolve "{selector}" --module {executable}')
    text = []
    while True:
        line = session.output.expect(r"^(.*)$")[1]
        if line.startswith("debug_info {"):
            text = [line]
            break
    while True:
        line = session.output.expect(r"^(.*)$")[1]
        text.append(line)
        if line == "}":
            break
    spec = parse_textproto("\n".join(text[1:-1]))
    holder = spec["result_object_type"][0] if in_result else spec["arguments"][0]["object_type"][0]
    name = holder["record_name"][0]
    reported = [wire_argument(a) for a in holder["template_arguments"]]
    dwarf = Dwarf(dwarfdump, executable)
    expected = dwarf.template_arguments(dwarf.record_named(name))
    assert reported == expected, (json.dumps(reported, indent=1), json.dumps(expected, indent=1))
    assert [a["kind"] for a in reported], name
    return name, reported


def metadata_checks(repl, server, agent, executable, dwarfdump, root, stack):
    session = start(repl, server, agent, executable, stack, root)
    # A program-defined template with every argument kind, by value as a result.
    _, gadget = metadata_phase(session, dwarfdump, executable, "stdh::make_gadget()", True)
    assert [a["kind"] for a in gadget] == [
        "TYPE", "VALUE", "VALUE", "VALUE", "VALUE", "TEMPLATE", "PACK"], gadget
    assert gadget[1]["value"] == "-3" and gadget[3]["value"] == "122", gadget
    assert [e["type"]["kind"] for e in gadget[6]["elements"]] == [
        "BUILTIN", "ARRAY", "FUNCTION", "POINTER"], gadget[6]
    # Standard-library records: a type and a constant, and a non-default comparator.
    name, array = metadata_phase(session, dwarfdump, executable, "stdh::make_array(int)", True)
    assert name == "std::array<int, 4>" and [a["kind"] for a in array] == ["TYPE", "VALUE"], array
    assert array[1]["value"] == "4", array
    name, ordered = metadata_phase(session, dwarfdump, executable, "stdh::make_ordered(int)", True)
    assert name.startswith("std::map<int, int, std::greater<int>"), name
    assert [a["kind"] for a in ordered] == ["TYPE"] * 4, ordered
    assert ordered[2]["type"]["name"] == "std::greater<int>", ordered
    # A signed constant that needs more than seven bits is read through its type, not its form.
    name, ratio = metadata_phase(session, dwarfdump, executable, "stdh::ratio_den", False)
    assert name == "std::ratio<1, 200>" and [a["value"] for a in ratio] == ["1", "200"], ratio
    # An array of unknown bound, and every dimension of a multi-dimensional one.
    _, unbound = metadata_phase(session, dwarfdump, executable, "stdh::sum_array", False)
    assert unbound[0]["type"] == {"kind": "ARRAY", "element": {
        "kind": "BUILTIN", "name": "int", "size": 4, "encoding": 5}}, unbound
    _, grid = metadata_phase(session, dwarfdump, executable, "stdh::grid_sum", False)
    assert grid[0]["type"]["count"] == 2 and grid[0]["type"]["element"]["count"] == 3, grid
    assert grid[0]["type"]["element"]["element"]["kind"] == "BUILTIN", grid
    _, modes = metadata_phase(session, dwarfdump, executable, "stdh::make_modes(int)", True)
    assert modes[0]["type"] == {"kind": "ENUM", "name": "stdh::Mode"}, modes
    session.detach()


def main():
    repl, server, agent, project_exe, fixtures, compiler, link_flags, dwarfdump = sys.argv[1:9]
    link_flags = link_flags.split() if link_flags != "-" else []
    fixtures = Path(fixtures)
    with ExitStack() as stack:
        root = Path(tempfile.mkdtemp(prefix="repl-std-headers-"))
        stack.callback(shutil.rmtree, root, ignore_errors=True)
        (root / "src").mkdir()
        for name in FIXTURES:
            shutil.copy(fixtures / name, root / "src" / name)
        # The project-built fixture: whichever compiler built the tracer.
        session = start(repl, server, agent, project_exe, stack, root)
        automatic_phase(session)
        session = start(repl, server, agent, project_exe, stack, root)
        spelling_phase(session)
        # The fixture built by the GCC the tracer SDK ships, in both string ABIs.
        modern = build(compiler, link_flags, root, "target-cxx11")
        session = start(repl, server, agent, modern, stack, root)
        automatic_phase(session)
        session = start(repl, server, agent, modern, stack, root)
        spelling_phase(session)
        session = start(repl, server, agent, modern, stack, root)
        unmapped_phase(session)
        session = start(repl, server, agent, modern, stack, root)
        explicit_header_phase(session, root, compiler, link_flags)
        session = start(repl, server, agent, modern, stack, root)
        mismatch_phase(session)
        legacy = build(compiler, link_flags, root, "target-cxx98", "-D_GLIBCXX_USE_CXX11_ABI=0")
        session = start(repl, server, agent, legacy, stack, root)
        old_abi_phase(session)
        if dwarfdump == "-":
            # Everything above passed, but the comparison with the DWARF tags did not run:
            # report the test as skipped (ctest SKIP_RETURN_CODE) rather than as passed.
            print("SKIPPED metadata comparison: no llvm-dwarfdump; the other phases passed")
            sys.exit(77)
        metadata_checks(repl, server, agent, modern, dwarfdump, root, stack)
    print("standard headers and DWARF template arguments passed")


if __name__ == "__main__":
    main()
