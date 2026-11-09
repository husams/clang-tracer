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
    env = {**os.environ, "XDG_CACHE_HOME": str(root / "cache")}
    for name in ("a", "b"):
        shutil.copyfile(examples / f"plugin_{name}.cpp", root / f"plugin_{name}.cpp")

    def run(commands, *, expected_status=0):
        result = subprocess.run(
            [repl], input=commands + "\n%quit\n", cwd=root, env=env,
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
    print(cold + warm + rejected + invalidated, end="")
