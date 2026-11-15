"""Regression cases for live entry, layout and linkage publication."""

from argparse import ArgumentParser
from contextlib import ExitStack
import json
import os
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

from repl_live_update import Output, send, sha, stop


DRIVER = r"""
#include <cstdio>
#include <thread>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif
int first(int);
int second(int);
int lookup(int);
int third(int);
int adjacent_value();
int calculate(int);
int accepted_key();
int registered(int);
int persistent_capture(int);
int local_capture(int);
int policy_check(int);
int count_above(int);
int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  std::thread([] { for (;;) usleep(100000); }).detach();
  std::puts("READY");
  std::fflush(stdout);
  for (;;) {
    if (!access("call", F_OK)) {
      unlink("call");
      int one;
      try { one = first(10); } catch (int value) { one = value; }
      std::printf("%d %d %d %d %d %d %d %d %d %d %d %d\n", one, second(10), lookup(1), third(10),
                  adjacent_value(), calculate(10), accepted_key(), registered(10), policy_check(10),
                  count_above(2), persistent_capture(10), local_capture(10));
      std::fflush(stdout);
    }
    usleep(3000);
  }
}
"""

APP = """
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>
#include <unistd.h>
#include "system_policy.hpp"
#define LIVE_ENTRY __attribute__((patchable_function_entry(16, 0)))
struct Config { int a; };
struct Storage { Config config; int adjacent; };
struct Handle { int* copies; };
int tracked_copies() {
  int copies = 0;
  Handle handle{&copies};
  std::vector<Handle> values;
  values.push_back(handle);
  return copies;
}
Storage storage{{1}, 7};
static const int table[] = {1, 2, 3};
int lookup(int i) { const char *empty = ""; return table[i] + empty[0]; }
LIVE_ENTRY int first(int i) { return i + 1; }
LIVE_ENTRY int second(int i) { return i + 2; }
LIVE_ENTRY int policy_check(int i) { return policy_total(i) + 1; }
LIVE_ENTRY int count_above(int threshold) {
  std::array<int, 4> values{1, 2, 3, 4};
  const auto predicate = [=](const auto& value) {
    return value > threshold;
  };
  return std::count_if(values.begin(), values.end(), predicate);
}
LIVE_ENTRY int registered(int i) {
  static int (*const callback)(int) = [](int v) LIVE_ENTRY { return v + 17; };
  return callback(i);
}
LIVE_ENTRY int persistent_capture(int i) {
  int a = 5, b = 2;
  static auto stored = [=](int v) { return v + a * 10 + b; };
  auto copy = stored;
  return copy(i);
}
LIVE_ENTRY int local_capture(int i) {
  auto add = [&](int n) { return i + n; };
  return add(2);
}
static int counter = 1;
LIVE_ENTRY int calculate(int i) { int values[] = {1, 2, 3}; return values[i % 3] + counter; }
int adjacent_value() { return storage.adjacent; }
int vector_entry(int i) { std::vector<int> values; values.push_back(i); return values.back(); }
int formatted(int i) { std::ostringstream out; out << i; return out.str().size(); }
std::optional<std::string> label(int i) {
  auto stringify = [&] { return std::to_string(i); };
  return i < 0 ? std::nullopt : std::optional{stringify()};
}
LIVE_ENTRY int accepted_key() {
  auto width = [](std::string_view key) { return key.size(); };
  const std::string_view name = "deposit";
  const std::array<std::string_view, 2> keys{"deposit", "withdraw"};
  for (const auto &key : keys)
    if (name == key) return width(key);
  for (auto key : {std::string_view{"settled"}, std::string_view{"pending"}})
    if (name == key) return width(key);
  return 0;
}
"""

OTHER = """
#include <string>
#include <vector>
#define LIVE_ENTRY __attribute__((patchable_function_entry(16, 0)))
static int counter = 2;
LIVE_ENTRY static int helper(int i) { return i + 3; }
LIVE_ENTRY int third(int i) { return helper(i) + counter; }
int names(int i) {
  std::vector<std::string> values{"entry " + std::to_string(i)};
  return values.front().size();
}
"""

