"""Exercise live data and managed layout commands against existing target objects."""

import argparse
from contextlib import ExitStack
from hashlib import sha256
import json
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

from repl_browser_integration import Transcript, stop

ROOT = Path(__file__).resolve().parent
FLAGS = ["-std=c++23", "-O0", "-g", "-fno-inline", "-fno-omit-frame-pointer"]


def build_target(args, root, sources, headers):
    target = root / "live-target"
    inputs = [*sources, *headers]
    baseline = root / "baseline"
    baseline.mkdir()
    for name in inputs:
        shutil.copyfile(root / name, baseline / name)
    command = [args.compiler, *FLAGS, "-I" + args.sdk]
    subprocess.run(
        [*command, "-Wl,--build-id=sha1", "-pthread", *sources, "-o", str(target)],
        cwd=root, check=True,
    )
    database = root / "compile_commands.json"
    database.write_text(json.dumps([
        {"directory": str(root), "file": str(root / name), "arguments":
         [*command, "-c", str(root / name), "-o", name + ".o"]}
        for name in sources
    ]))
    digest = lambda path: sha256(path.read_bytes()).hexdigest()
    (root / "clang-tracer-live.textproto").write_text(
        f"source_root: {json.dumps(str(root))}\n"
        f"snapshot_root: {json.dumps(str(baseline))}\n"
        f"module: {json.dumps(str(target))}\n"
        f'module_sha256: "{digest(target)}"\n'
        f'compilation_database_sha256: "{digest(database)}"\n'
        + "".join(f'inputs {{ path: "{name}" sha256: "{digest(baseline / name)}" }}\n'
                  for name in inputs)
    )
    return target


