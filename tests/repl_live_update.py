"""Build-bound same-process live patch and rollback through the native REPL."""

import argparse
import hashlib
import json
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
from contextlib import ExitStack

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks" / "object-write"))
from run import Output, send, stop


HEADER = """#pragma once
struct Base { virtual int quote(int) const noexcept = 0; };
struct Pricer : Base {
  int markup = 2;
  int quote(int cents) const noexcept override;
};
int checkout(int);
"""
MAIN = """#include "price.hpp"
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

__attribute__((noinline)) int direct(const Pricer &p) { return p.Pricer::quote(10); }
__attribute__((noinline)) int indirect(const Base &p) { return p.quote(10); }
int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  Pricer p;
  int (*volatile saved)(int) = &checkout;
  std::puts("READY");
  std::fflush(stdout);
  while (true) {
    usleep(3000);
    if (access("call", F_OK)) continue;
    unlink("call");
    std::printf("%d %d %d %d\\n", getpid(), saved(6000), direct(p), indirect(p));
    std::fflush(stdout);
  }
}
"""
ORIGINAL = """#include "price.hpp"
int checkout(int cents) { return cents; }
int Pricer::quote(int cents) const noexcept { return cents + markup; }
"""
UPDATED = """#include "price.hpp"
int discount(int cents);
int checkout(int cents) { return cents - discount(cents); }
int Pricer::quote(int cents) const noexcept { return cents + 2 * markup; }
"""


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(args, root, cleanup):
    source = root / "source"
    baseline = root / "baseline"
    source.mkdir()
    baseline.mkdir()
    for name, contents in {"main.cpp": MAIN, "price.hpp": HEADER, "price.cpp": ORIGINAL}.items():
        (source / name).write_text(contents)
        (baseline / name).write_text(contents)
    executable = root / "target"
    flags = ["-std=c++17", "-g", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-fpatchable-function-entry=16,0"]
    compiler = str(Path(args.compiler).absolute())
    subprocess.run([compiler, *flags, "-Wl,--build-id=sha1", source / "main.cpp",
                    source / "price.cpp", "-o", executable], check=True)
    # The verified profile also records how a future TU should be compiled.
    commands = [{"directory": str(source), "file": str(source / name),
                 "arguments": [compiler, *flags, "-c", str(source / name), "-o", name + ".o"]}
                for name in ("main.cpp", "price.cpp", "discount.cpp")]
    database = root / "compile_commands.json"
    database.write_text(json.dumps(commands))
    manifest = root / "clang-tracer-live.textproto"
    manifest.write_text(
        f'source_root: {json.dumps(str(source))}\n'
        f'snapshot_root: {json.dumps(str(baseline))}\n'
        f'module: {json.dumps(str(executable))}\n'
        f'module_sha256: "{sha(executable)}"\n'
        f'compilation_database_sha256: "{sha(database)}"\n' +
        "".join(f'inputs {{ path: "{path.name}" sha256: "{sha(path)}" }}\n'
                for path in sorted(baseline.iterdir()))
    )

    def launch(*argv):
        child = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, cwd=root)
        cleanup.callback(stop, child)
        return child, Output(child.stdout, 60)

    target, app = launch(executable)
    app.expect(r"^READY$")
    repl, console = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(target.pid), "--fail-fast", "--timeout", "60")
    console.expect(rf"^attached session=\S+ pid={target.pid}$")

    def call(cents, quote):
        (root / "call").touch()
        app.expect(rf"^{target.pid} {cents} {quote} {quote}$")

    call(6000, 12)
    (source / "price.cpp").write_text("// comment only\n" + ORIGINAL)
    send(repl, "%live preview source/price.cpp")
    console.expect(r"^live UNCHANGED revision=0")
    call(6000, 12)

    (source / "price.cpp").write_text(UPDATED)
    (source / "discount.cpp").write_text("int discount(int cents) { return cents >= 5000 ? 500 : 0; }\n")
    send(repl, "%live preview source/price.cpp source/discount.cpp")
    console.expect(r"^live PLANNED revision=0")
    console.expect(r"^  PLANNED .*checkout")
    console.expect(r"^  PLANNED .*Pricer::quote")
    call(6000, 12)
    before = sha(executable)
    send(repl, "%live update source/price.cpp source/discount.cpp --retention-limit 1")
    console.expect(r"^live APPLIED revision=1 retained=1/1")
    call(5500, 14)

    send(repl, "%live status")
    console.expect(r"^live CURRENT revision=1 retained=1/1")
    (source / "price.cpp").write_text(UPDATED.replace("2 * markup", "3 * markup"))
    send(repl, "%live update source/price.cpp")
    console.expect(r"^live BLOCKED revision=1 .*generation limit")
    call(5500, 14)

    (source / "price.cpp").write_text(UPDATED.replace("discount(cents)", "missing(cents)"))
    send(repl, "%live update source/price.cpp")
    console.expect(r"^live BLOCKED revision=1 .*undeclared identifier")
    call(5500, 14)

    send(repl, "%live revert")
    console.expect(r"^live REVERTED revision=0 retained=1/1")
    call(6000, 12)
    assert sha(executable) == before
    (source / "price.cpp").write_text(ORIGINAL)
    send(repl, "%live watch source/price.cpp --retention-limit 2")
    console.expect(r"^Watching 1 live source files")
    (source / "price.cpp").write_text(ORIGINAL.replace("return cents;", "return cents - 40;"))
    console.expect(r"^live APPLIED revision=2 retained=2/2")
    call(5960, 12)
    repl.send_signal(signal.SIGINT)
    console.expect(r"^Live watch stopped")
    send(repl, "%detach")
    console.expect(r"^detached session=")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for option in ("compiler", "repl", "server", "agent"):
        parser.add_argument("--" + option, required=True)
    with tempfile.TemporaryDirectory(prefix="ct-live-update-") as directory, ExitStack() as cleanup:
        verify(parser.parse_args(), Path(directory), cleanup)
