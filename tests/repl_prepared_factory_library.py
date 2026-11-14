"""Verify live prepared decoding with a destructor from a matched application DSO."""

import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time


HEADER = r'''#pragma once
#include <string>
#include <utility>

namespace ngd {
class HeapObject {
public:
  explicit HeapObject(int base) : base_id(base) {}
  virtual ~HeapObject();

protected:
  const int base_id;
};
}

namespace HFW {
class HfwItem : public ngd::HeapObject {
public:
  HfwItem(int base, std::string text) : HeapObject(base), label(std::move(text)) {}
  int revision;

private:
  std::string label;
};

HfwItem *createItem(int enabled);
HfwItem &referenceItem(int revision);
}
'''

LIBRARY = r'''#include "item.hpp"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

ngd::HeapObject::~HeapObject() {
  if (const auto *marker = std::getenv("TRACER_FIXTURE_DESTRUCTION_LOG")) {
    if (auto *file = std::fopen(marker, "a")) {
      std::fprintf(file, "%ld %d\n", static_cast<long>(::getpid()), base_id);
      std::fclose(file);
    }
  }
}
'''

TARGET = r'''#include "item.hpp"
#include <cstdio>
#include <sys/prctl.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY (~0UL)
#endif
#include <unistd.h>

namespace {
HFW::HfwItem item{53, "library-backed HfwItem"};
}

__attribute__((noinline)) HFW::HfwItem *HFW::createItem(int enabled) {
  return enabled ? &item : nullptr;
}
__attribute__((noinline)) HFW::HfwItem &HFW::referenceItem(int revision) {
  item.revision = revision;
  return item;
}

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0) != 0)
    return 1;
  item.revision = 827;
  std::puts("ready");
  std::fflush(stdout);
  for (;;) {
    HFW::createItem(1);
    HFW::referenceItem(827);
    ::usleep(100000);
  }
}
'''


def build(compiler, root):
    (root / "item.hpp").write_text(HEADER)
    (root / "heap_object.cpp").write_text(LIBRARY)
    (root / "target.cpp").write_text(TARGET)
    flags = [str(compiler), "-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer",
             "-fcf-protection=none"]
    subprocess.run([*flags, "-shared", "-fPIC", "-Wl,--build-id=sha1", "heap_object.cpp",
                    "-o", "libfactory_heap_object.so"], cwd=root, check=True, timeout=45)
    subprocess.run([*flags, "target.cpp", "-L.", "-lfactory_heap_object", "-Wl,-rpath,$ORIGIN",
                    "-o", "target"], cwd=root, check=True, timeout=45)
    subprocess.run([*flags, "-fvisibility=hidden", "target.cpp", "heap_object.cpp",
                    "-o", "target_hidden"], cwd=root, check=True, timeout=45)
    (root / "compile_commands.json").write_text(json.dumps([
        {"directory": str(root), "file": str(root / "target.cpp"),
         "arguments": [*flags, "-c", str(root / "target.cpp")]},
    ]))


