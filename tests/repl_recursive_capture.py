"""Capture records that contain themselves: trees, lists and shared graphs (issue #180).

A target calls one function per case with a populated structure. Each case is captured
through a prepared package, the tree also through a direct injection, and every node is
read back from the event: by field, and as a typed value where the graph can be rebuilt.
A chain longer than the depth budget keeps the nodes before the cut, a cycle is cut at the
object that is met again, and an object that two pointers own is captured once.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile

HEADER = r"""#pragma once
// Populated recursive records for issue #180: one function per case.
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rec {
struct TreeNode { int v = 0; std::string name; std::vector<TreeNode> kids; };
struct ListNode { int v = 0; std::unique_ptr<ListNode> next; };
struct SharedNode {
  int v = 0;
  std::shared_ptr<SharedNode> other;
  std::shared_ptr<SharedNode> next;
  std::string text;
};
struct Folder;
struct Entry { std::string name; std::unique_ptr<Folder> folder; };
struct Folder { int id = 0; std::vector<Entry> entries; std::map<std::string, Folder> named; };
struct Graph { std::shared_ptr<SharedNode> left; std::shared_ptr<SharedNode> right; int tail = 0; };

int f_tree(const TreeNode &value);
int f_leaf(const TreeNode &value);
int f_list(const ListNode &value);
int f_single(const ListNode &value);
int f_long_list(const ListNode &value);
int f_cycle(const SharedNode &value);
int f_dag(const Graph &value);
int f_folder(const Folder &value);
} // namespace rec
"""

MODULE = r"""#include "recursive.hpp"
namespace rec {
__attribute__((noinline)) int touch(const void *p) { return p != nullptr; }
#define REC_DEFINE(name, Type) \
  __attribute__((noinline)) int name(const Type &value) { return touch(&value); }
REC_DEFINE(f_tree, TreeNode) REC_DEFINE(f_leaf, TreeNode) REC_DEFINE(f_list, ListNode)
REC_DEFINE(f_single, ListNode) REC_DEFINE(f_long_list, ListNode) REC_DEFINE(f_cycle, SharedNode)
REC_DEFINE(f_dag, Graph) REC_DEFINE(f_folder, Folder)
} // namespace rec
"""

TARGET = r"""#include "recursive.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace {
std::atomic<bool> stop{false};
void on_signal(int) { stop = true; }
rec::TreeNode tree(int v, std::string name, std::vector<rec::TreeNode> kids = {}) {
  rec::TreeNode node;
  node.v = v;
  node.name = std::move(name);
  node.kids = std::move(kids);
  return node;
}
std::unique_ptr<rec::ListNode> chain(int count) {
  std::unique_ptr<rec::ListNode> head;
  for (int v = count; v >= 1; --v) {
    auto node = std::make_unique<rec::ListNode>();
    node->v = v;
    node->next = std::move(head);
    head = std::move(node);
  }
  return head;
}
std::shared_ptr<rec::SharedNode> shared(int v, std::string text = {}) {
  auto node = std::make_shared<rec::SharedNode>();
  node->v = v;
  node->text = std::move(text);
  return node;
}
} // namespace

