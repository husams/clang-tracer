"""Automatic capture of a record keeps the members it can encode when another cannot be.

A record with a std::mutex or a std::ostringstream member used to be UNSUPPORTED as a whole,
which lost the members that are plain data. It is now PARTIAL: the other members are captured
with their field numbers unchanged, the member left out is listed with the reason (in the event,
and at install and validate time), reading it throws, and a record that captures nothing stays
UNSUPPORTED. Both ways of reading the target are covered: the direct route (`--include`, public
members) and the prepared route (`%probe prepare`, which also reads private members and
receivers).

The target is compiled here; the checks run inside the REPL through the C++ event API.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

HEADER = """#pragma once
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
namespace pc {
struct Plain { int id = 7; std::string name = "ada"; };
struct Mutex { int guarded = 5; mutable std::mutex m; };
struct Stream { int id = 5; std::ostringstream out; };
struct First { std::mutex m; int b = 22; int c = 33; };
struct Middle { int a = 11; std::mutex m; int c = 33; };
struct Last { int a = 11; int b = 22; std::mutex m; };
struct Nested { int tag = 4; Mutex inner; };
struct Items { std::vector<Mutex> items; };
struct ByName { std::map<std::string, Mutex> by_name; };
struct L { int v = 1; std::mutex lock; };
struct R { int w = 2; std::mutex lock; };
struct Both : L, R { int own = 3; };
struct Blocked { std::mutex only; };
struct Text { int id = 1; std::string text; std::mutex m; };
// Several members left out under one name: three anonymous unions (the prepared route
// refuses to prepare those, so this one is read directly).
struct Anons { union { int a; float b; }; union { int c; float d; }; union { int e; float f; }; int g = 9; };
// A record that contains itself and has a member left out: partial at every level.
struct Tree { int v = 1; std::mutex m; std::vector<Tree> kids; };
// A member left out and a captured one with the same name, in either order.
struct Base { std::mutex m; };
struct Derived : Base { int m = 6; };
struct Early { int m = 7; };
struct Late : Early { std::mutex m; };
// A private member: read by the prepared route only.
class Access {
public:
  int open = 1;
private:
  int hidden_ = 2;
};
class Counter {
public:
  int add(int n);
  int total = 40;
private:
  std::mutex lock_;
};
const Mutex &echo(const Mutex &value);
int f_plain(const Plain &value);
int f_mutex(const Mutex &value);
int f_stream(const Stream &value);
int f_first(const First &value);
int f_middle(const Middle &value);
int f_last(const Last &value);
int f_nested(const Nested &value);
int f_items(const Items &value);
int f_by_name(const ByName &value);
int f_both(const Both &value);
int f_blocked(const Blocked &value);
int f_text(const Text &value);
int f_tree(const Tree &value);
int f_anons(const Anons &value);
int f_derived(const Derived &value);
int f_late(const Late &value);
int f_access(const Access &value);
}
"""

SOURCE = """#include "pc.hpp"
#define PC_FUNCTION(name, Type, expression) \\
  __attribute__((noinline)) int name(const Type &value) { return (expression); }
