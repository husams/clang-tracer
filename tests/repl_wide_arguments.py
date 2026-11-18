"""Observe callables with many arguments through every resolution path.

The argument limit is the build option TRACER_MAX_ARGUMENTS (default 16). The fixture is
generated from the limit given on the command line, so one test covers a default build and a
build configured with another value: a callable with exactly the limit is observed with every
value, one with a single argument more is rejected, and the message names the option.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from elf_split_fixture import link_split_fixture


SHARED_CALLBACK = """struct Scope {};
struct ReviewState : shared_state<Scope> { int calls = 0; };
struct Interceptor {
  Interceptor(State<ReviewState>);
  void pre(const Call& call, ReviewState& state) { ++state.calls; }
  void post(const Call& call, ReviewState& state) {
    call.field("review", 7);
    call.context().event("review.step");
  }
};
"""


def parameters(count, type_name="int", prefix="a"):
    return ", ".join(f"{type_name} {prefix}{i}" for i in range(1, count + 1))


def names(count, prefix="a"):
    return [f"{prefix}{i}" for i in range(1, count + 1)]


def fixture_sources(limit):
    over = limit + 1
    # Eight integers (two of them are passed on the stack) followed by doubles interleaved with
    # integers: the stack carries integer and floating point arguments in declaration order.
    pattern = ["int"] * 8 + ["double", "int", "double", "int", "double", "int", "double", "int"]
    mixed = pattern[:min(limit, len(pattern))]
    mixed_parameters = ", ".join(f"{t} m{i}" for i, t in enumerate(mixed, 1))
    header = f"""#pragma once
namespace wide {{
int sum_limit({parameters(limit)});
int sum_over({parameters(over)});
int sum_nine({parameters(9)});
double mixed({mixed_parameters});
struct Holder {{
  int base;
  int method_nine({parameters(9)}) const;
  int method_limit({parameters(limit - 1)}) const;
  int method_over({parameters(over)}) const;
}};
}}
"""
    source = f"""#include "wide.hpp"
namespace wide {{
int sum_limit({parameters(limit)}) {{ return {" + ".join(names(limit))}; }}
int sum_over({parameters(over)}) {{ return {" + ".join(names(over))}; }}
int sum_nine({parameters(9)}) {{ return {" + ".join(names(9))}; }}
double mixed({mixed_parameters}) {{ return {" + ".join(names(len(mixed), "m"))}; }}
int Holder::method_nine({parameters(9)}) const {{ return base + {" + ".join(names(9))}; }}
int Holder::method_over({parameters(over)}) const {{ return base + {" + ".join(names(over))}; }}
int Holder::method_limit({parameters(limit - 1)}) const {{ return base + {" + ".join(names(limit - 1))}; }}
}}
"""
    leading = lambda count: "".join(f"{i}, " for i in range(1, count))
    mixed_values = ", ".join(f"{i}.5" if t == "double" else str(i)
                             for i, t in enumerate(mixed, 1))
    target = f"""#include "wide.hpp"
