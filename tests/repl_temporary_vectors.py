"""Capture fresh vector prvalues, then keep using vectors in the same REPL."""
import os
import re
import subprocess
import sys
import tempfile


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
            with tempfile.TemporaryDirectory(prefix="tracer-repl-vectors-") as directory:
                env = dict(os.environ, LLVM_PROFILE_FILE="/dev/null", NO_COLOR="1",
                           XDG_CACHE_HOME=directory)
                env.pop("CLANG_TRACER_ENDPOINT", None)
                run = subprocess.run([repl, "--fail-fast"], input=commands,
                                     text=True, capture_output=True, timeout=45, env=env)
            assert run.returncode == 0, (expression, include, run.stdout, run.stderr)
            assert "vectors-ok" in run.stdout, (run.stdout, run.stderr)
            assert "Symbols not found" not in run.stderr, run.stderr
            bare = run.stdout.split("bare-begin\n", 1)[1].split("bare-end\n", 1)[0]
            assert re.search(r"\(std::vector<int[^\n]*\) @?0x[0-9a-fA-F]+", bare), bare
            silent = run.stdout.split("semicolon-begin\n", 1)[1].split("semicolon-end\n", 1)[0]
            assert silent == "", silent
            named = re.findall(r"\(std::vector<int[^\n]*&\) @?0x[0-9a-fA-F]+", run.stdout)
            assert len(named) == 2, run.stdout
    print("fresh vector expressions, output, reuse and temporary lifetimes passed")


if __name__ == "__main__":
    main()
