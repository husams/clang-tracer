"""Check vector addresses, scalar expressions, cleanup and same-session reuse."""
import os
import re
import subprocess
import sys
import tempfile


def run_session(repl, commands, *, fail_fast=True, expected_status=0):
    with tempfile.TemporaryDirectory(prefix="tracer-repl-vectors-") as directory:
        env = dict(os.environ, LLVM_PROFILE_FILE="/dev/null", NO_COLOR="1",
                   XDG_CACHE_HOME=directory)
        env.pop("CLANG_TRACER_ENDPOINT", None)
        argv = [repl] + (["--fail-fast"] if fail_fast else [])
        run = subprocess.run(argv, input=commands, text=True,
                             capture_output=True, timeout=45, env=env)
    assert run.returncode == expected_status, (run.stdout, run.stderr)
    assert "Symbols not found" not in run.stderr, run.stderr
    assert "Failed to materialize" not in run.stderr, run.stderr
    return run


def main():
    repl = os.path.abspath(sys.argv[1])
    for expression in ("std::vector<int>{1,2,3}", "std::vector<int>(3)"):
        for include in ("", "#include <vector>\n"):
            commands = include + f'''std::cout << "bare-begin\\n";
{expression}
std::cout << "bare-end\\n";
auto v = std::vector<int>{{4,5}};
v
v.push_back(6);
if (v.size() != 3 || v[0] != 4 || v[2] != 6) throw std::runtime_error("vector state lost");
std::cout << "semicolon-begin\\n";
std::vector<int>{{1,2,3}};
std::vector<int>(3);
std::cout << "semicolon-end\\n";
std::vector<int> w{{1,2,3}};
w
if (w.size() != 3 || w[1] != 2) throw std::runtime_error("named vector state lost");
int alive = 0; int made = 0;
struct Tracked {{ Tracked(int) {{ ++alive; ++made; }} Tracked(const Tracked&) {{ ++alive; ++made; }} ~Tracked() {{ --alive; }} }};
std::vector<Tracked>{{Tracked{{1}}, Tracked{{2}}}}
if (alive != 0 || made != 4) throw std::runtime_error("temporary lifetime or evaluation count changed");
std::vector<Tracked>{{Tracked{{3}}, Tracked{{4}}}}.size()
if (alive != 0 || made != 8) throw std::runtime_error("member temporary cleanup changed");
long consume_tracked(std::vector<Tracked> values) {{ return values.size(); }}
consume_tracked(std::vector<Tracked>{{Tracked{{5}}, Tracked{{6}}}})
if (alive != 0 || made != 12) throw std::runtime_error("parameter temporary cleanup changed");
std::vector<Tracked> tracked_named{{Tracked{{7}}, Tracked{{8}}}};
consume_tracked(tracked_named)
if (alive != 2 || made != 18) throw std::runtime_error("named parameter cleanup changed");
int virtual_cleanup = 0;
struct VirtualBase {{ ~VirtualBase() {{ ++virtual_cleanup; }} }};
struct Argument : virtual VirtualBase {{ ~Argument() {{}} }};
struct Result {{ Result(const Argument& = Argument{{}}) {{}} ~Result() {{}} }};
Argument{{}};
virtual_cleanup = 0;
Result{{}}
if (virtual_cleanup != 1) throw std::runtime_error("virtual base cleanup changed");
std::cout << "vectors-ok\\n";
%quit
'''
            run = run_session(repl, commands)
            assert "vectors-ok" in run.stdout, (run.stdout, run.stderr)
            bare = run.stdout.split("bare-begin\n", 1)[1].split("bare-end\n", 1)[0]
            assert re.search(r"\(std::vector<int[^\n]*\) @?0x[0-9a-fA-F]+", bare), bare
            silent = run.stdout.split("semicolon-begin\n", 1)[1].split("semicolon-end\n", 1)[0]
            assert silent == "", silent
            named = re.findall(r"\(std::vector<int[^\n]*&\) @?0x[0-9a-fA-F]+", run.stdout)
            assert len(named) == 2, run.stdout

    # Each scalar form is the first vector use in a fresh session: earlier
    # named declarations must not hide the missing destructor definition.
    for prelude, expression, effects in (
        ("", "std::vector<int>{1,2}.size()", ""),
        ("int calls = 0;\nlong f(std::vector<int> values) { ++calls; return values.size(); }\n",
         "f(std::vector<int>{1,2})", "if (calls != 1) throw std::runtime_error(\"evaluated twice\");"),
        ("int calls = 0;\nlong f(std::vector<int>) { ++calls; return 2; }\nstd::vector<int> n{1,2};\n",
         "f(n)", "if (calls != 1 || n.size() != 2) throw std::runtime_error(\"named argument changed\");"),
    ):
        run = run_session(repl, "#include <vector>\n" + prelude + f'''std::cout << "scalar-begin\\n";
{expression}
std::cout << "scalar-end\\n";
{effects}
std::vector<int> v{{4,5}};
v.push_back(6);
if (v.size() != 3 || v[0] != 4 || v[2] != 6) throw std::runtime_error("session poisoned");
std::cout << "scalar-reuse-ok\\n";
%quit
''')
        scalar = run.stdout.split("scalar-begin\n", 1)[1].split("scalar-end\n", 1)[0]
        assert re.search(r"\([^\n]+\) 2\s*$", scalar), scalar
        assert "scalar-reuse-ok" in run.stdout, run.stdout

    run = run_session(repl, '''#include <vector>
int attempts = 0;
long throwing(std::vector<int>) { ++attempts; throw std::runtime_error("expected-vector-throw"); }
auto retry = throwing(std::vector<int>{1,2});
auto retry = 42L;
if (retry != 42 || attempts != 1) throw std::runtime_error("initializer recovery changed");
int destroyed = 0;
struct Outside { ~Outside(); int size() const { return 1; } };
Outside::~Outside() { ++destroyed; }
Outside{}.size()
if (destroyed != 1) throw std::runtime_error("out-of-line destructor changed");
std::cout << "rollback-ok\\n";
%quit
''', fail_fast=False, expected_status=1)
    assert "expected-vector-throw" in run.stderr, run.stderr
    assert re.findall(r"^.*error:.*$", run.stderr, re.MULTILINE) == [
        "error: expected-vector-throw"], run.stderr
    assert "rollback-ok" in run.stdout, (run.stdout, run.stderr)
    print("vector addresses, scalar expressions, parameter cleanup and recovery passed")


if __name__ == "__main__":
    main()
