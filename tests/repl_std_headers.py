"""Standard-library records resolve their real headers automatically and carry DWARF template
arguments (issue #140).

The fixture (tests/fixtures/repl_std_headers.*) is built with the GCC 8.5 g++ the tracer SDK
ships (the test falls back to the project compiler without one) in its two libstdc++ string ABIs.
It returns and takes std::string, std::vector<Person>, std::array<int, 4>, a map with a
non-default comparator and nested specializations, and has a program-defined template that uses
every template-argument kind DWARF has. One "g" byte makes the fixture do one exact pass.

Checked here:
  * probes install with no header given, and their results are the target's values;
  * the explicit header route still works and the headers it names are used as before;
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
import subprocess
import sys
import tempfile

from repl_cdtor import Session, Target

PASSES = 3
FIXTURES = ("repl_std_headers.hpp", "repl_std_headers.cpp", "repl_std_headers_main.cpp")


def varints(payload):
    """The (field, value) pairs of a protobuf message holding only varint and bytes fields."""
    fields, index = [], 0
    while index < len(payload):
        key, index = read_varint(payload, index)
        if key & 7 == 0:
            value, index = read_varint(payload, index)
        elif key & 7 == 2:
            length, index = read_varint(payload, index)
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
    session = Session(repl, server, agent, target, stack, root)
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
    people = post("people")
    assert len(people) == PASSES and all(payload(e) for e in people)
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
    # Selecting the other string ABI is reported against what the target's DWARF says.
    error = inject_error(session, '"stdh::make_label(int)"', *options,
                                 "--compile-option", "-D_GLIBCXX_USE_CXX11_ABI=0")
    assert "libstdc++ ABI mismatch" in error and "_GLIBCXX_USE_CXX11_ABI=0" in error, error
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
            bound = next(c for c in die["children"] if c["tag"] == "DW_TAG_subrange_type")
            return {"kind": "ARRAY", "element": self.type_of(self.ref(die)),
                    "count": self.number(bound, "DW_AT_upper_bound") + 1}
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
        result["count"] = int(message["array_count"][0])
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
        # The fixture built by the GCC the tracer SDK ships, in both string ABIs.
        modern = build(compiler, link_flags, root, "target-cxx11")
        session = start(repl, server, agent, modern, stack, root)
        automatic_phase(session)
        session = start(repl, server, agent, modern, stack, root)
        unmapped_phase(session)
        session = start(repl, server, agent, modern, stack, root)
        explicit_header_phase(session, root, compiler, link_flags)
        session = start(repl, server, agent, modern, stack, root)
        mismatch_phase(session)
        legacy = build(compiler, link_flags, root, "target-cxx98", "-D_GLIBCXX_USE_CXX11_ABI=0")
        session = start(repl, server, agent, legacy, stack, root)
        old_abi_phase(session)
        if dwarfdump != "-":
            metadata_checks(repl, server, agent, modern, dwarfdump, root, stack)
        else:
            print("SKIPPED metadata comparison: no llvm-dwarfdump")
    print("standard headers and DWARF template arguments passed")


if __name__ == "__main__":
    main()
