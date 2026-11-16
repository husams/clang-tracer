import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


repl, examples = map(Path, sys.argv[1:])

with tempfile.TemporaryDirectory(prefix="tracer-repl-plugins-") as directory:
    root = Path(directory)
    env = {
        **os.environ,
        "XDG_CACHE_HOME": str(root / "cache"),
        "XDG_CONFIG_HOME": str(root / "config"),
        "XDG_DATA_HOME": str(root / "data"),
    }
    for name in ("a", "b"):
        shutil.copyfile(examples / f"plugin_{name}.cpp", root / f"plugin_{name}.cpp")

    def run(commands, *, expected_status=0, cwd=root):
        result = subprocess.run(
            [repl], input=commands + "\n%quit\n", cwd=cwd, env=env,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=40,
        )
        assert result.returncode == expected_status, result.stdout
        return result.stdout

    load = '%plugin load "plugin_a.cpp" --as a\n%plugin load "plugin_b.cpp" --as b\n'
    cold = run(load + """%plugin run a address
%plugin run b address
%plugin run a tick
%plugin run b tick
%plugin run a tick
%plugin run b tick
%plugin reload a
%plugin run a address
%plugin run b address
%plugin run a value
%plugin run b value
%plugin run a tick
%plugin run b tick""")
    assert "a generation 1 active (compiled)" in cold, cold
    assert "b generation 1 active (compiled)" in cold, cold
    assert "a generation 2 active (cache hit)" in cold, cold
    addresses = {
        name: re.findall(rf"^{name}\.address = (\d+)$", cold, re.MULTILINE)
        for name in ("a", "b")
    }
    assert len(addresses["a"]) == len(addresses["b"]) == 2, cold
    assert addresses["a"][0] != addresses["a"][1], cold
    assert addresses["b"][0] == addresses["b"][1], cold
    assert addresses["b"][0] not in addresses["a"], cold
    assert re.findall(r"^[ab]\.(?:tick|value) = \d+$", cold, re.MULTILINE) == [
        "a.tick = 1", "b.tick = 10", "a.tick = 2", "b.tick = 20",
        "a.value = 0", "b.value = 20", "a.tick = 1", "b.tick = 30",
    ], cold

    warm = run(load + "%plugin run a value\n%plugin run b value")
    assert "a generation 1 active (cache hit)" in warm, warm
    assert "b generation 1 active (cache hit)" in warm, warm
    assert "a.value = 0" in warm and "b.value = 0" in warm, warm
    hints = run(load + "%help plugin load\n%plugin commands a")
    assert "%plugin load FILE --as NAME" in hints, hints
    assert "%plugin run a tick  Advance the counter" in hints, hints

    (root / "instance.cpp").write_text("""#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t count_ = 0;

public:
  uint64_t tick() noexcept { return ++count_; }
  uint64_t value() const noexcept { return count_; }

  static constexpr Command commands[] = {
      command<&Plugin::tick>("tick", "Increment the count."),
      command<&Plugin::value>("value", "Read the count."),
  };
};
""")
    (root / "legacy.cpp").write_text("""#include "clang_tracer_repl_plugin.h"

namespace {
uint64_t value() { return 42; }
constexpr TracerReplCommandV1 command{"value", "Read the value.", value};
constexpr TracerReplPluginV1 descriptor{sizeof(descriptor), 1, &command};
}

const TracerReplPluginV1 *tracer_repl_plugin_v1() { return &descriptor; }
""")
    mixed = run("""%plugin load instance.cpp --as instance
%plugin run instance tick
%plugin run instance value
%plugin load legacy.cpp --as legacy
%plugin run legacy value
%plugin reload instance
%plugin run instance value""")
    assert re.findall(r"^(?:instance|legacy)\.\w+ = \d+$", mixed, re.MULTILINE) == [
        "instance.tick = 1", "instance.value = 1", "legacy.value = 42",
        "instance.value = 0",
    ], mixed

    # A missing global must not bind to the same symbol in another plugin.
    source_b = (root / "plugin_b.cpp").read_text()
    (root / "broken.cpp").write_text(source_b.replace(
        "unsigned plugin_counter = 0;", "extern unsigned plugin_counter;"))
    rejected = run(load + """%plugin run b tick
%shell "cp broken.cpp plugin_b.cpp"
%plugin reload b
%plugin run b tick
%plugin list""", expected_status=1)
    assert "plugin_counter" in rejected, rejected
    assert "b.tick = 20" in rejected, rejected
    assert "b generation 1 " in rejected and "b generation 2 " not in rejected, rejected

    # The preprocessed input changes when only a transitive header changes.
    (root / "plugin_b.cpp").write_text(
        '#include "step.hpp"\n' + source_b.replace("+= 10", "+= STEP"))
    (root / "step.hpp").write_text("#define STEP 10\n")
    (root / "next.hpp").write_text("#define STEP 11\n")
    invalidated = run(load + """%plugin run b tick
%shell "cp next.hpp step.hpp"
%plugin reload b
%plugin run b tick""")
    assert "b generation 2 active (compiled)" in invalidated, invalidated
    assert "b.tick = 10" in invalidated and "b.tick = 11" in invalidated, invalidated
    assert "(no plugins loaded)" in run("%plugin list")
    (root / "plugin_b.cpp").write_text(source_b)
    enabled = run("""%plugin enable "plugin_a.cpp" --as a
%plugin enable "plugin_b.cpp" --as b
%help plugin enable
%plugin run a tick
%plugin run b tick""")
    assert "a enabled at startup" in enabled and "b enabled at startup" in enabled, enabled
    assert "%plugin enable FILE --as NAME" in enabled, enabled
    assert "a.tick = 1" in enabled and "b.tick = 10" in enabled, enabled

    startup_cwd = root / "other-directory"
    startup_cwd.mkdir()
    shutil.rmtree(root / "cache")
    cold_start = run("%plugin run a tick\n%plugin run b tick", cwd=startup_cwd)
    assert "a generation 1 active (compiled)" in cold_start, cold_start
    assert "b generation 1 active (compiled)" in cold_start, cold_start
    assert "a.tick = 1" in cold_start and "b.tick = 10" in cold_start, cold_start

    warm_start = run("""%plugin run a address
%plugin run b address
%plugin run a tick
%plugin run b tick
%plugin reload a
%plugin run a address
%plugin run b address
%plugin run a value
%plugin run b value""", cwd=startup_cwd)
    assert "a generation 1 active (cache hit)" in warm_start, warm_start
    assert "b generation 1 active (cache hit)" in warm_start, warm_start
    assert "a generation 2 active (cache hit)" in warm_start, warm_start
    assert "a.value = 0" in warm_start and "b.value = 10" in warm_start, warm_start
    a_first, a_second = re.findall(r"^a\.address = (\d+)$", warm_start, re.MULTILINE)
    b_first, b_second = re.findall(r"^b\.address = (\d+)$", warm_start, re.MULTILINE)
    assert a_first != a_second and b_first == b_second, warm_start
    assert b_first not in (a_first, a_second), warm_start

    source_a = root / "plugin_a.cpp"
    source_a.rename(root / "missing_a.cpp")
    skipped = run("""%plugin run b tick
%plugin disable a
%plugin disable b
%plugin run b tick""", cwd=startup_cwd)
    assert "startup plugin a:" in skipped, skipped
    assert "b generation 1 active (cache hit)" in skipped, skipped
    assert "b.tick = 10" in skipped and "b.tick = 20" in skipped, skipped
    assert "(no plugins loaded)" in run("%plugin list")
    print(cold + warm + rejected + invalidated + cold_start + warm_start, end="")

    # Install a package by copying its files and start the REPL normally.
    package = root / "data/clang-tracer/plugins/my-api"
    shutil.copytree(examples.parent / "repl-public-plugin", package)
    with (package / "src/my_type.cpp").open("a") as source:
        source.write("\nunsigned private_plugin_counter = 77;\n")
    (package / "src/private_detail.hpp").write_text("#pragma once\n")

    usage = """#include <my_api/my_type.hpp>
my_api::my_instance.add(3);
my_api::MyType local{};
local.add(5);
std::cout << "shared=" << my_api::my_instance.count << " local=" << local.count << std::endl;
"""
    first_start = run(usage, cwd=startup_cwd)
    assert "my-api generation 1 active (compiled)" in first_start, first_start
    assert "shared=3 local=5" in first_start, first_start
    second_start = run(usage, cwd=startup_cwd)
    assert "my-api generation 1 active (cache hit)" in second_start, second_start
    assert "shared=3 local=5" in second_start, second_start
    shared_with_command = run(usage + "%plugin run my-api value")
    assert "my-api.value = 3" in shared_with_command, shared_with_command

    hidden_class = run("#include <my_api/my_type.hpp>\ntracer::repl::Plugin hidden;", expected_status=1)
    assert "error:" in hidden_class and "Plugin" in hidden_class, hidden_class
    hidden_header = run("#include <private_detail.hpp>", expected_status=1)
    assert "private_detail.hpp" in hidden_header and "file not found" in hidden_header, hidden_header
    hidden_symbol = run("""extern unsigned private_plugin_counter;
std::cout << private_plugin_counter << std::endl;
""", expected_status=1)
    assert "Symbols not found" in hidden_symbol and "private_plugin_counter" in hidden_symbol, hidden_symbol
    hidden_entry = run("""extern "C" const void *tracer_repl_plugin_v1();
std::cout << tracer_repl_plugin_v1() << std::endl;
""", expected_status=1)
    assert "Symbols not found" in hidden_entry and "tracer_repl_plugin_v1" in hidden_entry, hidden_entry
    print(first_start + second_start, end="")

    project = root / "project"
    (project / "headers").mkdir(parents=True)
    (project / "headers/project_offset.hpp").write_text("#define PROJECT_OFFSET 4\n")
    (project / "main.cpp").write_text("int main() {}\n")
    database = project / "compile_commands.json"
    database.write_text(json.dumps([{
        "directory": str(project),
        "file": "main.cpp",
        "arguments": ["clang++", "-Iheaders", "-c", "main.cpp"],
    }]))
    (package / "extra").mkdir()
    (package / "extra/plugin_offset.hpp").write_text("#define PLUGIN_OFFSET 6\n")
    manifest = package / "plugin.toml"
    with manifest.open("a") as configuration:
        configuration.write(f"compile_commands = {json.dumps(str(database))}\n")
        configuration.write('extra_flags = ["-I", "extra"]\n')
    source = package / "src/my_type.cpp"
    source.write_text(
        '#include "project_offset.hpp"\n#include "plugin_offset.hpp"\n'
        + source.read_text().replace("count += amount;", "count += amount + PROJECT_OFFSET + PLUGIN_OFFSET;")
    )
    configured = run(usage, cwd=startup_cwd)
    assert "my-api generation 1 active (compiled)" in configured, configured
    assert "shared=13 local=15" in configured, configured

    # Reject mutable installed sources, headers and their containing directories.
    for entry in (package / "src", source, package / "include/my_api/my_type.hpp"):
        original_mode = entry.stat().st_mode & 0o777
        entry.chmod(original_mode | 0o022)
        try:
            unsafe = run("%plugin list", cwd=startup_cwd)
            assert "plugin package entry must be controlled by its owner" in unsafe, unsafe
            assert str(entry) in unsafe and "my-api generation" not in unsafe, unsafe
        finally:
            entry.chmod(original_mode)

    (root / "requires-libc.cpp").write_text("""#include <cstdio>
#include "clang_tracer_repl_plugin.hpp"

class tracer::repl::Plugin : public tracer::repl::PluginInterface<Plugin> {
  uint64_t greet() noexcept { return std::puts("hello"); }

public:
  static constexpr Command commands[] = {
    command<&Plugin::greet>("greet", "Print a greeting."),
  };
};
""")
    unresolved = run(f'%plugin load "{root / "requires-libc.cpp"}" --as external', expected_status=1)
    assert "unresolved symbols in plugin external" in unresolved and "puts" in unresolved, unresolved
    assert "Failed to materialize symbols" not in unresolved, unresolved

    invalid = root / "bad-location.cpp"
    invalid.write_text('#include "clang_tracer_repl_plugin.hpp"\nint broken = missing_name;\n')
    compile_error = run(f'%plugin load "{invalid}" --as invalid', expected_status=1)
    assert "bad-location.cpp:2:" in compile_error, compile_error