def attach(args, target, cleanup):
    def launch(argv):
        child = subprocess.Popen(argv, cwd=target.parent, text=True, stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        cleanup.callback(stop, child)
        return child, Transcript(child.stdout)

    target_process, app = launch([target])
    app.expect(r"^ready$")
    repl, console = launch([args.repl, "--server", args.server, "--agent", args.agent,
                            "--timeout", "60", "--attach", str(target_process.pid)])
    console.expect(rf"^attached session=\S+ pid={target_process.pid}$")

    def send(child, output, command, pattern):
        child.stdin.write(command + "\n")
        child.stdin.flush()
        return output.expect(pattern)

    return (lambda command, pattern: send(target_process, app, command, pattern),
            lambda command, pattern: send(repl, console, command, pattern),
            app.expect, console.expect)


DATA_MAIN = r"""
#include "model.hpp"
#include <cstdio>
#include <iostream>
#include <string>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>
int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  using namespace example;
  g_primary_cache = std::make_shared<Cache>();
  secondary_cache = std::make_shared<Cache>();
  g_active_account = std::make_shared<Account>(1);
  accounts = {std::make_shared<Account>(2), std::make_shared<Account>(3)};
  auto unowned = std::make_unique<Account>(7);
  std::cout << "ready" << std::endl;
  for (std::string input;;) {
    usleep(2000);
    pollfd ready{STDIN_FILENO, POLLIN, 0};
    if (poll(&ready, 1, 0) <= 0)
      continue;
    if (!std::getline(std::cin, input))
      break;
    if (input == "addresses") {
      std::printf("known cache=0x%lx account=0x%lx unowned=0x%lx\n",
          reinterpret_cast<unsigned long>(g_primary_cache.get()),
          reinterpret_cast<unsigned long>(g_active_account.get()),
          reinterpret_cast<unsigned long>(unowned.get()));
    } else {
      auto primary = std::dynamic_pointer_cast<Cache>(g_primary_cache);
      auto secondary = std::dynamic_pointer_cast<Cache>(secondary_cache);
      std::printf("cache=%d/%d other=%d/%d account=%d/%d unowned=%d/%d\n",
          primary->price(), primary->calls(), secondary->price(), secondary->calls(),
          g_active_account->limit(), g_active_account->calls(), unowned->limit(), unowned->calls());
    }
    std::fflush(stdout);
  }
}
"""


def live_data(args, folder):
    shutil.copytree(ROOT / "fixtures" / "live_data", folder)
    (folder / "main.cpp").write_text(DATA_MAIN)
    target = build_target(args, folder, ["main.cpp", "model.cpp"], ["model.hpp"])
    with ExitStack() as cleanup:
        app, repl, _, result_line = attach(args, target, cleanup)
        app("status", r"^cache=100/0 other=100/0 account=200/0 unowned=200/0$")
        repl("%live update-data shared.cpp --discover-file discover.cpp --discover-function discover",
             r"^live APPLIED .*updated=5 .*duplicates=1")
        app("status", r"^cache=999/1 other=999/1 account=5000/1 unowned=200/0$")
        addresses = app("addresses", r"^known cache=(0x\w+) account=(0x\w+) unowned=(0x\w+)$")
        repl("%live scan-heap --type example::Cache --type example::Account "
             "--discover-file discover.cpp --discover-function scan_owners", r"^live CURRENT")
        # The known allocations were created before attach. Scan results follow
        # ascending address order, with type and ownership evidence per row.
        ids = []
        for address in sorted(addresses.groups(), key=lambda value: int(value, 16)):
            result = result_line(rf"\[(H\d+)\].*address={address}\b")
            ids.append(result[1])
        repl("%live update-data raw.cpp --from-scan " + " ".join(ids),
             r"^live APPLIED .*updated=2 skipped=1 failed=0")
        app("status", r"^cache=999/2 other=999/1 account=5000/2 unowned=200/0$")


MANAGED_MAIN = r"""
#include "state.hpp"
#include <cstdio>
#include <iostream>
#include <string>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>
namespace managed = tracer::live::managed;
using namespace fixture;
managed::Registry registry;
extern "C" managed::Registry* clang_tracer_managed_registry_v1() noexcept { return &registry; }
extern "C" void fixture_hook_observe(int) {}

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  managed::InstanceId ids[3];
  {
    auto access = registry.access();
    managed::Schema before{managed::layout<CacheV1>("cache.1", "fixture::CacheV1"),
                           managed::create_from<CacheV1, Seed, make_v1>};
    access.add_type("Cache", before);
    access.add_type("Singleton", before);
    access.add_type("UninitializedSingleton", before);
    access.add_type("Tagged", {managed::layout<TaggedV1>("tagged.1", "fixture::TaggedV1"),
                               managed::create_default<TaggedV1>});
    const Seed seeds[]{{"search", 700}, {"catalog", 1400}, {"empty", 0}};
    for (int i = 0; i != 3; ++i)
      ids[i] = access.create("Cache", &seeds[i]);
  }
  std::cout << "ready" << std::endl;
  for (std::string command;;) {
    usleep(2000);
    pollfd ready{STDIN_FILENO, POLLIN, 0};
    if (poll(&ready, 1, 0) <= 0)
      continue;
    if (!std::getline(std::cin, command))
      break;
    auto access = registry.access();
    for (auto id : ids) {
      if (command == "old")
        access.with<CacheV1>("Cache", id, "cache.1", [](const auto& c) {
          std::printf("old %zu\n", c.entries.size());
        });
      else
        access.with<CacheV2>("Cache", id, "cache.2", [](const auto& c) {
          std::printf("new %zu %zu %d\n", c.entries.size(), c.max_entries, c.hook_calls);
        });
    }
    std::fflush(stdout);
  }
}
"""


def migration(args, folder):
    folder.mkdir()
    for name in ("state.hpp", "update.hpp", "after.cpp"):
        shutil.copyfile(ROOT / "live_migration" / name, folder / name)
    (folder / "main.cpp").write_text(MANAGED_MAIN)
    target = build_target(args, folder, ["main.cpp"], ["state.hpp"])
    source = (folder / "after.cpp").read_text()
    (folder / "reject.cpp").write_text("#define FAIL_HOOK\n" + source)
    with ExitStack() as cleanup:
        app, repl, app_line, _ = attach(args, target, cleanup)
        app("old", r"^old 0$")
        options = " --after-function fixture::after --for-each Cache"
        repl("%live update update.hpp --after-file reject.cpp" + options,
             r"^live BLOCKED revision=0 .*second hook failed")
        app("old", r"^old 0$")
        repl("%live update update.hpp --after-file after.cpp" + options,
             r"^live APPLIED revision=1 .*instances=3")
        app("new", r"^new 700 4796 1$")
        app_line(r"^new 1400 1656 1$")
        app_line(r"^new 0 1024 1$")
        repl("%live revert", r"^live REVERTED revision=0")
        app("old", r"^old 0$")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("compiler", "sdk", "agent", "server", "repl"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--case", choices=("data", "migration"), required=True)
    args = parser.parse_args()
    with TemporaryDirectory(prefix="clang-tracer-live-data-migration-") as name:
        root = Path(name)
        run = live_data if args.case == "data" else migration
        run(args, root / args.case)