int main() {
  std::signal(SIGTERM, on_signal);
  std::signal(SIGINT, on_signal);
  // A populated three-level tree and a leaf.
  const auto root = tree(1, "root", {tree(2, "left", {tree(4, "left.a"), tree(5, "left.b")}),
                                     tree(3, "right", {tree(6, "right.a")})});
  const auto leaf = tree(9, "leaf");
  // A five-node list, a single node and a chain longer than the depth budget.
  const auto list = chain(5);
  const auto single = chain(1);
  const auto long_list = chain(40);
  // Two nodes that own each other.
  auto first = shared(1, "first"), second = shared(2, "second");
  first->next = second;
  second->next = first;
  // Two edges that share a child, without a cycle.
  rec::Graph dag;
  dag.left = shared(10, "left");
  dag.right = shared(20, "right");
  dag.left->next = shared(30, "shared child");
  dag.right->next = dag.left->next;
  dag.right->other = dag.left;
  dag.tail = 7;
  // Two records that contain each other, and a map of the record itself.
  rec::Folder folder;
  folder.id = 1;
  folder.entries.resize(2);
  folder.entries[0].name = "docs";
  folder.entries[0].folder = std::make_unique<rec::Folder>();
  folder.entries[0].folder->id = 2;
  folder.entries[0].folder->entries.resize(1);
  folder.entries[0].folder->entries[0].name = "deep";
  folder.entries[0].folder->entries[0].folder = std::make_unique<rec::Folder>();
  folder.entries[0].folder->entries[0].folder->id = 3;
  folder.entries[1].name = "empty";
  folder.named["alias"].id = 4;
  folder.named["alias"].named["inner"].id = 5;

  std::cout << "ready pid=" << getpid() << " first=" << first.get() << " second=" << second.get()
            << std::endl;
  long sum = 0;
  while (!stop) {
    sum += rec::f_tree(root) + rec::f_leaf(leaf) + rec::f_list(*list) + rec::f_single(*single) +
           rec::f_long_list(*long_list) + rec::f_cycle(*first) + rec::f_dag(dag) +
           rec::f_folder(folder);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  second->next.reset(); // The cycle is owned by this program: break it before leaving.
  // Every call returned 1 and the structures are what they were.
  const bool intact = sum % 8 == 0 && root.kids[0].kids[1].name == "left.b" && list->next->v == 2 &&
                      long_list->v == 1 && first->next == second && dag.right->next->v == 30 &&
                      folder.entries[0].folder->entries[0].folder->id == 3;
  std::cout << (intact ? "intact" : "changed") << std::endl;
  return intact ? 0 : 1;
}
"""

INTERCEPTOR = r"""struct Interceptor {
  void pre(const Call &) {}
  void post(const Call &) {}
};
"""

# The checks of each case use the event `e` and its argument snapshot `a`.
TREE = (
    'const auto& l = a.field("kids").at(0); const auto& r = a.field("kids").at(1); '
    'if (a.field("v").as<int>() != 1 || a.field("name").as<std::string>() != "root" || '
    'a.field("kids").size() != 2 || l.field("name").as<std::string>() != "left" || '
    'l.field("kids").size() != 2 || l.field("kids").at(0).field("v").as<int>() != 4 || '
    'l.field("kids").at(1).field("name").as<std::string>() != "left.b" || '
    'l.field("kids").at(1).field("kids").size() != 0 || r.field("v").as<int>() != 3 || '
    'r.field("kids").size() != 1 || '
    'r.field("kids").at(0).field("name").as<std::string>() != "right.a" || '
    'r.field("kids").at(0).field("v").as<int>() != 6) throw std::runtime_error("tree nodes"); ')
LEAF = ('if (a.field("v").as<int>() != 9 || a.field("name").as<std::string>() != "leaf" || '
        'a.field("kids").size() != 0) throw std::runtime_error("leaf"); ')
# case: (record type, checks)
CASES = {
    "tree": ("TreeNode", TREE +
             'const auto t = e.argument_as<rec::TreeNode>(0); '
             'if (t.kids.size() != 2 || t.kids[0].kids[1].name != "left.b" || '
             't.kids[1].kids[0].v != 6) throw std::runtime_error("typed tree"); '),
    "leaf": ("TreeNode", LEAF),
    "list": ("ListNode",
             'const auto* node = &a; '
             'for (int v = 1; v <= 5; ++v) { const auto& next = node->field("next"); '
             'if (node->field("v").as<int>() != v || '
             'next.field("omitted").as<std::string>() != "") throw std::runtime_error("list node"); '
             'if (v == 5) { if (!next.field("value").is_null() || '
             'next.field("address").as<unsigned long long>() != 0) '
             'throw std::runtime_error("list end"); } '
             'else { if (next.field("value").is_null()) throw std::runtime_error("list link"); '
             'node = &next.field("value"); } } '
             'const auto t = e.argument_as<rec::ListNode>(0); int sum = 0, count = 0; '
             'for (const auto* item = &t; item; item = item->next.get(), ++count) sum += item->v; '
             'if (count != 5 || sum != 15) throw std::runtime_error("typed list"); '),
    "single": ("ListNode",
               'if (a.field("v").as<int>() != 1 || !a.field("next").field("value").is_null() || '
               'a.field("next").field("suppressed").as<bool>()) throw std::runtime_error("single"); '),
    # The nodes before the depth cut are kept; the pointer at the cut names the next node and
    # the limit, the snapshot carries a note, and a typed list is refused, not shortened.
    "long_list": ("ListNode",
                  'const auto* node = &a; int kept = 1; '
                  'while (!node->field("next").field("value").is_null()) { '
                  'node = &node->field("next").field("value"); ++kept; '
                  'if (node->field("v").as<int>() != kept) throw std::runtime_error("chain node"); } '
                  'const auto& cut = node->field("next"); '
                  'if (kept < 8 || kept >= 40 || cut.field("omitted").as<std::string>() != "depth" || '
                  '!cut.field("suppressed").as<bool>() || '
                  'cut.field("address").as<unsigned long long>() == 0 || '
                  'a.field("snapshot_cut").as<std::string>().find("omitted: depth") == std::string::npos) '
                  'throw std::runtime_error("depth cut"); '
                  'bool refused = false; try { (void)e.argument_as<rec::ListNode>(0); } '
                  'catch (const std::runtime_error& error) { '
                  'refused = std::string(error.what()).find("omitted: depth") != std::string::npos; } '
                  'if (!refused) throw std::runtime_error("a cut chain became a typed list"); '),
    # Two nodes that own each other: the pointer back to the first is the cut.
    "cycle": ("SharedNode",
              'const auto& second = a.field("next"); '
              'const auto& back = second.field("value").field("next"); '
              'if (a.field("text").as<std::string>() != "first" || '
              'second.field("value").field("text").as<std::string>() != "second" || '
              'second.field("address").as<unsigned long long>() != @SECOND@ULL || '
              'back.field("omitted").as<std::string>() != "cycle" || !back.field("value").is_null() || '
              'back.field("address").as<unsigned long long>() != @FIRST@ULL) '
              'throw std::runtime_error("cycle"); '
              'bool refused = false; try { (void)e.argument_as<rec::SharedNode>(0); } '
              'catch (const std::runtime_error& error) { '
              'refused = std::string(error.what()).find("omitted: cycle") != std::string::npos; } '
              'if (!refused) throw std::runtime_error("a cycle became a typed value"); '),
    # Two pointers own one child: it is captured once and the typed value shares it again.
    "dag": ("Graph",
            'const auto& l = a.field("left"); const auto& r = a.field("right"); '
            'const auto& first = l.field("value").field("next"); '
            'const auto& again = r.field("value").field("next"); '
            'const auto& other = r.field("value").field("other"); '
            'if (first.field("value").field("text").as<std::string>() != "shared child" || '
            'first.field("omitted").as<std::string>() != "" || '
            'again.field("omitted").as<std::string>() != "alias" || !again.field("value").is_null() || '
            'again.field("address").as<unsigned long long>() != '
            'first.field("address").as<unsigned long long>() || '
            'other.field("omitted").as<std::string>() != "alias" || '
            'other.field("address").as<unsigned long long>() != '
            'l.field("address").as<unsigned long long>() || a.field("tail").as<int>() != 7) '
            'throw std::runtime_error("dag"); '
            'const auto g = e.argument_as<rec::Graph>(0); '
            'if (!g.left || !g.right || g.left->next.get() != g.right->next.get() || '
            'g.right->other.get() != g.left.get() || g.right->next->text != "shared child" || '
            'g.tail != 7) throw std::runtime_error("typed dag"); '),
    # Two records that contain each other, and a map of the record itself.
    "folder": ("Folder",
               'const auto& docs = a.field("entries").at(0); '
               'const auto& deep = docs.field("folder").field("value").field("entries").at(0); '
               'const auto& named = a.field("named"); '
               'if (a.field("id").as<int>() != 1 || a.field("entries").size() != 2 || '
               'docs.field("name").as<std::string>() != "docs" || '
               'docs.field("folder").field("value").field("id").as<int>() != 2 || '
               'deep.field("name").as<std::string>() != "deep" || '
               'deep.field("folder").field("value").field("id").as<int>() != 3 || '
               'deep.field("folder").field("value").field("entries").size() != 0 || '
               '!a.field("entries").at(1).field("folder").field("value").is_null() || '
               'named.size() != 1 || named.at(0).field("key").as<std::string>() != "alias" || '
               'named.at(0).field("value").field("id").as<int>() != 4 || '
               'named.at(0).field("value").field("named").at(0).field("value").field("id").as<int>() != 5) '
               'throw std::runtime_error("folder"); '
               'const auto f = e.argument_as<rec::Folder>(0); '
               'if (f.entries.size() != 2 || f.entries[0].folder->entries[0].folder->id != 3 || '
               'f.entries[1].folder || f.named.at("alias").named.at("inner").id != 5) '
               'throw std::runtime_error("typed folder"); '),
}
# The direct route captures public members and standard containers: the by-value tree.
DIRECT = {"tree": ("TreeNode", TREE), "leaf": ("TreeNode", LEAF)}


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "compiler"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="tracer-recursive-") as path:
        root = Path(path)
        env = dict(os.environ, HOME=str(root))
        env.pop("CLANG_TRACER_ENDPOINT", None)
        env["TRACER_PREPARE_EXECUTABLE"] = str(
            Path(args.repl).with_name("clang-tracer-prepare-injectors-native"))

        def run(argv, script=None):
            result = subprocess.run(argv, input=script, cwd=root, env=env,
                                    capture_output=True, text=True, timeout=500)
            assert result.returncode == 0, result.stdout + result.stderr
            return result.stdout

        (root / "recursive.hpp").write_text(HEADER)
        (root / "module.cpp").write_text(MODULE)
        (root / "target.cpp").write_text(TARGET)
        (root / "interceptor.cpp").write_text(INTERCEPTOR)
        flags = ["-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer", "-fcf-protection=none"]
        run([args.compiler, *flags, "-fPIC", "-shared", "module.cpp", "-o", "librec.so"])
        run([args.compiler, *flags, "target.cpp", "-L.", "-lrec", "-pthread",
             "-Wl,-rpath,$ORIGIN", "-o", "target"])
        (root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(root), "file": str(root / "module.cpp"),
            "arguments": [args.compiler, *flags, "-fPIC", "-c", str(root / "module.cpp"),
                          "-o", str(root / "module.o")]}]))
        (root / "pkg").mkdir()

        def trace(route, cases):
            with subprocess.Popen([root / "target"], cwd=root, stdout=subprocess.PIPE,
                                  text=True) as target:
                try:
                    ready = re.fullmatch(r"ready pid=\d+ first=(0x[0-9a-f]+) second=(0x[0-9a-f]+)",
                                         target.stdout.readline().strip())
                    assert ready, "target did not start"
                    script, names = "", {}
                    for name, (record, _) in cases.items():
                        if route == "prepared":
                            selector = f"rec::f_{name}(const rec::{record} &)"
                            script += (f'%probe prepare "{selector}" --include {root}/recursive.hpp '
                                       f"--output pkg/{name}\n"
                                       f'%probe inject "{selector}" --callback {root}/interceptor.cpp '
                                       f"--prepared pkg/{name} --prepared-mode require\n")
                            names[name] = selector
                        else:
                            script += (f'%probe inject "rec::f_{name}" --callback {root}/interceptor.cpp '
                                       f"--include {root}/recursive.hpp\n")
                            names[name] = f"rec::f_{name}(const rec::{record}&)"
                    for name, (_, body) in cases.items():
                        body = body.replace("@FIRST@", ready[1]).replace("@SECOND@", ready[2])
                        script += (
                            f'%collect --name "{names[name]}" --phase POST --count 2 --timeout 60 '
                            f"--as got_{name}\n"
                            f'{{ auto got = tracer.batch("got_{name}"); '
                            f'if (got.size() < 2) throw std::runtime_error("too few events: {name}"); '
                            f"for (const auto& e : got) {{ const auto a = e.argument_snapshot(0); {body}}} "
                            f'std::cout << "CASE {name} " << got.size() << " VALUES_OK\\n"; }}\n')
                    transcript = run([args.repl, "--fail-fast", "--server", args.server,
                                      "--agent", args.agent, "--attach", str(target.pid),
                                      "--timeout", "120"], script + "%detach\n%quit\n")
                    for name in cases:
                        assert f"CASE {name} " in transcript, transcript
                    assert transcript.count("VALUES_OK") == len(cases), transcript
                    assert target.poll() is None, f"target stopped during {route} tracing"
                    # The target checks that its structures are what they were and leaves.
                    target.send_signal(signal.SIGTERM)
                    assert target.stdout.readline().strip() == "intact", "target data changed"
                    assert target.wait(timeout=20) == 0
                finally:
                    if target.poll() is None:
                        target.kill()
                        target.wait(timeout=20)

        trace("prepared", CASES)
        trace("direct", DIRECT)


if __name__ == "__main__":
    main()
