#!/usr/bin/env python3
"""Exercise standalone generation for late declaration dependencies."""

import json
import pathlib
import subprocess
import sys
import tempfile


def run_case(preparer, compiler, case):
    with tempfile.TemporaryDirectory(prefix="clang-tracer-") as temp:
        root = pathlib.Path(temp)
        app = root / "application"
        app.mkdir()
        header = app / "types.hpp"
        if case == "nested-dependency":
            header.write_text(
                "struct Container { struct Nested { int value; }; };\n"
                "struct First { Container value; };\n"
                "struct Later { Container::Nested value; };\n"
                "struct Result { First first; Later later; };\n"
                "Result make_result();\n"
            )
            verify = (
                '#include "generated/adapter.cpp"\n'
                "#include <cstddef>\n"
                "static_assert(sizeof(Container::Nested) == sizeof(int));\n"
                "static_assert(sizeof(decltype(Later::value)) == sizeof(int));\n"
                "static_assert(offsetof(Result, later) > offsetof(Result, first));\n"
                "int check_nested_layout(Result& result) {\n"
                "  return result.later.value.value;\n"
                "}\n"
            )
        else:
            header.write_text(
                "struct Flags { static constexpr int Off = 0; };\n"
                "struct Holder { Flags flags; };\n"
                "template<int Mode = Flags::Off> struct Config {};\n"
                "struct Result { Holder first; Config<> second; };\n"
                "Result make_result();\n"
            )
            verify = (
                '#include "generated/adapter.cpp"\n'
                "static_assert(Flags::Off == 0);\n"
                "Config<>& check_default(Result& result) { return result.second; }\n"
            )
        source = app / "target.cpp"
        source.write_text(
            '#include "types.hpp"\n'
            "Result make_result() { return {}; }\n"
        )
        compdb = app / "compile_commands.json"
        compdb.write_text(
            json.dumps(
                [
                    {
                        "directory": str(app),
                        "file": str(source),
                        "arguments": [
                            compiler,
                            "-std=c++23",
                            "-I",
                            str(app),
                            "-c",
                            str(source),
                        ],
                    }
                ]
            )
        )
        output = root / "package"
        subprocess.run(
            [
                str(preparer),
                "--selector",
                "make_result()",
                "--include",
                str(header),
                "--compdb",
                str(compdb),
                "--output",
                str(output),
            ],
            check=True,
        )

        # Compile the generated adapter without the application's declarations.
        header.unlink()
        (output / "verify.cpp").write_text(verify)
        subprocess.run(
            [compiler, "-std=c++23", "-fsyntax-only", "verify.cpp"],
            cwd=output,
            check=True,
        )


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(
            "usage: flags_generator_regression.py PREPARER CXX [flags|nested-dependency]"
        )

    preparer = pathlib.Path(sys.argv[1]).resolve()
    compiler = sys.argv[2]
    case = sys.argv[3] if len(sys.argv) == 4 else "flags"
    if case not in ("flags", "nested-dependency"):
        raise SystemExit(f"unknown regression case: {case}")
    run_case(preparer, compiler, case)


if __name__ == "__main__":
    main()
