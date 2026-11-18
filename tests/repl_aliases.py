"""Several names for one entry (assembler alias, linker-folded functions, C1/C2 constructors)
are one callable: bulk trace and single probes observe every name through one probe."""

import argparse
from contextlib import ExitStack
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from elf_split_fixture import link_split_fixture
from repl_browser_integration import Transcript, stop

FIXTURE = Path(__file__).resolve().parent / "fixtures" / "repl_aliases.cpp"
INCOMPATIBLE_FIXTURE = FIXTURE.with_name("repl_aliases_incompatible.cpp")
TWIN = "alias_demo::Twin"
FOLDED = "alias_demo::Folded"
ALIASED_SYMBOL = "_ZNK10alias_demo4Twin7aliasedEi"
FOLD_A_SYMBOL = "_ZNK10alias_demo4Twin6fold_aEi"
STATUSES = ("DWARF_SUPPORTED ", "PREPARED_REQUIRED ", "UNSUPPORTED ", "AMBIGUOUS ",
            "ALIAS ", "UNMATCHED ")


class Live:
    """A running fixture with an attached REPL console."""

    def __init__(self, stack, args, target):
        self.child = subprocess.Popen([target], stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, text=True)
        stack.callback(stop, self.child)
        self.target_output = Transcript(self.child.stdout)
        self.target_output.expect(r"^ALIAS_FIXTURE pid=")
        self.console = subprocess.Popen(
            [args.repl, "--server", args.server, "--attach", str(self.child.pid)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        stack.callback(stop, self.console)
        self.output = Transcript(self.console.stdout)
        self.output.expect(rf"^attached session=\S+ pid={self.child.pid}$")
        self.memory = stack.enter_context(open(f"/proc/{self.child.pid}/mem", "rb", buffering=0))

    def send(self, line):
        self.console.stdin.write(line + "\n")
        self.console.stdin.flush()

    def code(self, address):
        data = os.pread(self.memory.fileno(), 16, address)
        assert len(data) == 16
        return data

    def preview(self, command, summary):
        first = len(self.output.seen)
        self.send(command)
        self.output.expect(r"^dry-run " + summary)
        return [line.rstrip("\n") for line in self.output.seen[first:] if line.startswith(STATUSES)]

    def report(self, command, operation):
        """Run a bulk command and return its rows, the batch id and the completeness flag."""
        first = len(self.output.seen)
        self.send(command)
        done = self.output.expect(rf"^bulk {operation} batch=([0-9a-f]{{32}}) complete=(true|false)")
        return self.output.seen[first:-1], done[1], done[2] == "true"

    def events(self, probe, ranges):
        """Collect PRE events of one probe and require an argument in every range."""
        self.send(f"%collect --probe {probe} --phase PRE --count 90 --timeout 20 --as alias_events")
        self.output.expect(r"received=\d+")
        flags = ", ".join(f"seen{i} = false" for i in range(len(ranges)))
        tests = " ".join(f"if (a >= {low} && a < {high}) seen{i} = true;"
                         for i, (low, high) in enumerate(ranges))
        every = " && ".join(f"seen{i}" for i in range(len(ranges)))
        self.send('{ auto batch = tracer.batch("alias_events"); bool ' + flags + "; "
                  "for (const auto& e : batch) { int a = e.argument_as<int>(0); " + tests + " } "
                  f'std::cout << ({every} ? "alias-events-ok" : "alias-events-missing") << std::endl; }}')
        self.output.expect(r"^alias-events-(ok|missing)$")
        assert self.output.seen[-1].strip() == "alias-events-ok", "".join(self.output.seen[-40:])


def row_for(rows, status, selector):
    # GCC DWARF spells this builtin "long int" where Clang uses "long".
    expected = re.sub(r"\blong int\b", "long", selector)
    found = [row for row in rows if row.startswith(status + " ") and
             f'"{expected}"' in re.sub(r"\blong int\b", "long", row)]
    assert len(found) == 1, (status, selector, rows)
    return found[0]


def address_of(row):
    return int(re.search(r"address=(0x[0-9a-f]+)", row)[1], 16)


def probe_of(row):
    return re.search(r"probe=(\d+)", row)[1]


def explicit_alias(stack, root, args, target):
    live = Live(stack, args, target)
    fold_a, fold_b = f"{TWIN}::fold_a(int) const", f"{TWIN}::fold_b(int) const"
    aliased, wide = f"{TWIN}::aliased(int) const", f"{TWIN}::wide(long) const"

    # Different functions are not aliases of each other.
    rows = live.preview(f'%trace preview --exact "{fold_a}" --exact "{fold_b}"',
                        r"matched=2 dwarf_supported=2 .*available=32 fits=yes$")
    assert len(rows) == 2 and all(row.startswith("DWARF_SUPPORTED ") for row in rows), rows
    assert address_of(rows[0]) != address_of(rows[1]), rows

    # Two names at one entry: one supported primary and an ALIAS row for the other name.
    manifest = root / "alias.textproto"
    rows = live.preview(f'%trace preview --exact "{fold_a}" --exact "{aliased}" --save "{manifest}"',
                        r"matched=2 dwarf_supported=1 .*available=32 fits=yes$")
    live.output.expect(r"^manifest=")
    alias = row_for(rows, "ALIAS", aliased)
    primary = row_for(rows, "DWARF_SUPPORTED", fold_a)
    assert f"symbol={ALIASED_SYMBOL} " in alias, alias
    assert f'reason="alias of {FOLD_A_SYMBOL}"' in alias, alias
    assert f"symbol={FOLD_A_SYMBOL} " in primary, primary
    address = address_of(primary)
    assert address_of(alias) == address
    original = live.code(address)

    # One probe covers both names; events from calls through either name arrive.
    rows, batch, complete = live.report(f'%trace apply --manifest "{manifest}"', "apply")
    assert complete and "available=32" in live.output.seen[-1], live.output.seen[-10:]
    installed = row_for(rows, "INSTALLED", fold_a)
    covered = row_for(rows, "ALIASED", aliased)
    probe = probe_of(installed)
    assert probe_of(covered) == probe, rows
    assert f"symbol={ALIASED_SYMBOL} " in covered, covered
    assert live.code(address) != original
    live.events(probe, [(0, 1000000), (1000000, 2000000)])

    # Removal reports the one probe and restores the entry bytes.
    rows, _, complete = live.report(f"%trace remove {batch}", "remove")
    removed = [row for row in rows if row.startswith("REMOVED ")]
    assert complete and len(removed) == 1 and f"probe={probe} restored=true" in removed[0], rows
    assert live.code(address) == original

    # A failure after the first install rolls the batch back; the alias row stays without a probe.
    rollback = root / "rollback.textproto"
    live.preview(f'%trace preview --exact "{fold_a}" --exact "{aliased}" --exact "{fold_b}" '
                 f'--exact "{wide}" --save "{rollback}"',
                 r"matched=4 dwarf_supported=3 .*fits=yes$")
    live.output.expect(r"^manifest=")
    fold_b_address = address_of(row_for(
        live.preview(f'%trace preview --exact "{fold_b}"', r"matched=1 dwarf_supported=1 "),
        "DWARF_SUPPORTED", fold_b))
    fold_b_original = live.code(fold_b_address)
    mismatched = root / "int-only.cpp"
    mismatched.write_text("template<> class Interceptor<int(int)> {\n"
                          "public: void pre(const Call&) {}\n};\n")
    rows, _, complete = live.report(
        f'%trace apply --manifest "{rollback}" --callback "{mismatched}"', "apply")
    assert not complete, rows
    row_for(rows, "ROLLED_BACK", fold_a)
    row_for(rows, "ROLLED_BACK", fold_b)
    row_for(rows, "INSTALL_FAILED", wide)
    skipped = row_for(rows, "ALIASED", aliased)
    assert "probe=" not in skipped, skipped
    assert live.code(address) == original and live.code(fold_b_address) == fold_b_original

    # A single probe by either spelling of the alias reaches the shared entry.
    for selector in (aliased, ALIASED_SYMBOL):
        live.send(f'%probe inject "{selector}"')
        probe = live.output.expect(r"^installed probe=(\d+)")[1]
        assert live.code(address) != original
        live.events(probe, [(0, 1000000), (1000000, 2000000)])
        live.send(f"%probe remove {probe}")
        live.output.expect(rf"^removed probe={probe} restored=true$")
        assert live.code(address) == original
    live.send("%quit")
    live.console.wait(timeout=15)


def folded_functions(stack, root, args, compiler):
    flags = ["-std=c++17", "-g", "-O0", "-ffunction-sections", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-fPIE"]
    target = root / "icf-target"
    try:
        subprocess.run([compiler, *flags, "-pie", "-fuse-ld=gold", "-Wl,--icf=all",
                        str(FIXTURE), "-o", str(target)], check=True, timeout=60,
                       capture_output=True)
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        print("SKIP folded functions: the compiler cannot link with gold --icf=all:",
              getattr(error, "stderr", b"")[-200:])
        return
    live = Live(stack, args, target)
    names = [f"{FOLDED}::{name}(int) const" for name in ("one", "two", "three")]
    selection = " ".join(f'--exact "{name}"' for name in names)
    manifest = root / "folded.textproto"
    first = len(live.output.seen)
    live.send(f'%trace preview {selection} --save "{manifest}"')
    live.output.expect(r"^dry-run matched=3 ")
    live.output.expect(r"^manifest=")
    rows = [line for line in live.output.seen[first:] if line.startswith(STATUSES)]
    if not any(row.startswith("ALIAS ") for row in rows):
        print("SKIP folded functions: the linker did not fold the identical bodies")
        return
    assert sorted(row.split()[0] for row in rows) == ["ALIAS", "ALIAS", "DWARF_SUPPORTED"], rows
    assert len({address_of(row) for row in rows}) == 1, rows
    address = address_of(rows[0])
    original = live.code(address)
    rows, batch, complete = live.report(f'%trace apply --manifest "{manifest}"', "apply")
    assert complete and "available=32" in live.output.seen[-1], rows
    rows = [row for row in rows if row.startswith(("INSTALLED ", "ALIASED "))]
    probes = {probe_of(row) for row in rows}
    assert len(probes) == 1 and sorted(row.split()[0] for row in rows) == [
        "ALIASED", "ALIASED", "INSTALLED"], rows
    live.events(probes.pop(), [(0, 1000000), (2000000, 3000000), (3000000, 4000000)])
    rows, _, complete = live.report(f"%trace remove {batch}", "remove")
    removed = [row for row in rows if row.startswith("REMOVED ")]
    assert complete and len(removed) == 1 and "restored=true" in removed[0], rows
    assert live.code(address) == original
    live.send("%quit")
    live.console.wait(timeout=15)


def constructor_names(root, args):
    """C1/C2 constructors are two names at one address; the index resolves them as one callable."""
    source = root / "counter.cpp"
    source.write_text("namespace demo {\nstruct Counter { int v; explicit Counter(int x); };\n"
                      "Counter::Counter(int x) : v(x) {}\n}\n")
    flags = ["-std=c++17", "-g", "-O0", "-fPIC", "-fno-omit-frame-pointer", "-fcf-protection=none"]
    link_split_fixture(args.compiler, "counter.cpp", root, "libcounter.so", "libcounter.debug",
                       flags, ["-shared"])
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_MODULE_EXECUTABLE"] = args.module_tool
    result = subprocess.run(
        [args.repl, "--fail-fast"], cwd=root, env=env, capture_output=True, text=True, timeout=120,
        input="""%module index --file ./libcounter.so
%module symbols --filter 'demo::Counter::Counter*' --format json
%module show --symbol "demo::Counter::Counter(int)"
%quit
""")
    assert result.returncode == 0, result.stdout + result.stderr
    rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    listing, shown = rows
    entries = listing["entries"]
    assert len(entries) == 2 and len({entry["address"] for entry in entries}) == 1, entries
    assert len({entry["linkage"] for entry in entries}) == 2, entries
    assert shown["address"] == entries[0]["address"] and shown["has_debug"], shown
    assert shown["id"] in {entry["id"] for entry in entries}, shown


def incompatible_folded_functions(stack, root, args):
    """Reject an entry whose other, possibly unselected name has a different ABI."""
    target = root / "incompatible-icf-target"
    try:
        subprocess.run(
            [args.compiler, "-std=c++17", "-g", "-O2", "-ffunction-sections", "-fno-inline",
             "-fno-omit-frame-pointer", "-fcf-protection=none", "-fPIE", "-pie",
             "-fuse-ld=gold", "-Wl,--icf=all", str(INCOMPATIBLE_FIXTURE), "-o", str(target)],
            check=True, timeout=60, capture_output=True)
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        print("SKIP incompatible folded functions: cannot link with gold --icf=all:",
              getattr(error, "stderr", b"")[-200:])
        return

    live = Live(stack, args, target)
    addresses = live.target_output.expect(
        r"^ICF_ENTRIES identity_float=(0x[0-9a-f]+) identity_double=(0x[0-9a-f]+) "
        r"return_float=(0x[0-9a-f]+) return_double=(0x[0-9a-f]+)$")
    identity_float, identity_double, return_float, return_double = (
        int(address, 16) for address in addresses.groups())
    # Determine whether folding happened from the target itself, never from the
    # ALIAS status being tested: a broken guard must fail rather than skip.
    assert identity_float == identity_double, "gold did not fold the identity pair"
    assert return_float == return_double, "gold did not fold the return-only pair"
    assert identity_float != return_float, "the two different bodies unexpectedly share an entry"
    originals = {address: live.code(address) for address in (identity_float, return_float)}
    checks = 0

    def unchanged_and_healthy():
        nonlocal checks
        assert all(live.code(address) == original for address, original in originals.items())
        assert live.child.poll() is None, "incompatible ICF target exited"
        live.child.stdin.write("\n")
        live.child.stdin.flush()
        checks += 1
        health = live.target_output.expect(r"^ICF_(?:OK count=(\d+)|CORRUPT .*)$")
        assert health[1] is not None and int(health[1]) == checks, "".join(live.target_output.seen)
        assert live.child.poll() is None

    def incompatible_diagnostic(text):
        lower = text.lower()
        assert "abi" in lower and ("incompatible" in lower or "conflict" in lower), text

    unchanged_and_healthy()
    pairs = [
        (("a_float(float)", "_Z7a_floatf"), ("z_double(double)", "_Z8z_doubled")),
        (("a_return_float()", "_Z14a_return_floatv"),
         ("z_return_double()", "_Z15z_return_doublev")),
    ]
    header = root / "incompatible.hpp"
    header.write_text("float a_float(float);\ndouble z_double(double);\n"
                      "float a_return_float();\ndouble z_return_double();\n")
    donor = root / "incompatible.cpp"
    donor.write_text('#include "incompatible.hpp"\n')
    database = root / "incompatible-compile_commands.json"
    database.write_text(json.dumps([{
        "directory": str(root), "file": str(donor),
        "arguments": [args.compiler, "-std=c++17", "-g", "-O2", "-fPIE",
                      "-c", str(donor), "-o", str(root / "incompatible.o")],
    }]))
    for pair in pairs:
        # The check must examine every DWARF description at an entry, not merely
        # the selected rows: either name alone still detours calls through both.
        for selected in (pair, pair[:1], pair[1:]):
            selection = " ".join(f'--exact "{name}"' for name, _ in selected)
            manifest = root / f"incompatible-{checks}.textproto"
            rows = live.preview(
                f'%trace preview {selection} --save "{manifest}"',
                rf"matched={len(selected)} dwarf_supported=0 .*"
                r"session_retained=0 agent_retained=0 available=32 fits=yes$")
            live.output.expect(r"^manifest=")
            assert len(rows) == len(selected), rows
            assert all(row.startswith(("UNSUPPORTED ", "AMBIGUOUS ")) for row in rows), rows
            for row in rows:
                incompatible_diagnostic(row)
            unchanged_and_healthy()

            rows, _, complete = live.report(f'%trace apply --manifest "{manifest}"', "apply")
            assert not complete, rows
            assert not any(row.startswith(("INSTALLED ", "ALIASED ")) for row in rows), rows
            blocked = [row for row in rows if row.startswith("BLOCKED ")]
            assert len(blocked) == len(selected) and all("probe=" not in row for row in blocked), rows
            for row in blocked:
                incompatible_diagnostic(row)
            # The CLI reports its command error after printing the bulk rows.
            live.output.expect(r"^error: selection contains blocked or unmatched entries$")
            unchanged_and_healthy()

        for name, linkage in pair:
            for selector in (name, linkage):
                live.send(f'%probe inject "{selector}"')
                result = live.output.expect(r"^error: .*|^installed probe=.*")
                assert result[0].startswith("error:"), result[0]
                incompatible_diagnostic(result[0])
                unchanged_and_healthy()

        # A source-prepared package carries a trustworthy signature for its own
        # name, but must not bypass conflicting DWARF at the shared live entry.
        # Test both argument/result widths and the return-only conflict.
        prepared_selector = pair[0][0]
        package = root / ("prepared-" + prepared_selector.split("(")[0])
        live.send(f'%probe prepare "{prepared_selector}" --include "{header}" '
                  f'--compdb "{database}" --translation-unit "{donor}" --output "{package}"')
        prepared = live.output.expect(r"^prepared directory=.*|^error: .*")
        assert prepared[0].startswith("prepared directory="), prepared[0]
        assert (package / "prepared.pb").is_file()
        unchanged_and_healthy()
        live.send(f'%probe inject "{prepared_selector}" --prepared "{package}" '
                  '--prepared-mode require')
        result = live.output.expect(r"^error: .*|^installed probe=.*")
        assert result[0].startswith("error:"), result[0]
        incompatible_diagnostic(result[0])
        unchanged_and_healthy()

        # Confirm no failed operation retained a probe or consumed a slot.
        live.preview(f'%trace preview --exact "{pair[0][0]}"',
                     r"matched=1 dwarf_supported=0 .*session_retained=0 "
                     r"agent_retained=0 available=32 fits=yes$")
    live.send("%quit")
    # Expected injection errors mark the piped session unsuccessful without
    # terminating it before the subsequent safety checks.
    assert live.console.wait(timeout=15) == 1, "".join(live.output.seen)
    unchanged_and_healthy()
    print(f"Incompatible ICF entries rejected in bulk, direct and prepared injection; health checks={checks}.")


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "module-tool", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ct-alias-") as directory, ExitStack() as stack:
        root = Path(directory)
        target = root / "alias-target"
        subprocess.run([args.compiler, "-std=c++17", "-g", "-O0", "-fno-inline",
                        "-fno-omit-frame-pointer", "-fcf-protection=none", "-fPIE", "-pie",
                        str(FIXTURE), "-o", str(target)], check=True, timeout=60)
        explicit_alias(stack, root, args, target)
        folded_functions(stack, root, args, args.compiler)
        constructor_names(root, args)
        incompatible_folded_functions(stack, root, args)
    print("Aliased names were previewed as ALIAS, observed through one probe and restored.")


if __name__ == "__main__":
    main()
