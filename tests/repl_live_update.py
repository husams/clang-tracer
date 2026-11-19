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
TEMPLATES = """#pragma once
template <typename T>
struct Adjust {
  static T apply(T value) { return value + 1; }
};

template <>
struct Adjust<double> {
  static double apply(double value) { return value; }
};

template <typename T>
struct CachePolicy {
  static int limit(T) { return 64; }
};

template <typename T>
struct CachePolicy<T *> {
  static int limit(T *value) { return value ? 256 : 0; }
};

template <typename T>
[[gnu::always_inline]] inline int folded(T) { return 5; }
"""

ABI_HEADER = """#pragma once
#include <optional>

struct Lifecycle {
  explicit Lifecycle(int);
  virtual ~Lifecycle();
  int value;
};

struct Left { virtual ~Left(); };
struct Right { virtual Right* self(); };
struct Joined : Left, Right {
  ~Joined() override;
  Joined* self() override;
};
struct Product {
  int cents;
  ~Product() noexcept;
};
std::optional<Product> product(int);
extern int destroyed;
extern int product_destructions;
extern "C" __attribute__((ms_abi)) int foreign_abi(int, int);
extern "C" int vararg_total(int, ...);
"""
ABI_SOURCE = """#include "abi.hpp"
#include <cstdarg>

int destroyed = 0;
int product_destructions = 0;
Lifecycle::Lifecycle(int initial) : value(initial + 1) {
  if (initial < 0) throw initial;
}
Lifecycle::~Lifecycle() { destroyed += value; }
Left::~Left() = default;
Right* Right::self() { return this; }
Joined::~Joined() = default;
Joined* Joined::self() { return this; }
Product::~Product() noexcept { ++product_destructions; }
std::optional<Product> product(int id) {
  if (id < 0) return {};
  return Product{id + 5};
}
extern "C" __attribute__((ms_abi)) int foreign_abi(int a, int b) { return a + b; }
extern "C" int vararg_total(int count, ...) {
  va_list args;
  va_start(args, count);
  int total = 0;
  for (int i = 0; i < count; ++i) total += va_arg(args, int);
  va_end(args);
  return total;
}
"""

MAIN = """#include "price.hpp"
#include "templates.hpp"
#include "abi.hpp"
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

__attribute__((noinline)) int direct(const Pricer &p) { return p.Pricer::quote(10); }
__attribute__((noinline)) int indirect(const Base &p) { return p.quote(10); }
__attribute__((noinline)) int inline_caller() { return folded(1); }
__attribute__((noinline)) Right* through_secondary(Right& value) { return value.self(); }
__attribute__((noinline)) void release(Lifecycle* value) { delete value; }

void exercise_abi() {
  int caught = 0;
  try {
    release(new Lifecycle(-7));
  } catch (int value) {
    caught = value;
  }
  auto* object = new Lifecycle(10);
  const int initialized = object->value;
  release(object);
  Joined joined;
  Right& secondary = joined;
  const bool returns_secondary = through_secondary(secondary) == &secondary;
  auto present = product(11);
  auto empty = product(-1);
  std::printf("abi %d %d %d %d %d %d %d %d\\n", initialized, caught, destroyed,
      returns_secondary, present->cents, !empty, foreign_abi(7, 4), vararg_total(2, 7, 5));
}


int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  Pricer p;
  int (*volatile saved)(int) = &checkout;
  std::puts("READY");
  std::fflush(stdout);
  while (true) {
    usleep(3000);
    if (access("abi", F_OK) == 0) {
      unlink("abi");
      exercise_abi();
      std::fflush(stdout);
    }
    if (access("templates", F_OK) == 0) {
      unlink("templates");
      int i = 1;
      double d = 1;
      std::printf("templates %d %.0f %d %d %d %d %d\\n", Adjust<int>::apply(5),
          Adjust<double>::apply(150), CachePolicy<int>::limit(5),
          CachePolicy<int*>::limit(&i), CachePolicy<double*>::limit(&d),
          CachePolicy<int*>::limit(nullptr), inline_caller());
      std::fflush(stdout);
    }
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
    for name, contents in {"main.cpp": MAIN, "price.hpp": HEADER, "price.cpp": ORIGINAL,
                           "templates.hpp": TEMPLATES, "abi.hpp": ABI_HEADER,
                           "abi.cpp": ABI_SOURCE}.items():
        (source / name).write_text(contents)
        (baseline / name).write_text(contents)
    executable = root / "target"
    flags = ["-std=c++17", "-g", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-fpatchable-function-entry=16,0"]
    compiler = str(Path(args.compiler).absolute())
    subprocess.run([compiler, *flags, "-Wl,--build-id=sha1", source / "main.cpp",
                    source / "price.cpp", source / "abi.cpp", "-o", executable], check=True)
    # The verified profile also records how a future TU should be compiled.
    commands = [{"directory": str(source), "file": str(source / name),
                 "arguments": [compiler, *flags, "-c", str(source / name), "-o", name + ".o"]}
                for name in ("main.cpp", "price.cpp", "discount.cpp", "abi.cpp")]
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
    send(repl, "%live revert")
    console.expect(r"^live REVERTED revision=0")
    (root / "templates").touch()
    app.expect(r"^templates 6 150 64 256 256 0 5$")
    (source / "templates.hpp").write_text(
        TEMPLATES.replace("return value;", "return value > 100 ? 100 : value;")
                 .replace("value ? 256", "value ? 512").replace("return 5;", "return 8;"))
    send(repl, "%live update source/templates.hpp --retention-limit 3")
    console.expect(r"^live APPLIED revision=3 retained=3/3")
    (root / "templates").touch()
    app.expect(r"^templates 6 100 64 512 512 0 8$")
    send(repl, "%live revert")
    console.expect(r"^live REVERTED revision=0")
    (root / "templates").touch()
    app.expect(r"^templates 6 150 64 256 256 0 5$")
    (root / "abi").touch()
    app.expect(r"^abi 11 -7 11 1 16 1 11 12$")
    (source / "abi.cpp").write_text(
        ABI_SOURCE.replace("initial + 1", "initial + 2")
                  .replace("throw initial;", "throw initial - 100;")
                  .replace("destroyed += value;", "destroyed += value * 2;")
                  .replace("Joined::self() { return this; }", "Joined::self() { return nullptr; }")
                  .replace("id + 5", "id + 10").replace("return a + b;", "return a - b;")
                  .replace("int total = 0;", "int total = 10;"))
    send(repl, "%live update source/abi.cpp --retention-limit 4")
    outcome = console.expect(r"^live (APPLIED|BLOCKED|STALE|RESTORATION_REQUIRED) revision=([0-9]+)(.*)$")
    assert outcome[1] == "APPLIED" and outcome[2] == "4" and "retained=4/4" in outcome[3], (
        "".join(console.seen[-25:]))
    (root / "abi").touch()
    app.expect(r"^abi 12 -107 35 0 21 1 3 22$")
    send(repl, "%live revert")
    console.expect(r"^live REVERTED revision=0")
    (root / "abi").touch()
    app.expect(r"^abi 11 -7 46 1 16 1 11 12$")
    send(repl, "%detach")
    console.expect(r"^detached session=")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for option in ("compiler", "repl", "server", "agent"):
        parser.add_argument("--" + option, required=True)
    with tempfile.TemporaryDirectory(prefix="ct-live-update-") as directory, ExitStack() as cleanup:
        verify(parser.parse_args(), Path(directory), cleanup)
