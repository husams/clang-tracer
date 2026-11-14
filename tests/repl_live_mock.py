"""Exercise typed live mocks from the native REPL in three independent targets."""

from contextlib import contextmanager
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from repl_browser_integration import Transcript, stop


FILES = Path(__file__).parent / "fixtures" / "live_mock"


def stage(root, compiler):
    source = root / "application.cpp"
    header = root / "application.hpp"
    baseline = root / "baseline"
    baseline.mkdir()
    for path in (source, header):
        shutil.copyfile(FILES / path.name, path)
        shutil.copyfile(path, baseline / path.name)
    target = root / "test-target"
    flags = ["-std=c++17", "-O0", "-g", "-fno-inline", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-fpatchable-function-entry=16,0"]
    subprocess.run([compiler, *flags, "-Wl,--build-id=sha1", str(source),
                    "-o", str(target)], check=True)
    argv = [compiler, *flags, "-c", str(source), "-o", "application.o"]
    database = root / "compile_commands.json"
    database.write_text(json.dumps([{"directory": str(root), "file": str(source),
                                     "arguments": argv}]))
    digest = lambda path: sha256(path.read_bytes()).hexdigest()
    (root / "clang-tracer-live.textproto").write_text(
        f'source_root: {json.dumps(str(root))}\n'
        f'snapshot_root: {json.dumps(str(baseline))}\n'
        f'module: {json.dumps(str(target))}\n'
        f'module_sha256: "{digest(target)}"\n'
        f'compilation_database_sha256: "{digest(database)}"\n'
        + "".join(f'inputs {{ path: "{path.name}" sha256: "{digest(path)}" }}\n'
                  for path in (source, header)))
    for path in FILES.iterdir():
        if path.name not in (source.name, header.name):
            shutil.copyfile(path, root / path.name)
    return target


@contextmanager
def attached(repl, server, agent, target):
    child = subprocess.Popen([target], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    console = None
    try:
        app = Transcript(child.stdout)
        app.expect(r"^ready$")
        console = subprocess.Popen([repl, "--server", server, "--agent", agent,
                                    "--attach", str(child.pid), "--timeout", "60"],
                                   cwd=target.parent, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        trace = Transcript(console.stdout)
        trace.expect(rf"^attached session=\S+ pid={child.pid}$")

        def execute(line, *expected):
            child.stdin.write(line + "\n")
            child.stdin.flush()
            for pattern in expected:
                app.expect("^" + pattern + "$")

        def command(line, pattern):
            console.stdin.write(line + "\n")
            console.stdin.flush()
            return trace.expect(pattern)

        yield execute, command
        child.stdin.close()
        assert child.wait(timeout=10) == 0, "".join(app.seen)
    finally:
        if console is not None:
            stop(console)
        stop(child)


def mock(command, signature, file, function, options="", state="APPLIED"):
    line = f'%live mock "{signature}" --file {file} --function {function} {options}'.strip()
    return command(line, rf"^live {state} revision=(\d+)")


def products(repl, server, agent, target):
    signature = "ProductStore::load(int)"
    with attached(repl, server, agent, target) as (call, command):
        good = mock(command, signature, "product_fixture.cpp", "load_fixture",
                    "--fixture products.txt")[1]
        # Removing the input proves the calling thread never opens the fixture.
        (target.parent / "products.txt").unlink()
        call("l 42", "load=42 price=1999 name=Desk lamp queries=0 stored=1999")
        call("l 70", "load=70 price=-1 name=none queries=0 stored=1999")
        command(f'%live mock "{signature}" --file product_fixture.cpp '
                '--function load_fixture --fixture products.txt', r"cannot read mock fixture")
        assert mock(command, signature, "invalid_source.cpp", "invalid", state="BLOCKED")[1] == good
        assert mock(command, signature, "invalid_abi.cpp", "invalid_load", state="BLOCKED")[1] == good
        call("l 42", "load=42 price=1999 name=Desk lamp queries=0 stored=1999")
        command("%live revert", r"^live REVERTED revision=0")
        call("l 42", "load=42 price=500 name=Database queries=1 stored=1999")

    with attached(repl, server, agent, target) as (call, command):
        mock(command, signature, "product_cached.cpp", "load_cached_discount",
             '--bind "find_cached=ProductStore::find_cached(int)"')
        call("l 42", "load=42 price=999 name=Desk lamp queries=0 stored=1999")
        call("l 70", "load=70 price=-1 name=none queries=0 stored=1999")
        command("%live revert", r"^live REVERTED revision=0")
        call("l 42", "load=42 price=500 name=Database queries=1 stored=1999")


def ownership(repl, server, agent, target):
    with attached(repl, server, agent, target) as (call, command):
        quote_revision = mock(command, "Pricer::make_quote(int)", "ownership.cpp", "make_test_quote",
                              '--bind "is_test_product=Pricer::is_test_product(int) const" '
                              '--bind "quote_create=quote_create(int, int)"')[1]
        call("q 77", "quote=77 price=200 calls=1")
        call("d", "destroying=77 price=200", "destroyed=1")
        call("q 42", "quote=42 price=125 calls=1")
        mock(command, "status_message()", "status.cpp", "mock_status",
             '--bind "target_allocate=target_allocate(unsigned long)"')
        call("x", "fail=1")
        call("s", "message=null buffers=0")
        call("x", "fail=0")
        call("s", "message=mock ready buffers=1")
        command(f"%live revert {quote_revision}", rf"^live REVERTED revision={quote_revision}")
        call("f", "releasing=mock ready", "buffers=0")
        call("s", "message=original buffers=1")
        call("f", "releasing=original", "buffers=0")
        command("%live revert 0", r"^live REVERTED revision=0")
        call("d", "destroying=42 price=125", "destroyed=2")
        call("q 42", "quote=42 price=200 calls=2")
        call("d", "destroying=42 price=200", "destroyed=3")

        dispatch = mock(command, "nesting(int)", "nesting.cpp", "mock_nesting")[1]
        call("n 2", "nesting=33 originals=3")
        mock(command, "nesting(int)", "nesting.cpp", "mock_nesting", "--recursion original")
        call("n -1", "caught=mock failure originals=0")
        call("n 2", "nesting=13 originals=3")
        command(f"%live revert {dispatch}", rf"^live REVERTED revision={dispatch}")
        call("n 2", "nesting=33 originals=3")
        command("%live revert 0", r"^live REVERTED revision=0")
        call("n 2", "nesting=3 originals=3")


def main(repl, server, agent, compiler=os.environ.get("CXX", "clang++")):
    with tempfile.TemporaryDirectory(prefix="ct-live-mock-") as directory:
        target = stage(Path(directory).resolve(), compiler)
        binaries = [str(Path(path).resolve()) for path in (repl, server, agent)]
        products(*binaries, target)
        ownership(*binaries, target)
    print("Native fixture, cache, private and hidden bindings, original, recursion and ownership passed")


if __name__ == "__main__":
    main(*sys.argv[1:])
