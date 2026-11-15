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
FLAGS = ["-O0", "-g", "-fno-inline", "-fno-omit-frame-pointer"]


def build_target(args, root, sources, headers, standard="c++23",
                 entry_flags=("-fpatchable-function-entry=16,0",)):
    target = root / "live-target"
    inputs = [*sources, *headers]
    baseline = root / "baseline"
    baseline.mkdir()
    for name in inputs:
        shutil.copyfile(root / name, baseline / name)
    command = [args.compiler, "-std=" + standard, *FLAGS, *entry_flags, "-I" + args.sdk]
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
#include <atomic>
#include <cstdio>
#include <iostream>
#include <string>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>
std::atomic<std::shared_ptr<example::Cache>> auto_cache;
std::atomic<std::shared_ptr<example::Account>> auto_account;
std::atomic<std::shared_ptr<example::Account>> auto_alias;
int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  using namespace example;
  g_primary_cache = std::make_shared<Cache>();
  secondary_cache = std::make_shared<Cache>();
  g_active_account = std::make_shared<Account>(1);
  accounts = {std::make_shared<Account>(2), std::make_shared<Account>(3)};
  auto unowned = std::make_unique<Account>(7);
  auto_cache.store(std::dynamic_pointer_cast<Cache>(g_primary_cache));
  auto_account.store(g_active_account);
  auto_alias.store(std::shared_ptr<Account>(g_active_account, unowned.get()));
  std::cout << "ready" << std::endl;
  for (std::string input;;) {
    usleep(2000);
    pollfd ready{STDIN_FILENO, POLLIN, 0};
    if (poll(&ready, 1, 0) <= 0)
      continue;
    if (!std::getline(std::cin, input))
      break;
    if (input == "clear") {
      auto_account.store({});
      std::puts("cleared");
    } else if (input == "addresses") {
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
        update = "%live update-data shared.cpp --discover-file discover.cpp --discover-function discover"
        repl(update + " --module libc.so.6", r"^live BLOCKED .*module filter differs")
        app("status", r"^cache=100/0 other=100/0 account=200/0 unowned=200/0$")
        repl(update + " --module live-target",
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
        repl("%live scan-heap --type example::Cache --type example::Account", r"^live CURRENT")
        roots = dict(zip(addresses.groups(), ("auto_cache", "auto_account", None)))
        ids = []
        for address in sorted(roots, key=lambda value: int(value, 16)):
            root = roots[address]
            evidence = f"shared owner observed under {root}" if root else "ownership unknown"
            ids.append(result_line(rf"\[(H\d+)\].*address={address}\b.*{evidence}")[1])
        app("clear", r"^cleared$")
        repl("%live update-data raw.cpp --from-scan " + " ".join(ids),
             r"^live APPLIED .*updated=1 skipped=2 failed=0")
        app("status", r"^cache=999/3 other=999/1 account=5000/2 unowned=200/0$")



MANAGED_MAIN = r"""
#include "state.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif
namespace managed = tracer::live::managed;
using namespace fixture;
managed::Registry registry;
extern "C" managed::Registry* clang_tracer_managed_registry_v1() noexcept { return &registry; }
extern "C" int cache_code();
extern "C" int cache_other();
extern "C" void cache_print(managed::Registry::Access&, managed::InstanceId);
std::array<unsigned char, 16> original_other;
std::atomic_bool corrupt_restore = false;

void write_other(bool unknown) {
  auto bytes = original_other;
  if (unknown)
    bytes[0] = 0xcc;
  const auto address = reinterpret_cast<std::uintptr_t>(&cache_other);
  // Inject stale bytes while preserving the target's ELF mapping boundaries.
  const auto memory = open("/proc/self/mem", O_WRONLY | O_CLOEXEC);
  if (memory == -1)
    std::abort();
  const auto written = pwrite(memory, bytes.data(), bytes.size(), static_cast<off_t>(address));
  close(memory);
  if (written != static_cast<ssize_t>(bytes.size()))
    std::abort();
}

extern "C" void fixture_hook_observe(int count) {
  if (cache_code() != 27 || cache_other() != 28)
    throw std::runtime_error("hook saw the old code");
  if (corrupt_restore && count == 1400)
    write_other(true);
}

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0)) return 1;
  std::memcpy(original_other.data(), reinterpret_cast<const void*>(&cache_other),
              original_other.size());
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
    if (command == "fault") {
      corrupt_restore = true;
      std::cout << "fault enabled" << std::endl;
      continue;
    }
    if (command == "repair") {
      const auto still_installed = cache_code();
      write_other(false);
      corrupt_restore = false;
      std::cout << "repair code " << still_installed << std::endl;
      continue;
    }
    auto access = registry.access();
    std::printf("code %d\n", cache_code());
    std::printf("other %d\n", cache_other());
    for (auto id : ids)
      cache_print(access, id);
    std::fflush(stdout);
  }
}
"""


def migration(args, folder):
    folder.mkdir()
    for name in ("state.hpp", "update.hpp", "after.cpp"):
        shutil.copyfile(ROOT / "live_migration" / name, folder / name)
    (folder / "main.cpp").write_text(MANAGED_MAIN)
    patch_source = folder / "cache_code.cpp"
    external_native_header = folder.parent / "outside_native.hpp"
    external_native_header.write_text("inline int cache_scale() { return 1; }\n")
    original_code = r"""
#include "state.hpp"
#include "../outside_native.hpp"
#include <cstdio>
using namespace fixture;
#define LIVE_ENTRY __attribute__((patchable_function_entry(16, 0)))
extern "C" LIVE_ENTRY int cache_code() { return cache_scale(); }
extern "C" LIVE_ENTRY int cache_other() { return 2; }
extern "C" LIVE_ENTRY void cache_print(managed::Registry::Access& access, managed::InstanceId id) {
  access.with<CacheV1>("Cache", id, "cache.1", [](const auto& cache) {
    std::printf("old %zu\n", cache.entries.size());
  });
}
"""
    patch_source.write_text(original_code)
    (folder / "gate.cpp").write_text("""
#include <tracer/live_migration.hpp>
namespace managed = tracer::live::managed;
extern managed::Registry registry;
managed::Registry::Access open_gate() { return registry.access(); }
""")
    target = build_target(args, folder, ["main.cpp", "cache_code.cpp", "gate.cpp"], ["state.hpp"],
                          standard="c++17", entry_flags=())
    shutil.copyfile(ROOT / "live_migration" / "state_v2.hpp", folder / "state_v2.hpp")
    source = (folder / "after.cpp").read_text()
    recipe = (folder / "update.hpp").read_text()
    (folder.parent / "outside_recipe.hpp").write_text(
        recipe.replace('"state_v2.hpp"', json.dumps(str(folder / "state_v2.hpp"))))
    (folder / "reject.cpp").write_text("#define FAIL_HOOK\n" + source)
    with ExitStack() as cleanup:
        app, repl, app_line, console_line = attach(args, target, cleanup)
        app("old", r"^old 0$")
        patched_code = (original_code.replace("return cache_scale();", "return 27;")
                        .replace("return 2;", "return 28;")
                        .replace('"state.hpp"', '"state_v2.hpp"')
                        .replace("CacheV1", "CacheV2").replace('"cache.1"', '"cache.2"')
                        .replace('"old %zu\\n", cache.entries.size()',
                                 '"new %zu %zu %d\\n", cache.entries.size(), '
                                 'cache.max_entries, cache.hook_calls'))
        update = "%live update update.hpp cache_code.cpp state_v2.hpp"
        patch_source.write_text(patched_code)
        external_native_header.write_text("inline int cache_scale() { return 2; }\n")
        repl(update + " ../outside_native.hpp --managed true",
             r"^live BLOCKED revision=0 .*native dependency outside the build source root")
        external_native_header.write_text("inline int cache_scale() { return 1; }\n")
        app("old", r"^code 1$")
        app_line(r"^old 0$")
        patch_source.write_text(patched_code.replace(
            "using namespace fixture;", "using namespace fixture;\nmanaged::Registry::Access open_gate();"
        ).replace("  access.with<CacheV2>", "  auto late_access = open_gate();\n"
                  "  late_access.with<CacheV2>"))
        repl(update + " --managed true", r"^live BLOCKED revision=0 .*incompatible native entry")
        console_line(r"^  BLOCKED .*acquire the managed registry gate in the caller")
        app("old", r"^old 0$")
        patch_source.write_text(patched_code)
        options = " --after-function fixture::after --for-each Cache"
        repl(update + " --after-file reject.cpp" + options,
             r"^live BLOCKED revision=0 .*second hook failed")
        app("old", r"^code 1$")
        app_line(r"^old 0$")
        app("fault", r"^fault enabled$")
        repl(update + " --after-file reject.cpp" + options,
             r"^live RESTORATION_REQUIRED revision=0 .*precondition")
        repl("%live status", r"^live RESTORATION_REQUIRED revision=0 .*%live recover")
        repl("%live recover", r"^live RESTORATION_REQUIRED revision=0 .*precondition")
        app("repair", r"^repair code 27$")
        repl("%live recover", r"^live REVERTED revision=0 .*managed publication restored")
        repl("%live recover", r"^live UNCHANGED revision=0")
        app("old", r"^code 1$")
        app_line(r"^other 2$")
        app_line(r"^old 0$")
        repl(update.replace("update.hpp", "../outside_recipe.hpp") + " --after-file after.cpp" + options,
             r"^live APPLIED revision=1 .*instances=3")
        app("new", r"^code 27$")
        app_line(r"^new 700 4796 1$")
        app_line(r"^new 1400 1656 1$")
        app_line(r"^new 0 1024 1$")
        repl("%live revert", r"^live REVERTED revision=0")
        app("old", r"^code 1$")
        app_line(r"^old 0$")
        repl(update + " --managed true", r"^live APPLIED revision=2 .*instances=3")
        app("new", r"^code 27$")
        app_line(r"^new 700 1024 0$")
        app_line(r"^new 1400 1024 0$")
        app_line(r"^new 0 1024 0$")
        repl("%live revert", r"^live REVERTED revision=0")
        app("old", r"^old 0$")
        repl("%live update update.hpp state_v2.hpp --managed true",
             r"^live APPLIED revision=3 .*instances=3")
        repl("%live revert", r"^live REVERTED revision=0")
        app("old", r"^old 0$")
        repl("%live update ../outside_recipe.hpp state_v2.hpp --managed true",
             r"^live APPLIED revision=4 .*instances=3")
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