SYSTEM_POLICY = """
#pragma GCC system_header
inline int policy_scale(int i) { return i * 2; }
inline int policy_total(int i) { return policy_scale(i) + 1; }
"""

PRELOAD = """
#include <cstdlib>
#include <dlfcn.h>
extern "C" void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
  if (alignment == 4096 && size == 1200128) return nullptr;
  using Allocator = decltype(&aligned_alloc);
  static const auto next = reinterpret_cast<Allocator>(::dlsym(RTLD_NEXT, "aligned_alloc"));
  return next(alignment, size);
}
"""


def run(args, root, cleanup):
    source = root / "source"
    baseline = root / "baseline"
    source.mkdir()
    baseline.mkdir()
    for name, content in {"driver.cpp": DRIVER, "app.cpp": APP, "other.cpp": OTHER,
                          "system_policy.hpp": SYSTEM_POLICY}.items():
        (source / name).write_text(content)
        (baseline / name).write_text(content)
    flags = ["-std=c++17", "-g", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-pthread"]
    target = root / "target"
    preload = root / "preload.cpp"
    preload.write_text(PRELOAD)
    allocator = root / "allocator.so"
    subprocess.run([args.compiler, "-std=c++17", "-fPIC", "-shared", preload, "-ldl", "-o", allocator],
                   check=True)
    subprocess.run([args.compiler, *flags, "-Wl,--build-id=sha1", source / "driver.cpp",
                    source / "app.cpp", source / "other.cpp", "-o", target], check=True)
    database = root / "compile_commands.json"
    database.write_text(json.dumps([
        {"directory": str(source), "file": str(source / name),
         "arguments": [args.compiler, *flags, "-c", str(source / name), "-o", name + ".o"]}
        for name in ("driver.cpp", "app.cpp", "other.cpp")
    ]))
    (root / "clang-tracer-live.textproto").write_text(
        f'source_root: {json.dumps(str(source))}\n'
        f'snapshot_root: {json.dumps(str(baseline))}\n'
        f'module: {json.dumps(str(target))}\n'
        f'module_sha256: "{sha(target)}"\n'
        f'compilation_database_sha256: "{sha(database)}"\n' +
        "".join(f'inputs {{ path: "{path.name}" sha256: "{sha(path)}" }}\n'
                for path in sorted(baseline.iterdir()))
    )
    (root / "pass.cpp").write_text(
        "int passthrough(const LiveMockContext &ctx, int value) { return ctx.original(value); }\n"
    )
    (root / "increment.cpp").write_text(
        "int increment(const LiveMockContext &ctx, int value) { return ctx.original(value) + 1000; }\n"
    )

    def launch(*argv, env=None):
        child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, env=env)
        cleanup.callback(stop, child)
        return child, Output(child.stdout, 60)

    preload_values = [str(allocator), os.environ.get("LD_PRELOAD", "")]
    child, app = launch(target, env={**os.environ, "LD_PRELOAD": " ".join(filter(None, preload_values))})
    app.expect(r"^READY$")
    repl, console = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(child.pid), "--timeout", "60")
    console.expect(rf"^attached session=\S+ pid={child.pid}$")

    def command(line, outcome):
        send(repl, line)
        result = console.expect(r"^live (APPLIED|BLOCKED|STALE|REVERTED|UNCHANGED|RESTORATION_REQUIRED) revision=")
        assert result[1] == outcome, "".join(console.seen[-25:])

    def call(first, second, *, third=15, calculated=3, key=7, callback=27,
             policy=22, count=2, local=12, runtime=False):
        (root / "call").touch()
        if runtime:
            app.expect(r"^runtime=10$")
        app.expect(rf"^{first} {second} 2 {third} 7 {calculated} {key} {callback} {policy} {count} 62 {local}$")

    app_source = source / "app.cpp"
    call(11, 12)
    app_source.write_text(APP.replace("value > threshold", "value >= threshold"))
    command("%live update source/app.cpp --retention-limit 32", "APPLIED")
    call(11, 12, count=3)
    mixed = APP.replace("value > threshold", "value < threshold").replace("return i + 2;", "return i + 20;")
    app_source.write_text(mixed)
    command("%live update source/app.cpp", "APPLIED")
    call(11, 30, count=1)
    # A later caller-only update must keep using the private predicate.
    app_source.write_text(mixed.replace("return std::count_if", "return 10 + std::count_if"))
    command("%live update source/app.cpp", "APPLIED")
    call(11, 30, count=11)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    revised = APP.replace("value > threshold", "value >= threshold")
    app_source.write_text(revised.replace(
        "return std::count_if(values.begin(), values.end(), predicate);", "return predicate(2);"))
    command("%live update source/app.cpp", "APPLIED")
    call(11, 12, count=1)
    app_source.write_text(revised)
    command("%live update source/app.cpp", "APPLIED")
    call(11, 12, count=3)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    app_source.write_text(APP.replace(
        "  auto add =", "  auto twice = [](int n) { return n * 2; };\n  i = twice(i);\n  auto add ="))
    command("%live update source/app.cpp", "APPLIED")
    call(11, 12, local=22)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    # Emitting policy_scale first adds nounwind to policy_total without
    # changing its instructions or introducing an unwind edge.
    app_source.write_text(APP.replace("return policy_total(i) + 1;",
                                      "return policy_scale(i) + policy_total(i) + 1;"))
    command("%live update source/app.cpp", "APPLIED")
    call(11, 12, policy=42)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    # glibc would allocate this block. Only the target's preloaded allocator
    # returns null, so an update must observe the target's lookup order.
    app_source.write_text(APP.replace("return i + 1;", """
      auto* block = std::aligned_alloc(4096, 1200128);
      if (!block) return i + 1000;
      std::free(block);
      return i + 1;"""))
    command("%live update source/app.cpp", "APPLIED")
    call(1010, 12)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    revised_callback = APP.replace("return v + 17;", "return v + 100;")
    app_source.write_text(revised_callback)
    compiler_version = subprocess.check_output([args.compiler, "--version"], text=True)
    if "clang" in compiler_version.splitlines()[0].lower():
        command("%live update source/app.cpp", "APPLIED")
        call(11, 12, callback=110)
        command("%live revert 0", "REVERTED")
    else:
        # GCC's function-scope lambda linkage name differs from Clang's.
        command("%live update source/app.cpp", "BLOCKED")
    call(11, 12)
    app_source.write_text(revised_callback.replace(
        "  static int (*const callback)(int)", "  [[maybe_unused]] auto prior = [] { return 0; };\n"
                                             "  static int (*const callback)(int)"))
    command("%live update source/app.cpp", "BLOCKED")
    console.expect(r"cannot prove the existing lambda's calls")
    call(11, 12)
    app_source.write_text(APP.replace("a * 10 + b", "b * 100 + a"))
    command("%live update source/app.cpp", "BLOCKED")
    console.expect(r"lambda capture layout or callable ABI changed")
    call(11, 12)
    layout = APP.replace("struct Config { int a; }", "struct Config { int a; int b; }")
    layout = layout.replace("return i + 1;", "storage.config.b = 99; return i + 1;")
    app_source.write_text(layout)
    command("%live update source/app.cpp", "BLOCKED")
    call(11, 12)
    app_source.write_text(APP.replace("struct Handle { int* copies; };", """
struct Handle {
  int* copies;
  explicit Handle(int* counter) : copies(counter) {}
  Handle(const Handle& other) : copies(other.copies) { ++*copies; }
};"""))
    command("%live update source/app.cpp", "BLOCKED")
    console.expect(r"existing record layout changed")
    call(11, 12)
    app_source.write_text(APP.replace("table[] = {1, 2, 3}", "table[] = {1, 9, 3}"))
    command("%live update source/app.cpp", "BLOCKED")
    call(11, 12)

    with_local = APP.replace("LIVE_ENTRY int first",
                             "static int helper(int i) { return i * 1000; }\nLIVE_ENTRY int first")
    with_local = with_local.replace("return i + 1;", 'return helper(i) + "new"[i % 3];')
    with_local = with_local.replace("return i + 2;", "return i + 20;")
    app_source.write_text(with_local)
    command("%live update source/app.cpp", "APPLIED")
    call(10101, 30)
    command('%live mock "first(int)" --file pass.cpp --function passthrough', "APPLIED")
    call(10101, 30)
    command("%live revert 0", "REVERTED")
    call(11, 12)

    app_source.write_text(APP.replace("return i + 1;", """
      std::optional<int> value{i};
      auto bump = [&] { return *value + 10; };
      return bump();""").replace("return key.size();", "return key.size() + 1;", 1)
      .replace("  auto width =", "  auto unused = [] { return 0; };\n"
                                 "  if (unused()) return 0;\n  auto width ="))
    command("%live update source/app.cpp", "APPLIED")
    call(20, 12, key=8)
    command("%live revert 0", "REVERTED")

    (source / "extra.cpp").write_text("int extra(int i) { return i * 8; }")
    with_extra = "int extra(int);\n" + APP.replace("return i + 1;", "return extra(i);")
    app_source.write_text(with_extra)
    command("%live update source/app.cpp source/extra.cpp", "APPLIED")
    call(80, 12)
    app_source.write_text(with_extra.replace("return extra(i);", "return extra(i) + 1;"))
    command("%live update source/app.cpp", "APPLIED")
    call(81, 12)
    command("%live revert 0", "REVERTED")
    app_source.write_text(APP.replace("values[] = {1, 2, 3}", "values[] = {1, 9, 3}")
                         .replace("return i + 1;",
                                  'usleep(1); std::fprintf(stderr, "runtime=%d\\n", i); '
                                  'if (std::time(nullptr) <= 0) return -1; throw i + 79;'))
    send(repl, "%live update source/app.cpp --payload-limit 1")
    console.expect(r"^live BLOCKED .*exceeds --payload-limit")
    call(11, 12)
    command("%live update source/app.cpp --payload-limit 33554432", "APPLIED")
    call(89, 12, calculated=10, runtime=True)
    other_source = source / "other.cpp"
    other_source.write_text("#include <iostream>\n" + OTHER.replace(
        "return helper(i) + counter;", 'std::cerr << ""; return i + 50 + counter;')
        .replace("LIVE_ENTRY static int helper", "[[maybe_unused]] LIVE_ENTRY static int helper"))
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, third=62, calculated=10, runtime=True)
    other_source.write_text(OTHER)
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, calculated=10, runtime=True)
    other_source.write_text(OTHER.replace("return i + 3;", "return i + 30;"))
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, third=42, calculated=10, runtime=True)
    command('%live mock "helper(int)" --file increment.cpp --function increment', "APPLIED")
    call(89, 12, third=1042, calculated=10, runtime=True)
    other_source.write_text(OTHER.replace("return helper(i) + counter;", "return i + 5;"))
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, calculated=10, runtime=True)
    other_source.write_text(OTHER)
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, third=1015, calculated=10, runtime=True)
    send(repl, "%live status")
    console.expect(r"^  APPLIED helper\(int\).*increment\.cpp")
    command("%live revert", "REVERTED")
    call(89, 12, calculated=10, runtime=True)
    command("%live revert", "REVERTED")
    call(89, 12, third=1042, calculated=10, runtime=True)
    send(repl, "%detach")
    console.expect(r"^detached session=")
    call(11, 12)


if __name__ == "__main__":
    parser = ArgumentParser()
    for name in ("compiler", "repl", "server", "agent"):
        parser.add_argument("--" + name, required=True)
    with TemporaryDirectory(prefix="ct-live-review-") as directory, ExitStack() as cleanup:
        run(parser.parse_args(), Path(directory), cleanup)
