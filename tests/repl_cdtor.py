"""Observe constructors and destructors: every Itanium entry point, on every path.

The fixture (tests/fixtures/repl_cdtor.*) constructs and destroys a plain class (C1/C2 and D1/D2
are one body), a class template, and a virtual-inheritance hierarchy whose base-object entry
points take a hidden VTT argument and whose destructor is deleted through a base pointer (D0).
One "g" byte makes the fixture do one exact pass, so every count below is exact.
"""

from contextlib import ExitStack
import json
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import time

from repl_browser_integration import Transcript, stop

PASSES = 3

PLAIN_CTOR = "cdtor::Plain::Plain(int)"
PLAIN_COPY = "cdtor::Plain::Plain(const cdtor::Plain&)"
PLAIN_DTOR = "cdtor::Plain::~Plain()"
BOX_CTOR = "cdtor::Box<int>::Box(int)"
BOX_DTOR = "cdtor::Box<int>::~Box()"
LEAF_CTOR = "cdtor::Leaf::Leaf(int)"
HOLDER = "cdtor::Holder<std::unique_ptr<int>>"


class Output(Transcript):
    """A transcript that waits longer: installing a batch of probes compiles one observer each."""

    def expect(self, pattern, timeout=90):
        deadline = time.monotonic() + timeout
        while True:
            try:
                line = self.lines.get(timeout=max(0, deadline - time.monotonic()))
            except queue.Empty:
                line = None
            if line is None:
                raise AssertionError((pattern, "".join(self.seen)))
            self.seen.append(line)
            if match := re.search(pattern, line):
                return match


def send(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()


class Target:
    """The fixture process: one exact pass per "g" byte, and a view of its memory."""

    def __init__(self, path, stack):
        self.process = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True)
        stack.callback(stop, self.process)
        self.app = Output(self.process.stdout)
        self.app.expect(r"^READY$")
        self.memory = stack.enter_context(
            open(f"/proc/{self.process.pid}/mem", "rb", buffering=0))
        self.passes = 0

    def read(self, address, count=16):
        return os.pread(self.memory.fileno(), count, address)

    def run_passes(self, count=PASSES):
        for _ in range(count):
            send(self.process, "g")
            self.app.expect(rf"^PASS {self.passes} ")
            self.passes += 1