#include <cstdio>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif
int main() {{
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) return 1;
  wide::Holder holder{{1000}};
  std::puts("ready");
  std::fflush(stdout);
  for (int n = 100;; n = n % 900 + 100) {{
    wide::sum_limit({leading(limit)}n);
    wide::sum_over({leading(over)}n);
    wide::sum_nine({leading(9)}n);
    wide::mixed({mixed_values});
    holder.method_nine({leading(9)}n);
    holder.method_limit({leading(limit - 1)}n);
    holder.method_over({leading(over)}n);
    usleep(100000);
  }}
}}
"""
    return header, source, target, mixed


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "module-tool", "preparer", "compiler"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--max-arguments", type=int, required=True)
    args = parser.parse_args()
    limit = args.max_arguments
    assert 8 <= limit <= 64, limit
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = args.preparer
    env["TRACER_MODULE_EXECUTABLE"] = args.module_tool
    header, source, target_source, mixed = fixture_sources(limit)
    ints = lambda count: ", ".join(["int"] * count)
    limit_selector = f"wide::sum_limit({ints(limit)})"
    over_selector = f"wide::sum_over({ints(limit + 1)})"
    nine_selector = f"wide::sum_nine({ints(9)})"
    mixed_selector = f"wide::mixed({', '.join(mixed)})"
    method_selector = f"wide::Holder::method_nine({ints(9)}) const"
    method_over_selector = f"wide::Holder::method_over({ints(limit + 1)}) const"
    method_limit_selector = f"wide::Holder::method_limit({ints(limit - 1)}) const"

    with tempfile.TemporaryDirectory(prefix="ct-wide-") as path:
        root = Path(path)

        def run(argv, script=None, expected_error=None):
            result = subprocess.run(argv, input=script, cwd=root, env=env,
                                    capture_output=True, text=True, timeout=240)
            transcript = result.stdout + result.stderr
            if expected_error is None:
                assert result.returncode == 0, transcript
            else:
                assert result.returncode and expected_error in transcript, transcript
            return transcript

        def repl(script, *options, expected_error=None):
            return run([args.repl, "--fail-fast", *options], script, expected_error)

        (root / "wide.hpp").write_text(header)
        (root / "wide.cpp").write_text(source)
        (root / "target.cpp").write_text(target_source)
        flags = ["-std=c++17", "-g", "-O0", "-fno-inline", "-fno-omit-frame-pointer",
                 "-fcf-protection=none"]
        (root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(root), "file": "wide.cpp",
            "arguments": [args.compiler, *flags, "-c", "wide.cpp"]}]))
        context = " --compdb compile_commands.json --translation-unit wide.cpp"
        run([args.compiler, *flags, "wide.cpp", "target.cpp", "-o", "target"])
        link_split_fixture(args.compiler, "wide.cpp", root, "libwide.so", "libwide.debug",
                           [*flags, "-fPIC"], ["-shared"])
        run([args.compiler, *flags, "target.cpp", "-L.", "-lwide", "-Wl,-rpath,$ORIGIN",
             "-o", "target-lib"])

        def attached(executable, script, expected_error=None):
            """Run a REPL script attached to a fresh copy of the target."""
            with subprocess.Popen([root / executable], cwd=root, stdout=subprocess.PIPE,
                                  text=True) as process:
                try:
                    assert process.stdout.readline().strip() == "ready"
                    transcript = run([args.repl, "--server", args.server, "--agent", args.agent,
                                      "--attach", str(process.pid), "--timeout", "60"], script, expected_error)
                    assert process.poll() is None, "the target stopped while it was traced"
                    return transcript
                finally:
                    if process.poll() is None:
                        process.terminate()

        def observe(executable, probes, per_probe=60):
            """Inject every probe, collect POST events and return their text lines."""
            inject = "".join(f'%probe inject "{selector}"{options}\n' for selector, options in probes)
            transcript = attached(executable, f"""{inject}%collect --count {per_probe * len(probes)} --timeout 30 --phase POST --as post
%events list post
%detach
%quit
""")
            assert transcript.count("installed probe=") == len(probes), transcript
            assert "lost(agent=0,server=0)" in transcript, transcript
            return [line for line in transcript.splitlines() if " phase=POST " in line]

        def event(lines, function):
            matching = [line for line in lines if f" function={function}(" in line]
            assert matching, (function, len(lines), sorted({re.search(r' function=(\S+?)\(', l).group(1) for l in lines}))
            return matching[0]

        def arguments(line):
            text = re.search(r"args=\[(.*?)\] result=(\S+)", line)
            assert text, line
            return dict(item.split("=") for item in text.group(1).split(", ")), text.group(2)

        def check_integers(line, count, base=0):
            values, result = arguments(line)
            if "receiver" in values:  # a method prepared from ELF symbols reports its receiver
                del values["receiver"]
            # Debug information names the arguments; a prepared package without parameter
            # names reports arg0, arg1, ... (the fallback names reach arg15 and beyond).
            keys = list(values)
            assert keys in (names(count), [f"arg{i}" for i in range(count)]), line
            numbers = [int(values[key]) for key in keys]
            assert numbers[:-1] == list(range(1, count)), line
            last = numbers[-1]
            assert 100 <= last < 1000, line
            assert int(result) == base + sum(range(1, count)) + last, line
            return values

        def check_mixed(line):
            values, result = arguments(line)
            assert list(values) == names(len(mixed), "m"), line
            expected = 0.0
            for i, kind in enumerate(mixed, 1):
                value = i + 0.5 if kind == "double" else i
                assert float(values[f"m{i}"]) == value, (i, line)
                expected += value
            assert float(result) == expected, line

        # 1. Debug information only, no prepared package. The nine argument method has a
        # receiver and the mixed callable puts integers and doubles on the stack.
        # The nine argument callables fit only when the build allows at least nine arguments;
        # the supported minimum of eight must reject them instead.
        nine_fits = limit >= 9
        probes = [(limit_selector, ""), (mixed_selector, ""), (method_limit_selector, "")]
        if nine_fits:
            probes += [(nine_selector, ""), (method_selector, "")]
        lines = observe("target", probes)
        check_integers(event(lines, "wide::sum_limit"), limit)
        check_mixed(event(lines, "wide::mixed"))
        check_integers(event(lines, "wide::Holder::method_limit"), limit - 1, base=1000)
        if nine_fits:
            check_integers(event(lines, "wide::sum_nine"), 9)
            check_integers(event(lines, "wide::Holder::method_nine"), 9, base=1000)

        # 2. One argument more than the limit is refused and the message names the option.
        too_wide = [(over_selector, limit), (method_over_selector, limit)]
        if not nine_fits:
            too_wide += [(nine_selector, limit), (method_selector, limit)]
        for selector, allowed in too_wide:
            refused = attached("target", f"""%probe inject "{selector}"