namespace pc {
__attribute__((noinline)) int Counter::add(int n) { return total + n; }
Mutex shared;
__attribute__((noinline)) const Mutex &echo(const Mutex &value) { return value; }
PC_FUNCTION(f_plain, Plain, value.id)
PC_FUNCTION(f_mutex, Mutex, value.guarded)
PC_FUNCTION(f_stream, Stream, value.id)
PC_FUNCTION(f_first, First, value.b)
PC_FUNCTION(f_middle, Middle, value.a)
PC_FUNCTION(f_last, Last, value.a)
PC_FUNCTION(f_nested, Nested, value.tag)
PC_FUNCTION(f_items, Items, static_cast<int>(value.items.size()))
PC_FUNCTION(f_by_name, ByName, static_cast<int>(value.by_name.size()))
PC_FUNCTION(f_both, Both, value.own)
PC_FUNCTION(f_blocked, Blocked, 1)
PC_FUNCTION(f_text, Text, static_cast<int>(value.text.size()))
PC_FUNCTION(f_tree, Tree, value.v)
PC_FUNCTION(f_anons, Anons, value.g)
PC_FUNCTION(f_derived, Derived, value.m)
PC_FUNCTION(f_late, Late, value.Early::m)
PC_FUNCTION(f_access, Access, value.open)
}
"""

TARGET = """#include "pc.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY (-1UL)
#endif
int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) return 1;
  std::puts("ready");
  std::fflush(stdout);
  pc::Counter counter;
  for (;;) {
    pc::Plain plain; pc::Mutex mutex; pc::Stream stream; pc::First first; pc::Middle middle;
    pc::Last last; pc::Nested nested; pc::Both both; pc::Blocked blocked;
    pc::Items items{std::vector<pc::Mutex>(2)};
    items.items[0].guarded = 1;
    items.items[1].guarded = 2;
    pc::ByName by_name;
    by_name.by_name["x"].guarded = 8;
    by_name.by_name["y"].guarded = 9;
    pc::Anons anons; pc::Derived derived; pc::Late late; pc::Access access;
    pc::Tree tree;
    tree.v = 1;
    tree.kids = std::vector<pc::Tree>(2);
    tree.kids[0].v = 2;
    tree.kids[1].v = 3;
    tree.kids[0].kids = std::vector<pc::Tree>(1);
    tree.kids[0].kids[0].v = 4;
    pc::Text short_text, long_text;
    short_text.text = "short";
    long_text.text = std::string(6000, 'z');
    pc::f_plain(plain); pc::f_mutex(mutex); pc::f_stream(stream); pc::f_first(first);
    pc::f_middle(middle); pc::f_last(last); pc::f_nested(nested); pc::f_items(items);
    pc::f_by_name(by_name); pc::f_both(both); pc::f_blocked(blocked);
    pc::f_text(short_text); pc::f_text(long_text);
    pc::f_tree(tree);
    pc::f_anons(anons); pc::f_derived(derived); pc::f_late(late); pc::f_access(access);
    pc::echo(mutex);
    counter.add(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
"""

INTERCEPTOR = """struct Interceptor {
  void pre(const Call &) {}
  void post(const Call &) {}
};
"""

# Loaded into the REPL once: small helpers the per-case checks share.
CHECKS = """#include <stdexcept>
#include <string>
inline void pc_require(bool ok, const std::string &what) {
  if (!ok) throw std::runtime_error("CHECK FAILED: " + what);
}
inline bool pc_has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}
// The first status the event shows, at or after `from`.
inline std::string pc_status(const std::string &text, const std::string &from = "") {
  const auto begin = from.empty() ? std::size_t(0) : text.find(from);
  if (begin == std::string::npos) return "";
  const auto at = text.find("status: ", begin);
  if (at == std::string::npos) return "";
  const auto end = text.find_first_of("\\n ", at + 8);
  return text.substr(at + 8, end - at - 8);
}
// The `unavailable { ... }` entry of the event that names `name`.
inline std::string pc_entry(const std::string &text, const std::string &name) {
  for (auto at = text.find("unavailable {"); at != std::string::npos;
       at = text.find("unavailable {", at + 1)) {
    const auto block = text.substr(at, text.find('}', at) - at);
    if (pc_has(block, "name: \\"" + name + "\\"")) return block;
  }
  return "";
}
// What reading a field that was left out says.
template <class V> std::string pc_error(const V &value, const char *name) {
  try {
    value.field(name);
  } catch (const std::out_of_range &error) {
    return error.what();
  }
  return "";
}
// A value that is partial: `name` was left out for a reason that mentions `why`.
template <class V> void pc_left_out(const V &value, const char *name, const std::string &why) {
  pc_require(value.is_partial(), std::string("is_partial for ") + name);
  const auto found = value.unavailable().find(name);
  pc_require(found != value.unavailable().end() && pc_has(found->second, why),
             std::string("unavailable reason for ") + name);
  const auto error = pc_error(value, name);
  pc_require(pc_has(error, "was not captured") && pc_has(error, name) && pc_has(error, why),
             std::string("field() error for ") + name + ": " + error);
}
"""

MUTEX_WHY = "pthread_mutex_t"
STREAM_WHY = "basic_ostringstream"
PARTIAL = 'pc_require(pc_status(t) == "PARTIAL", "status " + pc_status(t)); '


def entry_checks(name, why, number=None):
    checks = f'{{ const auto block = pc_entry(t, "{name}"); pc_require(!block.empty(), "entry {name}"); '
    checks += f'pc_require(pc_has(block, "{why}"), "reason {name}"); '
    if number is not None:
        checks += f'pc_require(pc_has(block, "number: {number}"), "number {name}"); '
    return checks + "} "


# case: (function name to match in the event, check body over `e` and its text `t`,
#        notes the install output must show, signature for the prepared route)
CASES = {
    "plain": ("f_plain(", 'const auto s = e.argument_snapshot(0); '
              'pc_require(s.field("id").as<int>() == 7 && s.field("name").as<std::string>() == "ada", "plain"); '
              'pc_require(!s.is_partial() && s.unavailable().empty(), "plain is whole"); '
              'pc_require(pc_status(t) == "COMPLETE" && !pc_has(t, "unavailable {"), "plain status"); ',
              [], "pc::f_plain(const pc::Plain &)"),
    "mutex": ("f_mutex(", 'const auto s = e.argument_snapshot(0); '
              'pc_require(s.field("guarded").as<int>() == 5, "guarded"); '
              f'pc_left_out(s, "m", "{MUTEX_WHY}"); ' + PARTIAL + entry_checks("m", MUTEX_WHY, 2),
              ["pc::Mutex::m of type std::mutex"], "pc::f_mutex(const pc::Mutex &)"),
    "stream": ("f_stream(", 'const auto s = e.argument_snapshot(0); '
               'pc_require(s.field("id").as<int>() == 5, "id"); '
               f'pc_left_out(s, "out", "{STREAM_WHY}"); ' + PARTIAL + entry_checks("out", STREAM_WHY, 2),
               ["pc::Stream::out of type std::basic_ostringstream"], "pc::f_stream(const pc::Stream &)"),
    # The members around the one left out keep their values and numbers.
    "first": ("f_first(", 'const auto s = e.argument_snapshot(0); '
              'pc_require(s.field("b").as<int>() == 22 && s.field("c").as<int>() == 33, "siblings"); '
              f'pc_left_out(s, "m", "{MUTEX_WHY}"); ' + PARTIAL + entry_checks("m", MUTEX_WHY, 1),
              ["pc::First::m"], "pc::f_first(const pc::First &)"),
    "middle": ("f_middle(", 'const auto s = e.argument_snapshot(0); '
               'pc_require(s.field("a").as<int>() == 11 && s.field("c").as<int>() == 33, "siblings"); '
               f'pc_left_out(s, "m", "{MUTEX_WHY}"); ' + PARTIAL + entry_checks("m", MUTEX_WHY, 2),
               ["pc::Middle::m"], "pc::f_middle(const pc::Middle &)"),
    "last": ("f_last(", 'const auto s = e.argument_snapshot(0); '
             'pc_require(s.field("a").as<int>() == 11 && s.field("b").as<int>() == 22, "siblings"); '
             f'pc_left_out(s, "m", "{MUTEX_WHY}"); ' + PARTIAL + entry_checks("m", MUTEX_WHY, 3),
             ["pc::Last::m"], "pc::f_last(const pc::Last &)"),
    "nested": ("f_nested(", 'const auto s = e.argument_snapshot(0); '
               'pc_require(s.field("tag").as<int>() == 4 && s.field("inner").field("guarded").as<int>() == 5, "nested"); '
               'pc_require(s.is_partial() && s.unavailable().empty(), "the parent is whole itself"); '
               f'pc_left_out(s.field("inner"), "m", "{MUTEX_WHY}"); ' + PARTIAL,
               ["pc::Mutex::m"], "pc::f_nested(const pc::Nested &)"),
    "items": ("f_items(", 'const auto s = e.argument_snapshot(0); const auto &items = s.field("items"); '
              'pc_require(items.elements().size() == 2, "two elements"); '
              'pc_require(items.at(0).field("guarded").as<int>() == 1 && items.at(1).field("guarded").as<int>() == 2, "order"); '
              f'pc_left_out(items.at(0), "m", "{MUTEX_WHY}"); pc_left_out(items.at(1), "m", "{MUTEX_WHY}"); ' + PARTIAL,
              ["pc::Mutex::m"], "pc::f_items(const pc::Items &)"),
    "by_name": ("f_by_name(", 'const auto s = e.argument_snapshot(0); const auto &pairs = s.field("by_name"); '
                'pc_require(pairs.elements().size() == 2, "two entries"); '
                'pc_require(pairs.at(0).field("key").as<std::string>() == "x" && pairs.at(0).field("value").field("guarded").as<int>() == 8, "x"); '
                'pc_require(pairs.at(1).field("key").as<std::string>() == "y" && pairs.at(1).field("value").field("guarded").as<int>() == 9, "y"); '
                f'pc_left_out(pairs.at(1).field("value"), "m", "{MUTEX_WHY}"); ' + PARTIAL,
                ["pc::Mutex::m"], "pc::f_by_name(const pc::ByName &)"),
    # Two bases leave out a member of the same name: both are listed, under distinct names.
    "both": ("f_both(", 'const auto s = e.argument_snapshot(0); '
             'pc_require(s.field("v").as<int>() == 1 && s.field("w").as<int>() == 2 && s.field("own").as<int>() == 3, "bases"); '
             'pc_require(s.unavailable().size() == 2 && s.unavailable().count("pc::L::lock") == 1 && s.unavailable().count("pc::R::lock") == 1, "two names"); '
             f'pc_left_out(s, "pc::L::lock", "{MUTEX_WHY}"); pc_left_out(s, "pc::R::lock", "{MUTEX_WHY}"); ' + PARTIAL +
             entry_checks("pc::L::lock", MUTEX_WHY) + entry_checks("pc::R::lock", MUTEX_WHY),
             ["pc::L::lock of type std::mutex", "pc::R::lock of type std::mutex"],
             "pc::f_both(const pc::Both &)"),
    # Nothing is left to capture: unsupported, with the reason it always had.
    "blocked": ("f_blocked(", 'try { e.argument_snapshot(0); pc_require(false, "blocked snapshot is unavailable"); } '
                'catch (const std::runtime_error &error) { pc_require(pc_has(error.what(), "' + MUTEX_WHY + '"), error.what()); } '
                'pc_require(pc_status(t) == "UNSUPPORTED" && pc_has(t, "' + MUTEX_WHY + '"), "status " + pc_status(t)); '
                'pc_require(!pc_has(t, "unavailable {"), "nothing is listed as left out"); ',
                [], "pc::f_blocked(const pc::Blocked &)"),
    # A record that contains itself: every level is partial, and the numbers hold.
    "tree": ("f_tree(", 'const auto s = e.argument_snapshot(0); const auto &kids = s.field("kids"); '
             'pc_require(s.field("v").as<int>() == 1 && kids.elements().size() == 2, "root"); '
             'pc_require(kids.at(0).field("v").as<int>() == 2 && kids.at(1).field("v").as<int>() == 3, "level one"); '
             'const auto &deeper = kids.at(0).field("kids"); '
             'pc_require(deeper.elements().size() == 1 && deeper.at(0).field("v").as<int>() == 4, "level two"); '
             'pc_require(kids.at(1).field("kids").elements().empty(), "a leaf"); '
             f'pc_left_out(s, "m", "{MUTEX_WHY}"); pc_left_out(kids.at(0), "m", "{MUTEX_WHY}"); '
             f'pc_left_out(kids.at(1), "m", "{MUTEX_WHY}"); pc_left_out(deeper.at(0), "m", "{MUTEX_WHY}"); ' +
             PARTIAL + entry_checks("m", MUTEX_WHY, 2),
             ["pc::Tree::m of type std::mutex"], "pc::f_tree(const pc::Tree &)"),
    # A member left out and one that is captured share a name: the captured one keeps it,
    # the other names the record that declares it.
    "derived": ("f_derived(", 'const auto s = e.argument_snapshot(0); '
                'pc_require(s.field("m").as<int>() == 6, "the captured m"); '
                f'pc_left_out(s, "pc::Base::m", "{MUTEX_WHY}"); pc_require(s.unavailable().size() == 1, "one left out"); ' + PARTIAL,
                ["pc::Base::m of type std::mutex"], "pc::f_derived(const pc::Derived &)"),
    "late": ("f_late(", 'const auto s = e.argument_snapshot(0); '
             'pc_require(s.field("m").as<int>() == 7, "the captured m"); '
             f'pc_left_out(s, "pc::Late::m", "{MUTEX_WHY}"); pc_require(s.unavailable().size() == 1, "one left out"); ' + PARTIAL,
             ["pc::Late::m of type std::mutex"], "pc::f_late(const pc::Late &)"),
    # A limit that cuts a value at run time is not a static omission.
    "text": ("f_text(", 'if (pc_status(t) == "PARTIAL") { const auto s = e.argument_snapshot(0); ++partial; '
             'pc_require(s.field("id").as<int>() == 1 && s.field("text").as<std::string>() == "short", "short text"); '
             f'pc_left_out(s, "m", "{MUTEX_WHY}"); '
             '} else { ++cut; pc_require(pc_status(t) == "LIMIT_EXCEEDED" && pc_has(t, "string budget"), "status " + pc_status(t)); '
             'pc_require(!pc_has(t, "unavailable {"), "a cut value lists nothing"); '
             'try { e.argument_snapshot(0); pc_require(false, "cut snapshot is unavailable"); } '
             'catch (const std::runtime_error &error) { pc_require(pc_has(error.what(), "string budget"), error.what()); } } ',
             ["pc::Text::m"], "pc::f_text(const pc::Text &)"),
    # The result of a call, a record returned by reference.
    "echo": ("echo(", 'const auto s = e.result_snapshot(); pc_require(s.field("guarded").as<int>() == 5, "result"); '
             f'pc_left_out(s, "m", "{MUTEX_WHY}"); '
             'pc_require(pc_status(t, "result {") == "PARTIAL", "result status " + pc_status(t, "result {")); ',
             ["result (pc::Mutex)", "pc::Mutex::m"], "pc::echo(const pc::Mutex &)"),
}
# Only one route can do each of these.
ANONYMOUS = ("f_anons(", 'const auto s = e.argument_snapshot(0); '
             'pc_require(s.field("g").as<int>() == 9, "g"); '
             'pc_require(s.is_partial() && s.unavailable().size() == 3, "three members left out"); '
             'for (const auto &item : s.unavailable()) { '
             'pc_require(item.first.rfind("(anonymous union #", 0) == 0, item.first); '
             'pc_left_out(s, item.first.c_str(), "union state layout"); } ' + PARTIAL,
             ["(anonymous union #1)", "(anonymous union #2)", "(anonymous union #3)"],
             "pc::f_anons(const pc::Anons &)")
# The private member is a note and a listed member on the direct route, plain data on the other.
ACCESS_DIRECT = ("f_access(", 'const auto s = e.argument_snapshot(0); '
                 'pc_require(s.field("open").as<int>() == 1, "open"); '
                 'pc_left_out(s, "hidden_", "non-public member"); ' + PARTIAL,
                 ["pc::Access::hidden_"], "pc::f_access(const pc::Access &)")
ACCESS_PREPARED = ("f_access(", 'const auto s = e.argument_snapshot(0); '
                   'pc_require(s.field("open").as<int>() == 1 && s.field("hidden_").as<int>() == 2, "both members"); '
                   'pc_require(!s.is_partial() && s.unavailable().empty() && pc_status(t) == "COMPLETE" && '
                   '!pc_has(t, "unavailable {"), "nothing is left out"); ',
                   [], "pc::f_access(const pc::Access &)")
# Methods are captured with their receiver on the prepared route only.
RECEIVER = ("Counter::add(", 'const auto s = e.receiver_snapshot(); pc_require(s.field("total").as<int>() == 40, "total"); '
            'pc_left_out(s, "lock_", "' + MUTEX_WHY + '"); '
            'pc_require(pc_status(t, "receiver_snapshot {") == "PARTIAL", "receiver status"); ',
            ["receiver (", "pc::Counter::lock_"], "pc::Counter::add(int)")


def block(case, match, body):
    counting = "++partial" in body
    prefix = "int partial = 0, cut = 0; " if counting else ""
    suffix = (' pc_require(partial >= 1 && cut >= 1, "both kinds of event seen: " + std::to_string(partial) '
              '+ "/" + std::to_string(cut));' if counting else "")
    return (f'{{ {prefix}auto got = tracer.batch("all"); int seen = 0; for (const auto &e : got) {{ '
            f'if (e.function_name().find("{match}") == std::string::npos) continue; ++seen; '
            f'const auto t = e.describe(); {body}}} pc_require(seen >= 1, "no events for {case}");{suffix} '
            f'std::cout << "CASE {case} " << seen << " OK\\n"; }}\n')


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "module-tool", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = str(Path(args.repl).with_name("clang-tracer-prepare-injectors-native"))
    env["TRACER_MODULE_EXECUTABLE"] = args.module_tool

    with tempfile.TemporaryDirectory(prefix="tracer-partial-") as path:
        root = Path(path)
        (root / "home").mkdir()
        (root / "pkg").mkdir()
        env["HOME"] = str(root / "home")
        header = root / "pc.hpp"
        header.write_text(HEADER)
        (root / "pc.cpp").write_text(SOURCE)
        (root / "target.cpp").write_text(TARGET)
        (root / "interceptor.cpp").write_text(INTERCEPTOR)
        (root / "checks.hpp").write_text(CHECKS)
        flags = ["-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer", "-fcf-protection=none", "-pthread"]
        subprocess.run([args.compiler, *flags, "-I.", "pc.cpp", "target.cpp", "-o", "target"],
                       cwd=root, env=env, check=True, capture_output=True, text=True, timeout=300)
        # The prepared route reads the build's flags from the compilation database.
        (root / "compile_commands.json").write_text(json.dumps([
            {"directory": str(root), "file": str(root / source),
             "arguments": [args.compiler, *flags, "-I" + str(root), "-c", str(root / source),
                           "-o", source.replace(".cpp", ".o")]}
            for source in ("pc.cpp", "target.cpp")]))

        def repl(script, *, pid, target_options=()):
            result = subprocess.run(
                [args.repl, "--fail-fast", "--server", args.server, "--agent", args.agent,
                 "--attach", str(pid), "--timeout", "120", *target_options],
                input=script, cwd=root, env=env, capture_output=True, text=True, timeout=500)
            transcript = result.stdout + result.stderr
            assert result.returncode == 0, transcript
            return result.stdout

        def with_target(work, extra_env=None):
            target_env = {**env, **(extra_env or {})}
            with subprocess.Popen([root / "target"], cwd=root, env=target_env,
                                  stdout=subprocess.PIPE, text=True) as target:
                try:
                    assert target.stdout.readline().strip() == "ready"
                    result = work(target.pid)
                    assert target.poll() is None, "the target stopped while it was traced"
                    return result
                finally:
                    if target.poll() is None:
                        target.terminate()
                        target.wait(timeout=20)

        def notes_of(transcript, installed):
            """The capture note lines that follow each install, in install order."""
            sections, current = [], None
            for line in transcript.splitlines():
                if line.startswith("installed probe="):
                    current = []
                    sections.append(current)
                elif line.startswith("capture note: ") and current is not None:
                    current.append(line)
            assert len(sections) == len(installed), (len(sections), installed, transcript)
            return dict(zip(installed, sections))

        def check_notes(transcript, cases):
            notes = notes_of(transcript, list(cases))
            for case, (_, _, expected, _) in cases.items():
                lines = "\n".join(notes[case])
                if not expected:
                    assert not notes[case], (case, notes[case])
                for part in expected:
                    assert part in lines, (case, part, notes[case])
                for line in notes[case]:
                    assert "is not captured: " in line, line

        def collect_and_check(cases):
            # Probes are installed one after the other, so the first ones have run many times
            # before the last is in place: collect for long enough that each has been seen.
            script = "%collect --phase POST --count 3000 --timeout 12 --as all\n"
            for case, (match, body, _, _) in cases.items():
                script += block(case, match, body)
            return script

        def route(kind):
            cases = dict(CASES)
            if kind == "prepared":
                cases["counter"] = RECEIVER
                cases["access"] = ACCESS_PREPARED
            else:
                cases["anonymous"] = ANONYMOUS
                cases["access"] = ACCESS_DIRECT
            script = "%cpp load checks.hpp\n"
            if kind == "prepared":
                for case, (_, _, _, signature) in cases.items():
                    script += (f'%probe prepare "{signature}" --include {header} '
                               f"--output pkg/{case}\n")
            for case, (match, _, _, signature) in cases.items():
                if kind == "prepared":
                    script += (f'%probe inject "{signature}" --callback interceptor.cpp '
                               f"--prepared pkg/{case} --prepared-mode require\n")
                else:
                    name = signature.split("(")[0]
                    script += (f'%probe inject "{name}" --callback interceptor.cpp '
                               f"--include {header}\n")
            script += collect_and_check(cases)
            script += "%detach\n"
            # The events stay readable after the session is gone.
            for case in ("mutex", "both", "text"):
                match, body, _, _ = cases[case]
                script += block(case + "_after_detach", match, body)
            output = with_target(lambda pid: repl(script + "%quit\n", pid=pid))
            for case in cases:
                assert f"CASE {case} " in output, (kind, case, output)
            for case in ("mutex", "both", "text"):
                assert f"CASE {case}_after_detach " in output, (kind, case, output)
            check_notes(output, cases)
            return kind

        def validate():
            """The validation report carries the same notes, on either route."""
            signature = "pc::f_mutex(const pc::Mutex &)"
            script = (f'%probe validate "pc::f_mutex" --callback interceptor.cpp --include {header}\n'
                      f'%probe prepare "{signature}" --include {header} --output pkg/validate\n'
                      f'%probe validate "{signature}" --callback interceptor.cpp '
                      "--prepared pkg/validate --prepared-mode require\n"
                      f'%probe validate "pc::f_plain" --callback interceptor.cpp --include {header}\n')
            output = with_target(lambda pid: repl(script + "%detach\n%quit\n", pid=pid))
            note = "member pc::Mutex::m of type std::mutex is not captured: "
            assert output.count(note) == 2, output
            assert output.count('stage: "snapshot"') == 2 and 'severity: "note"' in output, output
            return "validate"

        def fallback():
            """Code that captures partial records and does not compile gives way to the
            whole-record capture: the probe installs and reports what it always did."""
            script = ("%cpp load checks.hpp\n"
                      f'%probe inject "pc::f_mutex" --callback interceptor.cpp --include {header}\n'
                      "%collect --phase POST --count 2 --timeout 60 --as all\n" +
                      block("fallback", "f_mutex(",
                            'try { e.argument_snapshot(0); pc_require(false, "unsupported"); } '
                            'catch (const std::runtime_error &error) { pc_require(pc_has(error.what(), "' + MUTEX_WHY + '"), error.what()); } '
                            'pc_require(pc_status(t) == "UNSUPPORTED", "status " + pc_status(t)); '
                            'pc_require(!pc_has(t, "unavailable {"), "nothing is partial"); '))
            output = with_target(lambda pid: repl(script + "%detach\n%quit\n", pid=pid),
                                 {"TRACER_TEST_BREAK_PARTIAL_CAPTURE": "1"})
            assert "CASE fallback " in output and "installed probe=" in output, output
            assert "capture note:" not in output, output
            return "fallback"

        jobs = [lambda: route("direct"), lambda: route("prepared"), validate, fallback]
        with concurrent.futures.ThreadPoolExecutor(max_workers=len(jobs)) as pool:
            futures = [pool.submit(job) for job in jobs]
            for future in futures:
                print("passed:", future.result())


if __name__ == "__main__":
    main()
