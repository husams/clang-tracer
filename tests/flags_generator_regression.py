#!/usr/bin/env python3
"""Regress standalone preparation of nested constexpr/template dependencies."""

import json
import pathlib
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: flags_generator_regression.py PREPARER CXX")

    preparer = pathlib.Path(sys.argv[1]).resolve()
    compiler = sys.argv[2]
    with tempfile.TemporaryDirectory(prefix="clang-tracer-flags-") as temp:
        root = pathlib.Path(temp)
        app = root / "application"
        app.mkdir()
        header = app / "types.hpp"
        header.write_text(
            "struct Flags { static constexpr int Off = 0; };\n"
            "struct Holder { Flags flags; };\n"
            "template<int Mode = Flags::Off> struct Config {};\n"
            "struct Result { Holder first; Config<> second; };\n"
            "Result make_result();\n"
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
                            "-std=c++17",
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
        (output / "verify.cpp").write_text(
            '#include "generated/adapter.cpp"\n'
            "static_assert(Flags::Off == 0);\n"
            "Config<>& check_default(Result& result) { return result.second; }\n"
        )
        subprocess.run(
            [compiler, "-std=c++17", "-fsyntax-only", "verify.cpp"],
            cwd=output,
            check=True,
        )


if __name__ == "__main__":
    main()
