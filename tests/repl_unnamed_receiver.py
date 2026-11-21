"""A receiver without DW_AT_name is still the receiver (issue #151).

The fixture is built twice with GCC, once as emitted and once with the name attribute of every
artificial `this` removed from the assembly. DW_AT_object_pointer still identifies the receiver, so
both builds must install the same probes, capture the same arguments, and keep the hidden VTT
argument of a base-object constructor. Exits 77 (skipped) when the compiler is not GCC.
"""

from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from repl_cdtor import PASSES, Session, Target, argument, argument_names, send

SKIP = 77
FLAGS = ["-std=gnu++17", "-g", "-gdwarf-4", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
         "-fcf-protection=none", "-fno-pie"]
LINK = ["-no-pie", "-Wl,--build-id=sha1"]


def build(compiler, fixtures, root):
    """Return {name: executable} for the named and the unnamed receiver variant."""
    sources = [fixtures / "receiver.cpp", fixtures / "main.cpp"]
    result = {}
    for variant in ("named", "unnamed"):
        objects = []
        for source in sources:
            assembly = root / f"{variant}-{source.stem}.s"
            subprocess.run([compiler, *FLAGS, "-I", str(fixtures), "-S", "-dA", str(source),
                            "-o", str(assembly)], check=True)
            if variant == "unnamed" and source.name == "receiver.cpp":
                stripped = root / f"{variant}-{source.stem}-stripped.s"
                subprocess.run([sys.executable, str(fixtures / "strip_receiver_name.py"),
                                str(assembly), str(stripped)], check=True)
                assembly = stripped
            obj = root / f"{variant}-{source.stem}.o"
            subprocess.run([compiler, *FLAGS, "-c", str(assembly), "-o", str(obj)], check=True)
            objects.append(str(obj))
        executable = root / f"target-{variant}"
        subprocess.run([compiler, *objects, *LINK, "-o", str(executable)], check=True)
        result[variant] = executable
    return result


def observe(repl, server, agent, executable, root):
    n = PASSES
    with ExitStack() as stack:
        target = Target(str(executable), stack)
        session = Session(repl, server, agent, target, stack, root)
        ordinary, _ = session.inject('"unnamed::Plain::ordinary(int)"')
        constant, _ = session.inject('"unnamed::Plain::constant(int) const"')
        plain_ctor, _ = session.inject('"unnamed::Plain::Plain(int)"')
        plain_dtor, _ = session.inject('"unnamed::Plain::~Plain()"')
        static, _ = session.inject('"unnamed::Plain::statik(int)"')
        mid_c2, _ = session.inject("_ZN7unnamed3MidC2Ei")
        mid_d2, _ = session.inject("_ZN7unnamed3MidD2Ev")
        session.run_passes()

        for probe, name in ((ordinary, "o"), (constant, "c"), (static, "s")):
            events = session.events(probe, "POST", n, name)
            assert all(argument(e, "x") == 7 for e in events), events
        events = session.events(ordinary, "POST", n, "ordinary-result")
        assert all(argument(e, "x") == 7 for e in events), events
        assert sorted(argument(e, "x") for e in session.events(plain_ctor, "POST", n, "pc")) == \
            list(range(n))
        assert len(session.events(plain_dtor, "POST", n, "pd")) == n

        # The hidden VTT pointer follows the receiver, precedes x and is a real argument.
        mid = session.events(mid_c2, "POST", n, "mid")
        assert all(argument_names(e) == ["__vtt_parm", "x"] for e in mid), mid
        assert all(argument(e, "__vtt_parm") != 0 for e in mid), mid
        assert sorted(argument(e, "x") for e in mid) == [i + 1 for i in range(n)], mid
        dtor = session.events(mid_d2, "POST", n, "mid-d2")
        assert all(argument_names(e) == ["__vtt_parm"] for e in dtor), dtor
        assert all(argument(e, "__vtt_parm") != 0 for e in dtor), dtor

        session.run_passes(1)
        assert target.process.poll() is None
        session.detach()
        send(target.process, "q")
        target.process.wait(timeout=10)


def main(repl, server, agent, compiler):
    version = subprocess.run([compiler, "--version"], capture_output=True, text=True).stdout
    if "clang" in version.lower() or "gcc" not in version.lower():
        print("skipped: a GCC compiler is required for the assembly transformation")
        sys.exit(SKIP)
    os.environ.pop("CLANG_TRACER_ENDPOINT", None)
    fixtures = Path(__file__).resolve().parent / "fixtures" / "unnamed_receiver"
    with tempfile.TemporaryDirectory(prefix="ct-unnamed-") as directory:
        root = Path(directory)
        executables = build(compiler, fixtures, root)
        for variant, executable in executables.items():
            work = root / f"run-{variant}"
            work.mkdir()
            observe(repl, server, agent, executable, work)
    print("named and unnamed receivers observed identically")


if __name__ == "__main__":
    main(*sys.argv[1:])
