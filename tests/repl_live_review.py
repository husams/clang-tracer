"""Regression cases for live entry, layout and linkage publication."""

from argparse import ArgumentParser
from contextlib import ExitStack
import json
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
      std::printf("%d %d %d %d %d %d\n", one, second(10), lookup(1), third(10),
                  adjacent_value(), calculate(10));
      std::fflush(stdout);
    }
    usleep(3000);
  }
}
"""

APP = """
#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#define LIVE_ENTRY __attribute__((patchable_function_entry(16, 0)))
struct Config { int a; };
struct Storage { Config config; int adjacent; };
Storage storage{{1}, 7};
static const int table[] = {1, 2, 3};
int lookup(int i) { const char *empty = ""; return table[i] + empty[0]; }
LIVE_ENTRY int first(int i) { return i + 1; }
LIVE_ENTRY int second(int i) { return i + 2; }
static int counter = 1;
LIVE_ENTRY int calculate(int i) { int values[] = {1, 2, 3}; return values[i % 3] + counter; }
int adjacent_value() { return storage.adjacent; }
int vector_entry(int i) { std::vector<int> values; values.push_back(i); return values.back(); }
int formatted(int i) { std::ostringstream out; out << i; return out.str().size(); }
"""

OTHER = """
#include <string>
#include <vector>
#define LIVE_ENTRY __attribute__((patchable_function_entry(16, 0)))
static int counter = 2;
static int helper(int i) { return i + 3; }
LIVE_ENTRY int third(int i) { return helper(i) + counter; }
int names(int i) {
  std::vector<std::string> values{"entry " + std::to_string(i)};
  return values.front().size();
}
"""


def run(args, root, cleanup):
    source = root / "source"
    baseline = root / "baseline"
    source.mkdir()
    baseline.mkdir()
    for name, content in {"driver.cpp": DRIVER, "app.cpp": APP, "other.cpp": OTHER}.items():
        (source / name).write_text(content)
        (baseline / name).write_text(content)
    flags = ["-std=c++17", "-g", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
             "-fcf-protection=none", "-pthread"]
    target = root / "target"
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

    def launch(*argv):
        child = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
        cleanup.callback(stop, child)
        return child, Output(child.stdout, 60)

    child, app = launch(target)
    app.expect(r"^READY$")
    repl, console = launch(args.repl, "--server", args.server, "--agent", args.agent,
                           "--attach", str(child.pid), "--timeout", "60")
    console.expect(rf"^attached session=\S+ pid={child.pid}$")

    def command(line, outcome):
        send(repl, line)
        result = console.expect(r"^live (APPLIED|BLOCKED|STALE|REVERTED|UNCHANGED|RESTORATION_REQUIRED) revision=")
        assert result[1] == outcome, "".join(console.seen[-25:])

    def call(first, second, *, third=15, calculated=3, runtime=False):
        (root / "call").touch()
        if runtime:
            app.expect(r"^runtime=10$")
        app.expect(rf"^{first} {second} 2 {third} 7 {calculated}$")

    app_source = source / "app.cpp"
    call(11, 12)
    layout = APP.replace("struct Config { int a; }", "struct Config { int a; int b; }")
    layout = layout.replace("return i + 1;", "storage.config.b = 99; return i + 1;")
    app_source.write_text(layout)
    command("%live update source/app.cpp", "BLOCKED")
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
    (source / "other.cpp").write_text("#include <iostream>\n" + OTHER.replace(
        "return helper(i) + counter;", 'std::cerr << ""; return i + 30 + counter;'))
    command("%live update source/other.cpp", "APPLIED")
    call(89, 12, third=42, calculated=10, runtime=True)
    send(repl, "%detach")
    console.expect(r"^detached session=")
    call(11, 12)


if __name__ == "__main__":
    parser = ArgumentParser()
    for name in ("compiler", "repl", "server", "agent"):
        parser.add_argument("--" + name, required=True)
    with TemporaryDirectory(prefix="ct-live-review-") as directory, ExitStack() as cleanup:
        run(parser.parse_args(), Path(directory), cleanup)