%detach
%quit
""", expected_error="TRACER_MAX_ARGUMENTS")
            assert "installed probe=" not in refused, refused
            assert re.search(r"error: .*TRACER_MAX_ARGUMENTS", refused), refused
            assert f"more than {allowed} arguments" in refused, refused

        # 3. Prepared package built from the header (source based resolution).
        repl(f'%probe prepare "{limit_selector}" --include wide.hpp{context} --output prepared-limit\n')
        if nine_fits:
            repl(f'%probe prepare "{nine_selector}" --include wide.hpp{context} --output prepared-nine\n')
        else:
            repl(f'%probe prepare "{nine_selector}" --include wide.hpp{context} --output prepared-nine\n',
                 expected_error="TRACER_MAX_ARGUMENTS")
        repl(f'%probe prepare "{over_selector}" --include wide.hpp{context} --output prepared-over\n',
             expected_error="TRACER_MAX_ARGUMENTS")
        assert not (root / "prepared-over").exists()
        prepared = [(limit_selector, " --prepared prepared-limit --prepared-mode require")]
        if nine_fits:
            prepared.append((nine_selector, " --prepared prepared-nine --prepared-mode require"))
        lines = observe("target", prepared)
        check_integers(event(lines, "wide::sum_limit"), limit)
        if nine_fits:
            check_integers(event(lines, "wide::sum_nine"), 9)

        # 4. Module index: ELF symbols with split debug information and no application headers.
        repl(f"""%module index --file ./libwide.so
%module emit --symbol "{limit_selector}" --output recovered
%module emit --symbol "{method_limit_selector}" --output recovered-method
%module emit --symbol "{over_selector}" --output recovered-over
%quit
""")
        repl(f'%probe prepare "{limit_selector}" --module ./libwide.so --generated recovered '
             '--output prepared-module\n')
        lines = observe("target-lib", [
            (limit_selector, " --prepared prepared-module --prepared-mode require")])
        check_integers(event(lines, "wide::sum_limit"), limit)
        repl(f'%probe prepare "{method_limit_selector}" --module ./libwide.so --generated '
             'recovered-method --output prepared-method\n')
        lines = observe("target-lib", [
            (method_limit_selector, " --prepared prepared-method --prepared-mode require")])
        check_integers(event(lines, "wide::Holder::method_limit"), limit - 1, base=1000)
        repl(f'%probe prepare "{over_selector}" --module ./libwide.so --generated recovered-over '
             '--output prepared-module-over\n', expected_error="TRACER_MAX_ARGUMENTS")
        # 5. A shared-context callback is compiled against the on-disk headers: it must agree
        # with the tracer on the argument limit, or the fields it records are lost.
        (root / "shared.cpp").write_text(SHARED_CALLBACK)
        shared = attached("target", f"""%probe inject "{limit_selector}" --callback shared.cpp --print-arguments
%collect --count 4 --timeout 30 --phase POST --as post
%events list post
%detach
%quit
""")
        events = [line for line in shared.splitlines() if " phase=POST " in line]
        assert len(events) == 4, shared
        for line in events:
            assert f"a{limit}=" in line and line.count("review=7") == 1, line
        print(f"ok limit={limit}")


if __name__ == "__main__":
    main()
