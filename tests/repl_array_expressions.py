"""Reject unsupported array value capture before it changes the live session."""
import os
import re
import subprocess
import sys
import tempfile


DIAGNOSTIC = "array expressions cannot be printed; add ';' or use explicit output"


def main():
    repl = os.path.abspath(sys.argv[1])
    expressions = (
        '"banking"',
        '""',
        '("bank" "ing")',
        'R"tag(bank;ing)tag"',
        'L"banking"',
        'u"banking"',
        'U"banking"',
        'u8"banking"',
        'BANKING_LABEL',
        'label',
        'numbers',
        'matrix',
        'array_ref()',
        '(++attempts, numbers)',
        'true ? numbers : other',
        'int rejected_declaration = 99; "banking"',
    )
    commands = '''#define BANKING_LABEL "banking"
int preserved = 40;
int attempts = 0;
char label[] = "banking";
int numbers[] = {1, 2};
int other[] = {3, 4};
int matrix[2][2] = {{1, 2}, {3, 4}};
int (&array_ref())[2] { ++attempts; return numbers; }
'''
    for index, expression in enumerate(expressions):
        commands += expression + f'''
1 + 1
preserved + 2
if (preserved != 40 || attempts != 0 || numbers[0] != 1 || label[0] != 'b') throw std::runtime_error("array rejection changed session state");
std::cout << "array-reuse-{index}\\n";
'''
    commands += '''int rejected_declaration = 42;
if (rejected_declaration != 42) throw std::runtime_error("rejected declaration leaked");
std::cout << "explicit-begin\\n";
"banking";
numbers;
(++attempts, numbers);
std::cout << "explicit-end\\n";
if (attempts != 1) throw std::runtime_error("terminated array was not evaluated once");
std::cout << label << '\\n';
std::string("banking").size()
std::vector<int>{1, 2, 3}
std::vector<int>{1, 2}.size()
numbers[0] = 5;
if (numbers[0] != 5 || preserved != 40) throw std::runtime_error("session no longer writable");
std::cout << "arrays-ok\\n";
%quit
'''
    with tempfile.TemporaryDirectory(prefix="tracer-repl-arrays-") as directory:
        env = dict(os.environ, LLVM_PROFILE_FILE="/dev/null", NO_COLOR="1",
                   XDG_CACHE_HOME=directory)
        env.pop("CLANG_TRACER_ENDPOINT", None)
        run = subprocess.run([repl], input=commands, text=True,
                             capture_output=True, timeout=90, env=env)
    assert run.returncode == 1, (run.stdout, run.stderr)
    assert run.stderr.count(DIAGNOSTIC) == len(expressions), run.stderr
    errors = re.findall(r"^.*error:.*$", run.stderr, re.MULTILINE)
    assert len(errors) == len(expressions) and all(DIAGNOSTIC in error for error in errors), run.stderr
    for forbidden in ("operator new[]", "diverged", "%cpp reset",
                      "Symbols not found", "Failed to materialize"):
        assert forbidden not in run.stderr, run.stderr
    for index in range(len(expressions)):
        assert f"array-reuse-{index}\n" in run.stdout, (run.stdout, run.stderr)
    assert len(re.findall(r"^\(int\) 2$", run.stdout, re.MULTILINE)) == len(expressions), run.stdout
    assert len(re.findall(r"^\(int\) 42$", run.stdout, re.MULTILINE)) == len(expressions), run.stdout
    silent = run.stdout.split("explicit-begin\n", 1)[1].split("explicit-end\n", 1)[0]
    assert silent == "", silent
    assert "\nbanking\n" in run.stdout, run.stdout
    assert "arrays-ok\n" in run.stdout, (run.stdout, run.stderr)
    print("array rejection, arithmetic, retained variables and explicit output passed")


if __name__ == "__main__":
    main()
