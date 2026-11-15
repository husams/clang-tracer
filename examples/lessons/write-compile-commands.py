import json
from pathlib import Path
import shutil
import sys


def main():
    output = Path(sys.argv[1])
    separator = sys.argv.index("--")
    sources = [Path(source) for source in sys.argv[2:separator]]
    command = sys.argv[separator + 1:]
    compiler = shutil.which(command[0])
    if compiler:
        command[0] = str(Path(compiler).absolute())

    directory = Path.cwd()
    entries = []
    for source in sources:
        object_file = output.parent / source.with_suffix(".o")
        entries.append({
            "directory": str(directory),
            "file": str(directory / source),
            "arguments": command + ["-c", str(source), "-o", str(object_file)],
        })

    contents = json.dumps(entries, indent=2) + "\n"
    if not output.exists() or output.read_text() != contents:
        output.write_text(contents)


if __name__ == "__main__":
    main()