def start_external_server(args, root, env):
    socket = root / "external.sock"
    server = subprocess.Popen(
        [args.server, "--listen", f"unix://{socket}", "--agent", args.agent, "--timeout", "60"],
        cwd=root, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.monotonic() + 10
    while not socket.exists():
        assert server.poll() is None, "external server exited"
        assert time.monotonic() < deadline, "external server did not listen"
        time.sleep(0.05)
    return server, f"unix://{socket}"


def check_return(args, root, target, env, kind, selector, inject_options, expect_factory,
                 endpoint=None):
    marker = root / f"destruction-{kind}.log"
    console_env = {**env, "TRACER_FIXTURE_DESTRUCTION_LOG": str(marker)}
    source = f'''%probe prepare "{selector}" --include item.hpp --output prepared_{kind}
%probe inject "{selector}" --prepared prepared_{kind} --prepared-mode require{inject_options}
%collect --name "{selector}" --count 4 --timeout 15 --phase POST --as captured
static_assert(!std::is_default_constructible_v<HFW::HfwItem>);
auto captured = tracer.batch("captured");
if (captured.size() != 4) throw std::runtime_error("expected four POST events");
auto event = captured.at(0);
auto snapshot = event.result_snapshot();
if (snapshot.field("base_id").as<int>() != 53 || snapshot.field("label").as<std::string>() != "library-backed HfwItem" || snapshot.field("revision").as<int>() != 827) throw std::runtime_error("wrong snapshot fields");
'''
    if expect_factory:
        source += '''void verify_item(const tracer_repl::Event& event) { auto item = event.result_as<HFW::HfwItem>(); if (item.base_id != 53 || item.label != "library-backed HfwItem" || item.revision != 827) throw std::runtime_error("wrong typed fields"); }
verify_item(event);
'''
    else:
        # Without a usable destructor library the record is rebuilt field by field.
        source += '''void verify_reconstructed(const tracer_repl::Event& event) { auto item = event.result_as<HFW::HfwItem>(); if (item->base_id != 53 || item->label != "library-backed HfwItem" || item->revision != 827) throw std::runtime_error("wrong reconstructed fields"); }
verify_reconstructed(event);
'''
    success = f"FACTORY_LIBRARY_{kind.upper()}=1"
    source += f'''std::cout << "{success}" << std::endl;
%quit
'''
    connection = (["--endpoint", endpoint] if endpoint
                  else ["--server", args.server, "--agent", args.agent])
    with (root / f"{kind}.log").open("w+") as transcript:
        console = subprocess.Popen(
            [args.repl, *connection, "--attach", str(target.pid), "--timeout", "60",
             "--fail-fast"],
            cwd=root, env=console_env, stdin=subprocess.PIPE, stdout=transcript,
            stderr=subprocess.STDOUT, text=True,
        )
        try:
            console.communicate(source, timeout=75)
        finally:
            if console.poll() is None:
                console.kill()
                console.wait()
        transcript.seek(0)
        output = transcript.read()
    assert console.returncode == 0, output
    assert success in output.splitlines(), output
    assert target.poll() is None, output
    if expect_factory:
        destructor_calls = marker.read_text().splitlines() if marker.exists() else []
        assert destructor_calls and set(destructor_calls) == {f"{console.pid} 53"}, (
            destructor_calls, output,
        )
    else:
        assert any("reconstructed HFW::HfwItem" in line for line in output.splitlines()), output
        assert not any("skipped HFW::HfwItem" in line for line in output.splitlines()), output
        assert not marker.exists(), output
    print(success)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "preparer", "compiler"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    env = {key: value for key, value in os.environ.items()
           if key not in {"CLANG_TRACER_ENDPOINT", "TRACER_FIXTURE_DESTRUCTION_LOG"}}
    env["TRACER_PREPARE_EXECUTABLE"] = str(args.preparer)
    with tempfile.TemporaryDirectory(prefix="ct-factory-library-") as directory:
        root = Path(directory)
        env["XDG_CACHE_HOME"] = str(root / "cache")
        build(args.compiler, root)
        for binary, kind, selector, inject_options, expect_factory in (
            ("target", "pointer", "HFW::createItem(int)", " --snapshot-return-pointee", True),
            ("target", "reference", "HFW::referenceItem(int)", "", True),
            ("target_hidden", "hidden", "HFW::createItem(int)", " --snapshot-return-pointee", False),
            ("target", "external", "HFW::referenceItem(int)", "", True),
        ):
            with subprocess.Popen([root / binary], cwd=root, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True) as target:
                try:
                    assert select.select([target.stdout], [], [], 10)[0], "target did not start"
                    assert target.stdout.readline().strip() == "ready", "target failed to start"
                    if kind == "external":
                        server, endpoint = start_external_server(args, root, env)
                        try:
                            check_return(args, root, target, env, kind, selector,
                                         inject_options, expect_factory, endpoint)
                        finally:
                            if server.poll() is None:
                                server.terminate()
                                try:
                                    server.wait(timeout=10)
                                except subprocess.TimeoutExpired:
                                    server.kill()
                                    server.wait()
                    else:
                        check_return(args, root, target, env, kind, selector, inject_options,
                                     expect_factory)
                finally:
                    if target.poll() is None:
                        target.terminate()
                        target.wait(timeout=5)


if __name__ == "__main__":
    main()
