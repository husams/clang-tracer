"""Integration contract for AST-only wrappers, reuse, shell flags and compdb."""

import argparse
import json
from pathlib import Path
import subprocess
import struct
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("repl")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="tracer-header-reuse-") as directory:
        root = Path(directory)
        include = root / "app include"
        include.mkdir()
        cache = root / "cache"
        dependency = include / "leaf.hpp"
        dependency.write_text("#pragma once\nnamespace app { struct Leaf { int n=0; }; }\n")
        header = include / "types.hpp"
        header.write_text('''#pragma once
#include "leaf.hpp"
#include <string>
#include <vector>
#ifndef APP_ENTRY
#error application flags missing
#endif
namespace app {
struct Item { std::string name; std::vector<Leaf> leaves; };
struct Other { double ratio=0; };
inline constexpr int entry=APP_ENTRY;
}
''')
        unavailable = include / "unavailable.hpp"
        unavailable.write_text('''#pragma once
extern int missing_application_function();
inline int must_not_execute=missing_application_function();
namespace app { struct External { External(); ~External(); int value=0; }; }
''')
        flags = root / "flags.txt"
        flags.write_text(f'-I"{include}" -DAPP_ENTRY=7\n')
        output = root / "generated.hpp"

        def run(commands, success=True):
            import os
            env = dict(os.environ, XDG_CACHE_HOME=str(cache))
            reply = subprocess.run([args.repl, "--endpoint", "127.0.0.1:1"],
                                   input=commands + "\n%quit\n", text=True,
                                   capture_output=True, cwd=root, env=env, timeout=120)
            if success:
                assert reply.returncode == 0, (reply.stdout, reply.stderr)
            return reply

        shell = run(f'''%shell "printf shell-output"
%cpp options add `cat '{flags}'`
%cpp generate "types.hpp" --output "{output}"
std::cout << "generation-kept-session" << std::endl;
''')
        assert "shell-output" in shell.stdout
        assert "generation-kept-session" in shell.stdout
        generated = output.read_text()
        for name in ("app::Item", "app::Other", "app::Leaf"):
            assert f"EventRecord<::{name}>" in generated, generated
        assert "#pragma once" in generated
        loaded = run(f'''%cpp options add "-I{include}" "-DAPP_ENTRY=7"
%cpp import "types.hpp" --output "{output}"
%cpp import "types.hpp" --output "{output}"
''')
        assert "reused" in loaded.stdout.lower(), loaded.stdout
        contextual = run(f'''%cpp options add "-I{include}" "-DAPP_ENTRY=7"
int retained=7;
%cpp import "types.hpp" --output "{root / 'contextual.hpp'}"
%cpp import "types.hpp" --output "{root / 'contextual.hpp'}"
if(retained!=7) throw std::runtime_error("reimport lost a live variable");
''')
        assert "reused" in contextual.stdout.lower(), contextual.stdout

        ast = run(f'''%cpp options add "-I{include}"
%cpp generate "unavailable.hpp" --output "{root / 'external.hpp'}"
%cpp options add "-DGENERATION_DID_NOT_START_CPP=1"
''')
        assert "undefined symbol" not in ast.stderr.lower(), ast.stderr
        assert "app::External" in (root / "external.hpp").read_text()

        settings = f'%cpp options add "-I{include}" "-DAPP_ENTRY=7"\n'
        decode = '''tracer_repl::EventValue value{tracer_repl::EventValue::Object{{"ratio",tracer_repl::EventValue{2.5}}},"app::Other"};
if(value.as<app::Other>().ratio!=2.5 || app::entry!=7) throw std::runtime_error("bad generated conversion");
'''
        first = run(settings + '%cpp import "types.hpp"\n' + decode)
        saved = list(cache.rglob("*.hpp"))
        assert saved, first.stdout
        second = run(settings + '%cpp import "types.hpp"\n' + decode)
        assert "reused" in second.stdout.lower(), (second.stdout, second.stderr)
        dependency.write_text("#pragma once\nnamespace app { struct Leaf { int m=0; }; }\n")
        changed = run(settings + '%cpp import "types.hpp"\n' + decode)
        assert "reused" not in changed.stdout.lower(), changed.stdout
        assert any("::m)" in path.read_text() for path in cache.rglob("*.hpp"))

        early, later = root / "not-created-yet", root / "later"
        later.mkdir()
        (later / "shadow.hpp").write_text("#pragma once\nnamespace shadow { struct Model { int before=0; }; }\n")
        search_flags = f'%cpp options add "-I{early}" "-I{later}"\n'
        run(search_flags + '%cpp import "shadow.hpp"\n')
        early.mkdir()
        (early / "shadow.hpp").write_text("#pragma once\nnamespace shadow { struct Model { int after=0; }; }\n")
        shadowed = run(search_flags + '''%cpp import "shadow.hpp"
shadow::Model model;
model.after=5;
''')
        assert "reused" not in shadowed.stdout.lower(), shadowed.stdout
        hmap, mapped = root / "early.hmap", root / "mapped.hpp"
        hmap_flags = f'%cpp options add "-I{hmap}" "-I{later}"\n'
        run(hmap_flags + '%cpp import "shadow.hpp"\n')
        mapped.write_text("#pragma once\nnamespace shadow { struct Model { int new_value=0; }; }\n")
        key, value = b"shadow.hpp", str(mapped.resolve()).encode()
        hmap.write_bytes(struct.pack("<IHHIIII", 0x686d6170, 1, 0, 36, 1, 1, len(value)) +
                         struct.pack("<III", 1, len(key) + 2, 0) +
                         b"\0" + key + b"\0" + value + b"\0")
        remapped = run(hmap_flags + '''%cpp import "shadow.hpp"
shadow::Model model;
model.new_value=5;
''')
        assert "reused" not in remapped.stdout.lower(), remapped.stdout
        optional = include / "optional.hpp"
        optional.write_text('''#pragma once
namespace conditional { struct Model {
#if __has_include("feature.hpp")
bool enabled=false;
#else
int missing=0;
#endif
}; }
''')
        run(f'%cpp options add "-I{include}"\n%cpp import "optional.hpp"\n')
        (include / "feature.hpp").write_text("#pragma once\n")
        available = run(f'''%cpp options add "-I{include}"
%cpp import "optional.hpp"
conditional::Model model;
model.enabled=true;
''')
        assert "reused" not in available.stdout.lower(), available.stdout

        source = include / "nearby.cpp"
        source.write_text("int main() { return 0; }\n")
        db = root / "compile_commands.json"
        db.write_text(json.dumps([
            {"directory": str(root), "file": str(source),
             "arguments": ["never-execute-compiler", "-Iapp include", "-DAPP_ENTRY=2", "-c", str(source), "-o", "must-not-exist.o", "-MMD", "-MF", "must-not-exist.d"]},
            {"directory": str(root), "file": str(header),
             "arguments": ["never-execute-compiler", "-Iapp include", "-DAPP_ENTRY=1", "-x", "c++-header", "-c", str(header)]}]))
        exact = run(f'''%cpp options compdb "{db}" --file "{header}"
%cpp import "types.hpp"
if(app::entry!=1) throw std::runtime_error("exact database entry lost");
''')
        assert str(header) in exact.stdout
        missing_header = include / "without-entry.hpp"
        missing_header.write_text('#include "types.hpp"\n')
        fallback = run(f'''%cpp options compdb "{db}" --file "{missing_header}"
%cpp import "types.hpp"
if(app::entry!=2) throw std::runtime_error("same-directory entry lost");
''')
        assert str(source) in fallback.stdout
        assert not (root / "must-not-exist.o").exists()
        assert not (root / "must-not-exist.d").exists()
        failed = run(settings + f'''%cpp options add `printf bad >&2; exit 3`
%cpp options add `printf '%s' '$(touch unexpected-shell-execution)'`
%cpp options compdb "{db}" --file "{root / 'elsewhere' / 'missing.hpp'}"
%cpp import "types.hpp"
if(app::entry!=7) throw std::runtime_error("failed profile was committed");
std::cout << "profile-retained" << std::endl;
''', False)
        assert failed.returncode != 0
        assert "profile-retained" in failed.stdout, (failed.stdout, failed.stderr)
        assert not (root / "unexpected-shell-execution").exists()
        locked = run(settings + '''int retained=7;
%cpp options add `touch locked-profile-side-effect`
if(retained!=7) throw std::runtime_error("locked change lost state");
''', False)
        assert locked.returncode != 0
        assert not (root / "locked-profile-side-effect").exists()
        print("AST-only whole-header generation, dependency reuse, shell flags and compdb passed")


if __name__ == "__main__":
    main()