class Session:
    """One attached REPL."""

    def __init__(self, repl, server, agent, target, stack, root):
        self.target = target
        self.root = root
        self.console = subprocess.Popen(
            [repl, "--server", server, "--agent", agent, "--attach", str(target.process.pid),
             "--timeout", "120"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        stack.callback(stop, self.console)
        self.output = Output(self.console.stdout)
        self.output.expect(rf"^attached session=\S+ pid={target.process.pid}$")
        self.exports = 0

    def command(self, line):
        send(self.console, line)

    def read(self, address, count=16):
        return self.target.read(address, count)

    def run_passes(self, count=PASSES):
        self.target.run_passes(count)

    def inject(self, selector, *options):
        """Install a probe and return (probe id, entry address)."""
        self.command(f'%probe inject {selector} {" ".join(options)}')
        line = self.output.expect(r"^installed probe=(\d+) .*entry=(0x[0-9a-f]+)|^error: (.*)")
        assert line[1], line[3]
        return line[1], int(line[2], 16)

    def inject_error(self, selector, *options):
        self.command(f'%probe inject {selector} {" ".join(options)}')
        return self.output.expect(r"^error: (.*)")[1]

    def events(self, probe, phase, count, name):
        """Collect `count` events of one probe and phase and return them from a JSON export."""
        self.command(f"%collect --probe {probe} --phase {phase} --count {count} "
                     f"--timeout 10 --as {name}")
        self.output.expect(rf"received={count} ")
        path = self.root / f"{name}-{self.exports}.json"
        self.exports += 1
        self.command(f"%events export {name} --format json --output {path}")
        deadline = time.monotonic() + 15
        while True:
            try:
                return json.loads(path.read_text())
            except (OSError, ValueError):
                assert time.monotonic() < deadline, f"export of {name} did not appear"
                time.sleep(0.1)

    def detach(self):
        self.command("%detach")
        self.output.expect(r"^detached session=")


def argument_names(event):
    return [argument["name"] for argument in event.get("namedArguments", [])]


def argument(event, name):
    for entry in event.get("namedArguments", []):
        if entry["name"] == name:
            return int(entry["value"].get("signedValue", entry["value"].get("unsignedValue", 0)))
    raise AssertionError((name, event))


def dwarf_phase(session):
    """Probes by selector, by symbol and through the alias of a shared body; exact counts."""
    original = {}

    plain, plain_entry = session.inject(f'"{PLAIN_CTOR}"')
    copy, copy_entry = session.inject(f'"{PLAIN_COPY}"')
    plain_dtor, dtor_entry = session.inject(f'"{PLAIN_DTOR}"')
    box, box_entry = session.inject(f'"{BOX_CTOR}"')
    box_dtor, box_dtor_entry = session.inject(f'"{BOX_DTOR}"')

    # One declaration with entry points at different addresses is ambiguous by selector; the
    # error lists every entry point so a caller can pick one by its linkage symbol.
    error = session.inject_error(f'"{LEAF_CTOR}"')
    assert "ambiguous debug callable" in error, error
    listed = {session.output.expect(r"^\s+void cdtor::Leaf::Leaf\(int\) \[(\w+)\]")[1]
              for _ in range(2)}
    assert listed == {"_ZN5cdtor4LeafC1Ei", "_ZN5cdtor4LeafC2Ei"}, listed

    leaf_c1, _ = session.inject("_ZN5cdtor4LeafC1Ei")
    leaf_c2, _ = session.inject("_ZN5cdtor4LeafC2Ei")
    mid_c2, _ = session.inject("_ZN5cdtor3MidC2Ei")
    # C1 of VBase is an alias of C2 (same address); DWARF describes the body once.
    vbase, _ = session.inject("_ZN5cdtor5VBaseC1Ei")
    leaf_d0, _ = session.inject("_ZN5cdtor4LeafD0Ev")
    leaf_d1, _ = session.inject("_ZN5cdtor4LeafD1Ev")
    leaf_d2, _ = session.inject("_ZN5cdtor4LeafD2Ev")

    session.run_passes()
    n = PASSES

    def post(probe, count, name):
        return session.events(probe, "POST", count, name)

    # Exact per-pass counts: Plain(int) once, copy once, ~Plain twice, Box<int> once.
    for probe, count, name in ((plain, n, "plain"), (copy, n, "copy"), (plain_dtor, 2 * n, "pd"),
                               (box, n, "box"), (box_dtor, n, "bd")):
        events = post(probe, count, name)
        assert all(e["phase"] == "POST" for e in events), name
    constructor = post(plain, n, "plain2")
    assert [argument(e, "x") for e in constructor] == list(range(n)), constructor

    # Leaf is constructed through C1 twice per pass (stack, and new through a base pointer);
    # C2 is only used for a class derived from Leaf, so it never fires. Each C1 runs the
    # base-object constructors of Mid (with a hidden VTT argument) and VBase once.
    leaf = post(leaf_c1, 2 * n, "leaf")
    assert sorted(argument(e, "x") for e in leaf) == sorted(
        [i for i in range(n)] + [i + 1 for i in range(n)])
    assert all(argument_names(e) == ["x"] for e in leaf), leaf
    session.command(f"%collect --probe {leaf_c2} --phase POST --count 1 --timeout 2")
    session.output.expect(r"received=0 ")

    mid = post(mid_c2, 2 * n, "mid")
    # The hidden VTT pointer stays a real register argument ahead of x but out of the selector.
    assert all(len(argument_names(e)) == 2 and argument_names(e)[1] == "x" for e in mid), mid
    vtt = argument_names(mid[0])[0]
    assert vtt in ("__vtt_parm", "vtt"), vtt
    assert all(argument(e, vtt) != 0 for e in mid)
    assert sorted(argument(e, "x") for e in mid) == sorted(
        [i + 1 for i in range(n)] + [i + 2 for i in range(n)])
    assert all(e["functionName"] == "cdtor::Mid::Mid(int)" for e in mid)

    assert len(post(vbase, 2 * n, "vbase")) == 2 * n

    # The deleting destructor runs once per pass, for `delete` through a VBase pointer; the
    # complete destructor for the object on the stack. Both are observable.
    d0 = post(leaf_d0, n, "d0")
    assert all(e["functionName"] == "cdtor::Leaf::~Leaf()" for e in d0)
    assert len(post(leaf_d1, n, "d1")) >= n
    # The base-object destructor is installed too; whether anything calls it directly
    # depends on the compiler (the complete destructor may call it).
    assert leaf_d2 not in (leaf_d0, leaf_d1)

    # Every probe pairs PRE with POST.
    for probe in (leaf_c1, mid_c2, leaf_d0):
        session.command(f"%collect --probe {probe} --phase PRE --count {n} --timeout 10")
        session.output.expect(rf"received={n} ")

    session.detach()
    return original


def bulk_phase(session, bytes_at):
    """One preview and one batch install every entry point; remove restores the bytes."""
    manifest = session.root / "cdtor.textproto"
    original = {name: session.read(address) for name, address in bytes_at.items()}
    session.command('%trace preview --include "cdtor::Leaf::*" --include "cdtor::Mid::*" '
                    '--include "cdtor::VBase::*" --include "cdtor::Plain::*" '
                    '--include "cdtor::Box<int>::*" --exclude "*::id" --exclude "*::get" '
                    f'--save "{manifest}"')
    rows = []
    while True:
        line = session.output.expect(r"^(\w+) address=\S+ \"([^\"]+)\".*|^dry-run (.*)")
        if line.group(3):
            summary = line.group(3)
            break
        rows.append((line.group(1), line.group(2), line.group(0)))
    assert not [r for r in rows if r[0] != "DWARF_SUPPORTED"], rows
    counts = {}
    for _, selector, _ in rows:
        counts[selector] = counts.get(selector, 0) + 1
    # C1/C2 and D1/D2 of a shared body are one candidate; distinct bodies are one each.
    assert counts[PLAIN_CTOR] == 1 and counts[PLAIN_DTOR] == 1 and counts[BOX_CTOR] == 1, counts
    assert counts[LEAF_CTOR] == 2 and counts["cdtor::Mid::Mid(int)"] == 2, counts
    assert counts["cdtor::Leaf::~Leaf()"] == 3 and counts["cdtor::Mid::~Mid()"] == 3, counts
    leaf_rows = [r[2] for r in rows if r[1] == "cdtor::Leaf::~Leaf()"]
    assert all("constructor/destructor variant _ZN5cdtor4LeafD" in text for text in leaf_rows), rows
    session.output.expect(r"^manifest=")

    session.command(f'%trace apply --manifest "{manifest}"')
    batch = session.output.expect(r"^bulk apply starting batch=([0-9a-f]{32})$")[1]
    installed = {}
    while True:
        line = session.output.expect(
            rf'^INSTALLED "([^"]+)".*probe=(\d+) restored=false(?: reason="([^"]*)")?'
            rf'|^bulk apply batch={batch} complete=(\w+)')
        if line.group(4):
            assert line.group(4) == "true", "".join(session.output.seen)
            break
        installed.setdefault(line.group(1), []).append((line.group(2), line.group(3)))
    assert len(installed["cdtor::Leaf::~Leaf()"]) == 3
    symbols = sorted(reason.rsplit(" ", 1)[1] for _, reason in installed["cdtor::Leaf::~Leaf()"])
    assert symbols == ["_ZN5cdtor4LeafD0Ev", "_ZN5cdtor4LeafD1Ev", "_ZN5cdtor4LeafD2Ev"], symbols
    during = {name: session.read(address) for name, address in bytes_at.items()}
    assert any(during[name] != original[name] for name in original), "no entry was patched"

    session.run_passes()
    probes = [probe for entries in installed.values() for probe, _ in entries]
    n = PASSES
    d0_probe = [p for p, reason in installed["cdtor::Leaf::~Leaf()"] if reason.endswith("D0Ev")][0]
    session.command(f"%collect --probe {d0_probe} --phase POST --count {n} --timeout 10")
    session.output.expect(rf"received={n} ")
    plain_probe = installed[PLAIN_CTOR][0][0]
    session.command(f"%collect --probe {plain_probe} --phase POST --count {n} --timeout 10")
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
    assert removed == len(probes), (removed, len(probes))
    after = {name: session.read(address) for name, address in bytes_at.items()}
    assert after == original, "entry bytes were not restored"
    session.detach()


def prepared_phase(session, repl, root, header, source, compiler):
    """Prepared packages bind the complete-object entry point; D0 and C2/D2 are refused."""
    work = root / "prepared"
    work.mkdir()
    shutil.copy(header, work / "repl_cdtor.hpp")
    shutil.copy(source, work / "repl_cdtor.cpp")
    (work / "compile_commands.json").write_text(json.dumps([{
        "directory": str(work), "file": "repl_cdtor.cpp",
        "arguments": [compiler, "-std=c++17", "-c", "repl_cdtor.cpp"]}]))
    packages = {}

    def prepare(name, selector):
        output = work / f"package-{name}"
        session.command(
            f'%probe prepare "{selector}" --include "{work / "repl_cdtor.hpp"}" '
            f'--compdb "{work / "compile_commands.json"}" --output "{output}"')
        line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert line.group(1), line.group(2)
        packages[name] = output
        return line.group(1)

    assert prepare("plain", PLAIN_CTOR) == "_ZN5cdtor5PlainC1Ei"
    assert prepare("plain-dtor", PLAIN_DTOR) == "_ZN5cdtor5PlainD1Ev"
    assert prepare("box", BOX_CTOR).startswith("_ZN5cdtor3BoxIiEC1Ei")
    assert prepare("box-dtor", BOX_DTOR).startswith("_ZN5cdtor3BoxIiED1Ev")
    assert prepare("leaf", LEAF_CTOR) == "_ZN5cdtor4LeafC1Ei"
    assert prepare("leaf-dtor", "cdtor::Leaf::~Leaf()") == "_ZN5cdtor4LeafD1Ev"
    # Only the selected lifetime member of a class template is instantiated: Holder<unique_ptr>
    # has a copy() that is ill-formed for a move-only type and must not be compiled.
    assert prepare("holder", f"{HOLDER}::Holder(int)").endswith("EC1Ei")
    assert prepare("holder-dtor", f"{HOLDER}::~Holder()").endswith("ED1Ev")

    # The deleting destructor and the base-object variants cannot be called from source.
    for symbol in ("_ZN5cdtor4LeafD0Ev", "_ZN5cdtor3MidC2Ei", "_ZN5cdtor3MidD2Ev"):
        session.command(
            f'%probe prepare {symbol} --include "{work / "repl_cdtor.hpp"}" '
            f'--compdb "{work / "compile_commands.json"}" --output "{work / ("refused-" + symbol)}"')
        message = session.output.expect(r"^error: (.*)")[1]
        assert "complete-object" in message and "%probe inject SYMBOL" in message, message

    selectors = {
        "plain": PLAIN_CTOR, "plain-dtor": PLAIN_DTOR, "box": BOX_CTOR,
        "box-dtor": BOX_DTOR, "leaf": LEAF_CTOR, "leaf-dtor": "cdtor::Leaf::~Leaf()",
        "holder": f"{HOLDER}::Holder(int)", "holder-dtor": f"{HOLDER}::~Holder()",
    }
    probes = {}
    for name, selector in selectors.items():
        probes[name], _ = session.inject(f'"{selector}"', f'--prepared "{packages[name]}"',
                                         "--prepared-mode require")
    session.run_passes()
    n = PASSES

    # The receiver of a constructor is raw storage at entry: it is sampled when the call
    # returns, and the target survives the virtual-base constructor (it used to fault).
    plain = session.events(probes["plain"], "POST", n, "pplain")
    for event in plain:
        assert event["receiverSampledAtReturn"] is True, event
        assert event["receiverSnapshot"]["status"] == "COMPLETE", event
    pre = session.events(probes["plain"], "PRE", n, "pplain-pre")
    assert all("receiverSnapshot" not in event for event in pre), pre
    leaf = session.events(probes["leaf"], "POST", 2 * n, "pleaf")
    assert all(event["receiverSampledAtReturn"] is True for event in leaf)
    assert len(session.events(probes["box"], "POST", n, "pbox")) == n
    assert len(session.events(probes["box-dtor"], "POST", n, "pbox-dtor")) == n
    assert len(session.events(probes["plain-dtor"], "POST", 2 * n, "pplain-dtor")) == 2 * n
    assert len(session.events(probes["leaf-dtor"], "POST", n, "pleaf-dtor")) >= n
    assert [argument(e, "t") for e in session.events(probes["holder"], "POST", n, "pholder")] == \
        list(range(n))
    assert len(session.events(probes["holder-dtor"], "POST", n, "pholder-dtor")) == n
    session.run_passes(1)
    assert session.target.process.poll() is None
    session.detach()


def elf_phase(session, fixture):
    """Headerless (ELF symbol) packages: aliased entry points are one callable."""
    work = session.root / "elf"
    work.mkdir()
    for name, selector, signature in (("ctor", PLAIN_CTOR, "void(int)"),
                                      ("dtor", PLAIN_DTOR, "void()")):
        session.command(f'%probe prepare "{selector}" --signature "{signature}" --member '
                        f'--module "{fixture}" --output "{work / name}"')
        symbol = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
        assert symbol.group(1), symbol.group(2)
    # C1 and C2 of one body are the same ELF function; D0 and D1 are different code, so the
    # declaration still needs a linkage name to choose one.
    session.command(f'%probe prepare "cdtor::VBase::~VBase()" --signature "void()" --member '
                    f'--module "{fixture}" --output "{work / "ambiguous"}"')
    assert "multiple ELF function symbols" in session.output.expect(r"^error: (.*)")[1]
    ctor, _ = session.inject(f'"{PLAIN_CTOR}"', f'--prepared "{work / "ctor"}"',
                             "--prepared-mode require")
    dtor, _ = session.inject(f'"{PLAIN_DTOR}"', f'--prepared "{work / "dtor"}"',
                             "--prepared-mode require")
    session.run_passes()
    n = PASSES
    constructor = session.events(ctor, "POST", n, "ec")
    assert [argument(e, "arg0") for e in constructor] == list(range(n)), constructor
    assert len(session.events(dtor, "POST", 2 * n, "ed")) == 2 * n
    session.detach()


def index_phase(session, fixture):
    """Indexed preparation: concrete destructor entries are admitted whatever the compiler's
    DWARF says about virtuality; the base-object entry with a hidden VTT argument is not."""
    work = session.root / "index"
    work.mkdir()
    index = work / "fixture.sqlite"
    session.command(f'%module index --file "{fixture}" --output "{index}"')
    session.output.expect(r"^module index=")
    session.command(f"%module symbols --index \"{index}\" --filter 'cdtor::Leaf::~Leaf*' "
                    "--format json")
    entries = json.loads(session.output.expect(r"^(\{\"entries\":.*)$")[1])["entries"]
    by_symbol = {entry["linkage"]: entry for entry in entries}
    assert by_symbol["_ZN5cdtor4LeafD0Ev"]["diagnostic"] == "", by_symbol
    assert by_symbol["_ZN5cdtor4LeafD1Ev"]["diagnostic"] == "", by_symbol
    assert by_symbol["_ZN5cdtor4LeafD2Ev"]["diagnostic"] == "unsupported_artificial_parameter"
    identifier = by_symbol["_ZN5cdtor4LeafD1Ev"]["id"]
    session.command(f'%probe prepare "cdtor::Leaf::~Leaf()" --module "{fixture}" '
                    f'--module-index "{index}" --id {identifier} --output "{work / "d1"}"')
    line = session.output.expect(r"^symbol=(\S+)|^error: (.*)")
    assert line.group(1) == "_ZN5cdtor4LeafD1Ev", line.group(2)
    probe, _ = session.inject('"cdtor::Leaf::~Leaf()"', f'--prepared "{work / "d1"}"',
                              "--prepared-mode require")
    session.run_passes()
    assert len(session.events(probe, "POST", PASSES, "indexed-d1")) == PASSES
    session.detach()


def main(repl, server, agent, fixture, header, source, compiler):
    fixture = Path(fixture).resolve()
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    os.environ["TRACER_PREPARE_EXECUTABLE"] = str(
        Path(repl).resolve().parent / "clang-tracer-prepare-injectors-native")
    nm = subprocess.run(["nm", str(fixture)], check=True, capture_output=True, text=True)
    # Entry bytes of the probed functions, to compare before, during and after.
    entries = {}
    for symbol in ("_ZN5cdtor4LeafC1Ei", "_ZN5cdtor4LeafD0Ev", "_ZN5cdtor4LeafD1Ev",
                   "_ZN5cdtor3MidC2Ei", "_ZN5cdtor5VBaseD0Ev", "_ZN5cdtor5PlainC2Ei"):
        match = re.search(rf"^([0-9a-f]+) [TW] {symbol}$", nm.stdout, re.M)
        assert match, symbol
        entries[symbol] = int(match.group(1), 16)
    # A fixture process per phase: an agent keeps a fixed number of probe slots per process.
    for phase in (dwarf_phase, bulk_phase, prepared_phase, elf_phase, index_phase):
        with tempfile.TemporaryDirectory(prefix="ct-cdtor-") as directory, ExitStack() as stack:
            root = Path(directory)
            target = Target(fixture, stack)
            session = Session(repl, server, agent, target, stack, root)
            if phase is dwarf_phase:
                dwarf_phase(session)
            elif phase is bulk_phase:
                bulk_phase(session, entries)
            elif phase is prepared_phase:
                prepared_phase(session, repl, root, header, source, compiler)
            elif phase is elf_phase:
                elf_phase(session, fixture)
            else:
                index_phase(session, fixture)
            send(target.process, "q")
            target.process.wait(timeout=10)
    print("constructors and destructors observed: dwarf, bulk, prepared, elf, index")


if __name__ == "__main__":
    main(*sys.argv[1:])
