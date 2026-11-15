"""Build an ELF index and inject a prepared observer without application headers."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from elf_split_fixture import link_split_fixture


HEADER = """namespace demo {
struct Packet { int value; int samples[2]; };
int score(const Packet&);
extern int exported_data;
}
"""

MODULE = """#include "packet.hpp"
namespace demo {
int exported_data = 3;
__attribute__((noinline)) int score(const Packet& packet) {
    return packet.value * 3 + packet.samples[1] + exported_data;
}
__attribute__((noinline)) int score(int value) { return value * 7; }
}
"""

TARGET = """#include "packet.hpp"
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif
int main() {
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) return 1;
    demo::Packet packet{12, {6, 7}};
    std::puts("ready");
    std::fflush(stdout);
    for (;;) {
        if (demo::score(packet) != 46) return 2;
        usleep(10000);
    }
}
"""

CALLBACK = """template <> class Interceptor<int(const demo::Packet&)> {
public:
    long long pre(const Call& call) {
        const auto& packet = call.argument<0>();
        const int decoded = tracer_recovered::demo::PacketView::value(packet) * 10
                          + tracer_recovered::demo::PacketView::samples<1>(packet);
        call.field("recovered", decoded);
        return decoded;
    }
    long long post(const Call& call) { return call.result<int>(); }
};
"""

SELECTOR = "demo::score(demo::Packet const&)"


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "module-tool", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = str(Path(args.repl).with_name("clang-tracer-prepare-injectors-native"))
    env["TRACER_MODULE_EXECUTABLE"] = args.module_tool

    with tempfile.TemporaryDirectory(prefix="tracer-module-index-") as path:
        root = Path(path)

        def run(argv, *, script=None, expected_error=None):
            result = subprocess.run(argv, input=script, cwd=root, env=env,
                                    capture_output=True, text=True, timeout=120)
            transcript = result.stdout + result.stderr
            if expected_error is None:
                assert result.returncode == 0, transcript
            else:
                assert result.returncode and expected_error in transcript, transcript
            return result.stdout

        def repl(script, *options, expected_error=None):
            return run([args.repl, "--fail-fast", *options], script=script,
                       expected_error=expected_error)

        (root / "packet.hpp").write_text(HEADER)
        (root / "module.cpp").write_text(MODULE)
        (root / "target.cpp").write_text(TARGET)
        flags = ["-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer", "-fcf-protection=none"]
        link_split_fixture(args.compiler, "module.cpp", root, "libdemo.so", "libdemo.debug",
                           [*flags, "-fPIC"], ["-shared"])
        run([args.compiler, *flags, "target.cpp", "-L.", "-ldemo", "-Wl,-rpath,$ORIGIN", "-o", "target"])
        for source in ("packet.hpp", "module.cpp", "target.cpp"):
            (root / source).unlink()

        repl("%module index --file ./libdemo.so --debug-file ./target --output wrong.sqlite\n",
             expected_error="debug_file_mismatch")
        assert not (root / "wrong.sqlite").exists()

        listing = repl(f"""%module index --file ./libdemo.so
%module types --filter 'demo::*' --format json
%module symbols --exported --filter 'demo::*' --format json
%module show --type demo::Packet
%module show --symbol "{SELECTOR}"
%module emit --symbol "{SELECTOR}" --output recovered
%quit
""")
        rows = [json.loads(line) for line in listing.splitlines() if line.startswith("{")]
        types, symbols, packet, score = rows
        assert any(entry["name"] == "demo::Packet" for entry in types["entries"]), rows
        assert any(entry["name"] == "demo::exported_data" for entry in symbols["entries"]), rows
        assert packet["size"] == 12 and packet["fields"][1]["byte_offset"] == 4, packet
        assert score["has_debug"] and score["exported"] and len(score["parameters"]) == 1, score
        assert (root / "recovered/declarations.hpp").is_file()
        assert "samples" in (root / "recovered/views.hpp").read_text()
        page = json.loads(run([args.module_tool, "symbols", "--index", "libdemo.so.tracer.sqlite",
                               "--filter", "demo::*", "--exported", "--limit", "1"]))
        assert len(page["entries"]) == 1 and page["next_page"], page
        next_page = json.loads(run([args.module_tool, "symbols", "--index", "libdemo.so.tracer.sqlite",
                                    "--filter", "demo::*", "--exported", "--page", page["next_page"]]))
        assert score["id"] in {entry["id"] for entry in next_page["entries"]}, next_page

        repl(f"""%module use libdemo.so.tracer.sqlite
%module show --symbol demo::score
""", expected_error="ambiguous_symbol")
        (root / "libdemo.debug").unlink()
        repl(f'%probe prepare "{SELECTOR}" --module ./target --module-index libdemo.so.tracer.sqlite --output wrong\n',
             expected_error="module_identity_mismatch")
        repl(f'%probe prepare "{SELECTOR}" --module ./libdemo.so --generated recovered --output prepared\n')

        repl(f"""auto index = tracer.open_module_index("libdemo.so.tracer.sqlite");
auto score = index.find_symbol("{SELECTOR}");
if (!score.has_debug) throw std::runtime_error("missing DWARF");
index.prepare_probe(score.id, {{}}, "from-cpp", "recovered");
%quit
""")
        assert (root / "from-cpp/prepared.pb").is_file()

        changed = root / "recovered/views.hpp"
        changed.write_text(changed.read_text() + "\n// changed\n")
        repl(f'%probe prepare "{SELECTOR}" --module ./libdemo.so --generated recovered --output edited\n',
             expected_error="generated bundle differs")
        (root / "libdemo.so.tracer.sqlite").unlink()
        shutil.rmtree(root / "recovered")
        (root / "observer.cpp").write_text(CALLBACK)
        with subprocess.Popen([root / "target"], cwd=root, stdout=subprocess.PIPE, text=True) as target:
            try:
                assert target.stdout.readline().strip() == "ready"
                transcript = repl(f"""%probe validate "{SELECTOR}" --callback observer.cpp --prepared prepared --prepared-mode require
%probe inject "{SELECTOR}" --callback observer.cpp --prepared prepared --prepared-mode require
%collect --count 1 --timeout 20 --phase PRE --as before
%collect --count 1 --timeout 20 --phase POST --as after
%events list before
%events list after
%detach
%quit
""", "--server", args.server, "--agent", args.agent, "--attach", str(target.pid), "--timeout", "60")
                assert "installed probe=" in transcript, transcript
                assert "recovered=127" in transcript and "result=46" in transcript, transcript
                assert target.poll() is None, "target stopped during tracing"
            finally:
                if target.poll() is None:
                    target.terminate()


if __name__ == "__main__":
    main()
