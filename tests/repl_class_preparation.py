"""Prepare, install and observe callables whose records are passed or returned by value.

The callables are selected by ELF symbol (explicit signature) or by index entry, and their
types come from a supplied type context: a header that only defines the types. The target is
checked against its own untraced behaviour, including the copies and destructions of a
nontrivial class, so a probe that perturbs a by-value object makes it exit.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

TYPES = """#pragma once
#include <string>
namespace extra {
struct Plain { int value; };
struct Mix16 { long id; double scale; };
struct Duo { double scale; double bias; };
struct Large { long a, b, c, d, e; };
struct Tracked {
    int value;
    explicit Tracked(int v);
    Tracked(const Tracked& other);
    ~Tracked();
};
template <class T> struct Box { T item; int tag; };
struct Outer { struct Inner { int x; int y; }; };
}
"""

API = """#pragma once
#include "types.hpp"
namespace extra {
Plain make_plain(int n);
int take_plain(Plain value);
Mix16 make_mix16(long n);
double take_mix16(Mix16 value);
Duo make_duo(double n);
double take_duo(Duo value);
Large make_large(long n);
long take_large(Large value);
Tracked make_tracked(int n);
int take_tracked(Tracked value);
std::string make_label(int n);
int count_label(std::string value);
Box<Plain> make_box(int n);
int take_box(Box<Plain> value);
Outer::Inner make_inner(int n);
int take_inner(Outer::Inner value);
int read_plain(const Plain* value);
struct Opaque;
Plain make_with_ctx(Opaque* ctx);
__attribute__((abi_tag("v1"))) int tagged(int n);
__attribute__((abi_tag("a", "b"))) int multi(int n);
int tracked_live();
int tracked_copies();
}
"""

# Functions whose mangled names are written out, to give one stripped selector several tagged
# contracts. They are defined with asm labels so the compiler accepts what a header never could.
LOOKUP_NAMES = {
    "over_plain": "_ZN5extra4overEi",
    "over_tagged": "_ZN5extra4overB1zEi",
    "dup_x": "_ZN5extra3dupB1xEi",
    "ov_int": "_ZN5extra2ovB1tEi",
    "ov_long": "_ZN5extra2ovB1tEl",
    "cv_const": "_ZNK5extra2Cv3getB1tEv",
    "cv_plain": "_ZN5extra2Cv3getB1tEv",
    # Linkage names sort after every filler below, so a scan that stopped at the first page of
    # tagged symbols would see neither of them.
    "amb_first": "_ZN2pg7zzzzambB1pEi",
    "amb_last": "_ZN2pg7zzzzambB1qEi",
}
FILLERS = 300


def lookup_source():
    lines = []
    for name, label in LOOKUP_NAMES.items():
        if name == "cv_const" or name == "cv_plain":
            lines.append(f'extern "C" int {name}(void*) __asm__("{label}");')
        elif name == "ov_long":
            lines.append(f'extern "C" int {name}(long) __asm__("{label}");')
        else:
            lines.append(f'extern "C" int {name}(int) __asm__("{label}");')
    lines.append('extern "C" int dup_y(int) __asm__("_ZN5extra3dupB1yEi") '
                 '__attribute__((alias("_ZN5extra3dupB1xEi")));')
    for name in LOOKUP_NAMES:
        parameter = "void*" if name.startswith("cv_") else "long" if name == "ov_long" else "int"
        lines.append(f'extern "C" __attribute__((noinline)) int {name}({parameter}) {{ return 1; }}')
    for index in range(FILLERS):
        word = f"fill{index}"
        label = f"_ZN2pg{len(word)}{word}B1tEi"
        lines.append(f'extern "C" __attribute__((noinline)) int {word}(int) __asm__("{label}");')
        lines.append(f'extern "C" int {word}(int n) {{ return n; }}')
    return "\n".join(lines) + "\n"


MODULE = """#include "api.hpp"
#include <string>
namespace extra {
namespace { int live = 0, copies = 0; }
Tracked::Tracked(int v) : value(v) { ++live; }
Tracked::Tracked(const Tracked& other) : value(other.value) { ++live; ++copies; }
Tracked::~Tracked() { --live; }
int tracked_live() { return live; }
int tracked_copies() { return copies; }
__attribute__((noinline)) Plain make_plain(int n) { return {n}; }
__attribute__((noinline)) int take_plain(Plain value) { return value.value + 1; }
__attribute__((noinline)) Mix16 make_mix16(long n) { return {n, 1.5}; }
__attribute__((noinline)) double take_mix16(Mix16 value) { return value.id * value.scale; }
__attribute__((noinline)) Duo make_duo(double n) { return {n, n + 0.5}; }
__attribute__((noinline)) double take_duo(Duo value) { return value.scale + value.bias * 2; }
__attribute__((noinline)) Large make_large(long n) { return {n, n + 1, n + 2, n + 3, n + 4}; }
__attribute__((noinline)) long take_large(Large value) {
    return value.a + value.b + value.c + value.d + value.e;
}
__attribute__((noinline)) Tracked make_tracked(int n) { return Tracked(n); }
__attribute__((noinline)) int take_tracked(Tracked value) { return value.value + 1; }
__attribute__((noinline)) std::string make_label(int n) { return "label" + std::to_string(n); }
__attribute__((noinline)) int count_label(std::string value) { return static_cast<int>(value.size()); }
__attribute__((noinline)) Box<Plain> make_box(int n) { return {{n}, n + 1}; }
__attribute__((noinline)) int take_box(Box<Plain> value) { return value.item.value + value.tag; }
__attribute__((noinline)) Outer::Inner make_inner(int n) { return {n, n * 2}; }
__attribute__((noinline)) int take_inner(Outer::Inner value) { return value.x + value.y; }
__attribute__((noinline)) int read_plain(const Plain* value) { return value->value; }
struct Opaque { int n; };
__attribute__((noinline)) Plain make_with_ctx(Opaque* ctx) { return {ctx->n}; }
__attribute__((noinline)) int tagged(int n) { return n + 2; }
__attribute__((noinline)) int multi(int n) { return n + 3; }
}
""" + lookup_source()

TARGET = """#include "api.hpp"
#include <cstdio>
#include <string>
#include <sys/prctl.h>
#include <unistd.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif
int main() {
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) return 1;
    std::puts("ready");
    std::fflush(stdout);
    for (int tick = 0;; ++tick) {
        const int copies = extra::tracked_copies();
        extra::Plain plain = extra::make_plain(9);
        if (plain.value != 9 || extra::take_plain(plain) != 10 || extra::read_plain(&plain) != 9)
            return 2;
        extra::Mix16 mix = extra::make_mix16(4);
        if (mix.id != 4 || mix.scale != 1.5 || extra::take_mix16(mix) != 6.0) return 3;
        extra::Duo duo = extra::make_duo(2.0);
        if (duo.scale != 2.0 || duo.bias != 2.5 || extra::take_duo(duo) != 7.0) return 4;
        extra::Large large = extra::make_large(5);
        if (large.a != 5 || large.e != 9 || extra::take_large(large) != 35) return 5;
        {
            extra::Tracked tracked = extra::make_tracked(4);
            if (tracked.value != 4 || extra::take_tracked(tracked) != 5) return 6;
            if (extra::tracked_live() != 1) return 7;
        }
        // One copy per by-value call, no copy for the returned prvalue, nothing left alive.
        if (extra::tracked_live() != 0 || extra::tracked_copies() - copies != 1) return 8;
        extra::Box<extra::Plain> box = extra::make_box(2);
        if (box.item.value != 2 || box.tag != 3 || extra::take_box(box) != 5) return 11;
        extra::Outer::Inner inner = extra::make_inner(3);
        if (inner.x != 3 || inner.y != 6 || extra::take_inner(inner) != 9) return 12;
        std::string label = extra::make_label(1);
        if (label != "label1" || extra::count_label(label) != 6) return 9;
        if (extra::tagged(3) != 5 || extra::multi(3) != 6) return 10;
        usleep(10000);
    }
}
"""

ROUTES = {
    # name: (selector, signature or None)
    "make_plain": ("extra::make_plain(int)", "extra::Plain(int)"),
    "take_plain": ("extra::take_plain(extra::Plain)", "int(extra::Plain)"),
    "make_mix16": ("extra::make_mix16(long)", "extra::Mix16(long)"),
    "take_mix16": ("extra::take_mix16(extra::Mix16)", "double(extra::Mix16)"),
    "make_duo": ("extra::make_duo(double)", "extra::Duo(double)"),
    "take_duo": ("extra::take_duo(extra::Duo)", "double(extra::Duo)"),
    "make_large": ("extra::make_large(long)", "extra::Large(long)"),
    "take_large": ("extra::take_large(extra::Large)", "long(extra::Large)"),
    "make_tracked": ("extra::make_tracked(int)", "extra::Tracked(int)"),
    "take_tracked": ("extra::take_tracked(extra::Tracked)", "int(extra::Tracked)"),
    "make_box": ("extra::make_box(int)", "extra::Box<extra::Plain>(int)"),
    "take_box": ("extra::take_box(extra::Box<extra::Plain>)", "int(extra::Box<extra::Plain>)"),
    "make_inner": ("extra::make_inner(int)", "extra::Outer::Inner(int)"),
    "take_inner": ("extra::take_inner(extra::Outer::Inner)", "int(extra::Outer::Inner)"),
    "make_label": ("extra::make_label(int)", "std::string(int)"),
    # A selector is the demangled name, so a std::string parameter is spelled in full.
    "count_label": ("extra::count_label(std::__cxx11::basic_string<char, std::char_traits<char>, "
                    "std::allocator<char> >)", "int(std::string)"),
}

# What each observed call must show, checked on both PRE-independent POST events.
BODIES = {
    "make_plain": 'if (e.result_snapshot().field("value").as<int>() != 9) throw std::runtime_error("make_plain");',
    "take_plain": 'if (e.result_as<int>() != 10 || e.argument_snapshot(0).field("value").as<int>() != 9) '
                  'throw std::runtime_error("take_plain");',
    "make_mix16": 'const auto r = e.result_snapshot(); if (r.field("id").as<long long>() != 4 || '
                  'r.field("scale").as<double>() != 1.5) throw std::runtime_error("make_mix16");',
    "take_mix16": 'const auto a = e.argument_snapshot(0); if (e.result_as<double>() != 6.0 || '
                  'a.field("id").as<long long>() != 4) throw std::runtime_error("take_mix16");',
    "make_duo": 'const auto r = e.result_snapshot(); if (r.field("scale").as<double>() != 2.0 || '
                'r.field("bias").as<double>() != 2.5) throw std::runtime_error("make_duo");',
    "take_duo": 'const auto a = e.argument_snapshot(0); if (e.result_as<double>() != 7.0 || '
                'a.field("bias").as<double>() != 2.5) throw std::runtime_error("take_duo");',
    "make_large": 'const auto r = e.result_snapshot(); if (r.field("a").as<long long>() != 5 || '
                  'r.field("e").as<long long>() != 9) throw std::runtime_error("make_large");',
    "take_large": 'const auto a = e.argument_snapshot(0); if (e.result_as<long long>() != 35 || '
                  'a.field("c").as<long long>() != 7) throw std::runtime_error("take_large");',
    "make_tracked": 'if (e.result_snapshot().field("value").as<int>() != 4) throw std::runtime_error("make_tracked");',
    "take_tracked": 'if (e.result_as<int>() != 5 || e.argument_snapshot(0).field("value").as<int>() != 4) '
                    'throw std::runtime_error("take_tracked");',
    "make_box": 'if (e.result_snapshot().field("tag").as<int>() != 3) throw std::runtime_error("make_box");',
    "take_box": 'if (e.result_as<int>() != 5 || e.argument_snapshot(0).field("tag").as<int>() != 3) '
                'throw std::runtime_error("take_box");',
    "make_inner": 'const auto r = e.result_snapshot(); if (r.field("x").as<int>() != 3 || '
                  'r.field("y").as<int>() != 6) throw std::runtime_error("make_inner");',
    "take_inner": 'if (e.result_as<int>() != 9 || e.argument_snapshot(0).field("y").as<int>() != 6) '
                  'throw std::runtime_error("take_inner");',
    "make_label": 'if (e.result_as<std::string>() != "label1") throw std::runtime_error("make_label");',
    "count_label": 'if (e.result_as<int>() != 6 || e.argument_as<std::string>(0) != "label1") '
                   'throw std::runtime_error("count_label");',
    "tagged": 'if (e.result_as<int>() != 5) throw std::runtime_error("tagged");',
}


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "module-tool", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = str(Path(args.repl).with_name("clang-tracer-prepare-injectors-native"))
    env["TRACER_MODULE_EXECUTABLE"] = args.module_tool

    with tempfile.TemporaryDirectory(prefix="tracer-class-prep-") as path:
        root = Path(path)

        def run(argv, *, script=None, expected_error=None):
            result = subprocess.run(argv, input=script, cwd=root, env=env,
                                    capture_output=True, text=True, timeout=300)
            transcript = result.stdout + result.stderr
            if expected_error is None:
                assert result.returncode == 0, transcript
            else:
                assert result.returncode and expected_error in transcript, transcript
            return result.stdout

        def repl(script, *options, expected_error=None):
            return run([args.repl, "--fail-fast", *options], script=script,
                       expected_error=expected_error)

        (root / "types.hpp").write_text(TYPES)
        (root / "api.hpp").write_text(API)
        (root / "module.cpp").write_text(MODULE)
        (root / "target.cpp").write_text(TARGET)
        flags = ["-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer", "-fcf-protection=none"]
        run([args.compiler, *flags, "-fPIC", "-shared", "-I.", "module.cpp", "-o", "libclassprep.so"])
        run([args.compiler, *flags, "-I.", "target.cpp", "-L.", "-lclassprep",
             "-Wl,-rpath,$ORIGIN", "-o", "target"])
        (root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(root), "file": str(root / "module.cpp"),
            "arguments": [args.compiler, *flags, "-fPIC", "-I" + str(root), "-c",
                          str(root / "module.cpp"), "-o", str(root / "module.o")]}]))
        (root / "pkg").mkdir()
        context = ("--include types.hpp --compdb compile_commands.json "
                   "--translation-unit module.cpp")

        # A. Explicit signatures.
        explicit = "".join(
            f'%probe prepare "{selector}" --signature "{signature}" --module ./libclassprep.so '
            f"{context} --output pkg/explicit_{name}\n"
            for name, (selector, signature) in ROUTES.items())
        repl(explicit + "%quit\n")
        assert all((root / f"pkg/explicit_{name}/prepared.pb").is_file() for name in ROUTES)

        # Without a type context the message names what is missing instead of a syntax error.
        repl('%probe prepare "extra::make_plain(int)" --signature "extra::Plain(int)" '
             "--module ./libclassprep.so --output pkg/no_context\n",
             expected_error="--include HEADER --compdb FILE")
        repl('%probe prepare "extra::make_plain(int)" --signature "extra::Plain(int); int x" '
             f"--module ./libclassprep.so {context} --output pkg/bad\n",
             expected_error="must not contain")
        assert not (root / "pkg/bad").exists()
        # The headers must define the types.
        (root / "forward.hpp").write_text("#pragma once\nnamespace extra { struct Plain; }\n")
        repl('%probe prepare "extra::take_plain(extra::Plain)" --signature "int(extra::Plain)" '
             "--module ./libclassprep.so --include forward.hpp --compdb compile_commands.json "
             "--translation-unit module.cpp --output pkg/incomplete\n",
             expected_error="is incomplete in the supplied type context")
        (root / "unrelated.hpp").write_text("#pragma once\nnamespace other { struct Thing {}; }\n")
        repl('%probe prepare "extra::take_plain(extra::Plain)" --signature "int(extra::Plain)" '
             "--module ./libclassprep.so --include unrelated.hpp --compdb compile_commands.json "
             "--translation-unit module.cpp --output pkg/undeclared\n",
             expected_error="cannot be compiled in the supplied type context")
        assert not (root / "pkg/incomplete").exists() and not (root / "pkg/undeclared").exists()
        # A mismatched parameter list is still caught against the ELF symbol.
        repl('%probe prepare "extra::take_plain(extra::Plain)" --signature "int(extra::Plain, int)" '
             f"--module ./libclassprep.so {context} --output pkg/arity\n",
             expected_error="signature parameters differ from the ELF symbol")

        # B. Indexed preparation.
        repl("%module index --file ./libclassprep.so\n%quit\n")
        index = "libclassprep.so.tracer.sqlite"
        indexed = "".join(
            f'%probe prepare "{selector}" --module-index {index} --module ./libclassprep.so '
            f"{context} --output pkg/indexed_{name}\n"
            for name, (selector, _) in ROUTES.items())
        # std::string selectors name the tagged callable by its untagged spelling.
        repl(indexed + "%quit\n")
        assert all((root / f"pkg/indexed_{name}/prepared.pb").is_file() for name in ROUTES)
        # Records by value need their definitions; the message says so.
        repl(f'%probe prepare "extra::make_plain(int)" --module-index {index} --output pkg/headerless\n',
             expected_error="complete definition")
        # A context compiled with another libstdc++ ABI does not provide the recorded string type.
        (root / "compile_commands_old_abi.json").write_text(json.dumps([{
            "directory": str(root), "file": str(root / "module.cpp"),
            "arguments": [args.compiler, *flags, "-D_GLIBCXX_USE_CXX11_ABI=0", "-fPIC",
                          "-I" + str(root), "-c", str(root / "module.cpp"), "-o", "module.o"]}]))
        repl(f'%probe prepare "extra::make_label(int)" --module-index {index} --include types.hpp '
             "--compdb compile_commands_old_abi.json --translation-unit module.cpp "
             "--output pkg/abi_macro\n",
             expected_error="no member named '__cxx11'")
        assert not (root / "pkg/abi_macro").exists()
        # Layout evidence from debug information rejects headers that do not match the build.
        (root / "wrong.hpp").write_text(
            "#pragma once\nnamespace extra { struct Plain { int value; int extra_member; }; }\n")
        repl(f'%probe prepare "extra::make_plain(int)" --module-index {index} --include wrong.hpp '
             "--compdb compile_commands.json --translation-unit module.cpp --output pkg/layout\n",
             expected_error="bytes in the supplied type context but 4 bytes")
        assert not (root / "pkg/layout").exists()
        repl(f'%probe prepare "extra::make_plain(int)" --module-index {index} --include types.hpp '
             "--compdb compile_commands.json --translation-unit module.cpp --generated nowhere "
             "--output pkg/generated\n", expected_error="cannot be combined with --include")
        # The pointer control still prepares from the index alone.
        repl(f'%probe prepare "extra::read_plain(extra::Plain const*)" --module-index {index} '
             "--output pkg/indexed_pointer\n%quit\n")
        # A record reached only through a pointer needs a declaration, which the headers that
        # define the by-value types are not required to provide.
        repl(f'%probe prepare "extra::make_with_ctx(extra::Opaque*)" --module-index {index} '
             "--include types.hpp --compdb compile_commands.json --translation-unit module.cpp "
             "--output pkg/opaque_pointer\n%quit\n")
        assert (root / "pkg/opaque_pointer/prepared.pb").is_file()
        # A type context missing a header reports the header, not one of the types.
        repl(f'%probe prepare "extra::make_plain(int)" --module-index {index} '
             "--include missing.hpp --compdb compile_commands.json --translation-unit module.cpp "
             "--output pkg/no_header\n", expected_error="missing.hpp")

        # C. ABI-tag lookup.
        def show(selector, expected_error=None):
            return run([args.module_tool, "show", "--index", index, "--symbol", selector],
                       expected_error=expected_error)

        tagged = json.loads(show("extra::tagged[abi:v1](int)"))
        assert json.loads(show("extra::tagged(int)"))["id"] == tagged["id"], tagged
        assert json.loads(show("extra::tagged(int)"))["selector"] == "extra::tagged[abi:v1](int)"
        multi = json.loads(show("extra::multi(int)"))
        assert multi["selector"] == "extra::multi[abi:a][abi:b](int)", multi
        # An exact selector keeps precedence over a tagged callable that strips to it.
        assert json.loads(show("extra::over(int)"))["selector"] == "extra::over(int)"
        assert json.loads(show("extra::over[abi:z](int)"))["selector"] == "extra::over[abi:z](int)"
        # Overloads and qualifiers stay distinct.
        assert json.loads(show("extra::ov(int)"))["linkage"] == "_ZN5extra2ovB1tEi"
        assert json.loads(show("extra::ov(long)"))["linkage"] == "_ZN5extra2ovB1tEl"
        assert json.loads(show("extra::Cv::get() const"))["linkage"] == "_ZNK5extra2Cv3getB1tEv"
        assert json.loads(show("extra::Cv::get()"))["linkage"] == "_ZN5extra2Cv3getB1tEv"
        # Different tagged contracts are ambiguous, also at one address (an alias).
        show("extra::dup(int)", expected_error="ambiguous_symbol")
        assert json.loads(show("extra::dup[abi:x](int)"))["linkage"] == "_ZN5extra3dupB1xEi"
        # A request that spells a tag must match it exactly.
        show("extra::tagged[abi:other](int)", expected_error="unknown symbol")
        # Candidates beyond the first rows of the table are still seen.
        show("pg::zzzzamb(int)", expected_error="ambiguous_symbol")
        # The package keeps the tagged identity and an untagged ID check is accepted.
        repl(f'%probe prepare "extra::tagged(int)" --module-index {index} --output pkg/tag_scalar\n'
             f'%probe prepare "extra::tagged(int)" --module-index {index} --id {tagged["id"]} '
             "--output pkg/tag_id\n"
             f'%probe prepare "extra::multi(int)" --module-index {index} --output pkg/tag_multi\n'
             '%probe prepare "extra::tagged(int)" --signature "int(int)" --module ./libclassprep.so '
             "--output pkg/tag_explicit\n%quit\n")
        other = json.loads(show("extra::multi(int)"))
        repl(f'%probe prepare "extra::tagged(int)" --module-index {index} --id {other["id"]} '
             "--output pkg/tag_wrong_id\n", expected_error="--id does not match the requested selector")
        selector_text = (root / "pkg/tag_scalar/prepared.pb").read_bytes()
        assert b"extra::tagged[abi:v1](int)" in selector_text, "tagged selector not preserved"
        assert b"_ZN5extra6taggedB2v1Ei" in selector_text
        # The explicit route records the tagged spelling of a callable requested without tags.
        assert b"extra::make_label[abi:cxx11](int)" in (
            root / "pkg/explicit_make_label/prepared.pb").read_bytes()

        # D. Install, observe and check the target's own behaviour.
        checked = [*ROUTES, "tagged"]
        # An event carries the selector its package records: the tagged spelling when the
        # callable was requested without its ABI tags, otherwise the selector as written.
        indexed_names = {name: json.loads(show(selector))["selector"]
                         for name, (selector, _) in ROUTES.items()}
        indexed_names["tagged"] = tagged["selector"]
        spelled = {
            "indexed": indexed_names,
            "explicit": {name: (text if "[abi:" in text else ROUTES[name][0]
                                if name in ROUTES else text)
                         for name, text in indexed_names.items()},
        }
        for kind in ("explicit", "indexed"):
            with subprocess.Popen([root / "target"], cwd=root, stdout=subprocess.PIPE,
                                  text=True) as target:
                try:
                    assert target.stdout.readline().strip() == "ready"
                    script = "".join(
                        f'%probe inject "{selector}" --prepared pkg/{kind}_{name} '
                        "--prepared-mode require\n" for name, (selector, _) in ROUTES.items())
                    script += ('%probe inject "extra::tagged(int)" --prepared pkg/tag_scalar '
                               "--prepared-mode require\n")
                    for name in checked:
                        script += (
                            f'%collect --name "{spelled[kind][name]}" --phase POST --count 2 --timeout 40 '
                            f"--as got_{name}\n"
                            f'{{ auto got = tracer.batch("got_{name}"); '
                            f'if (got.size() < 2) throw std::runtime_error("too few events: {name}"); '
                            f"for (const auto& e : got) {{ {BODIES[name]} }} "
                            f'std::cout << "CASE {name} " << got.size() << " VALUES_OK\\n"; }}\n')
                    transcript = repl(script + "%detach\n%quit\n", "--server", args.server,
                                      "--agent", args.agent, "--attach", str(target.pid),
                                      "--timeout", "90")
                    for name in checked:
                        assert f"CASE {name} " in transcript, transcript
                    assert transcript.count("VALUES_OK") == len(checked), transcript
                    assert target.poll() is None, f"target stopped during {kind} tracing"
                finally:
                    if target.poll() is None:
                        target.terminate()
                        target.wait(timeout=20)


if __name__ == "__main__":
    main()
