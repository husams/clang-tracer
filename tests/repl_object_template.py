"""Prepare, preview, start and replay watches of scoped template specializations."""

import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import subprocess
import tempfile

from object_watch_template import CASES, Output, send, stop


def run(args):
    with tempfile.TemporaryDirectory(prefix="ct-template-repl-") as directory, ExitStack() as cleanup:
        root = Path(directory)

        def start(argv):
            process = subprocess.Popen(argv, cwd=root, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            cleanup.callback(stop, process)
            return process, Output(process.stdout, 30)

        repl, out = start([args.repl, "--server", args.server, "--agent", args.agent, "--fail-fast"])
        for index, (concrete, increment) in enumerate(CASES):
            package = root / f"plan-{index}"
            send(repl, f'%object prepare "{concrete}" --module "{args.fixture}" --field state_ '
                       f'--code "{concrete}::set(int)" --first-seen --synchronization thread-confined '
                       f'--backend {args.backend} --output {package}')
            result = out.expect(r"^object package=|^error:")
            assert result.string.startswith("object package="), result.string
            out.expect(r"^binding=offline$")
            target, app = start([args.fixture])
            app.expect(r"^READY$")
            send(repl, f"%attach {target.pid}")
            out.expect(rf"^attached session=\S+ pid={target.pid}$")
            send(repl, f"%object preview --plan {package}")
            out.expect(r"^binding=same-build$")
            out.expect(r'^field=state_ encoding="signed int32" offset=0 ')
            send(repl, f"%object start --plan {package}")
            watch = out.expect(r"^watch=(\d+) active=yes .*|^error:")
            assert watch[1], watch.string
            send(repl, f"%record {index}.db --sqlite --name object.trace --probe {watch[1]} --count 6 --timeout 20")
            for _ in range(2):
                send(target, "g")
                app.expect(r"^WROTE$")
            out.expect(r"^received=6 .*complete=true")
            send(repl, f"%object stop {watch[1]}")
            out.expect(rf"^watch={watch[1]} active=no .*restoration_pending=no ")
            send(repl, "%detach")
            out.expect(r"^detached session=")
            send(repl, f"%events open {index}.db --as saved{index}")
            out.expect(r"^opened events=6 retained=6 evicted=0$")
            send(repl, f"%events export saved{index} --format json --output {index}.json")
            # A synchronous command after export makes the output file ready to inspect.
            send(repl, f'std::cout << "EXPORTED_{index}" << std::endl;')
            out.expect(rf"^EXPORTED_{index}$")
            observations = [row["objectObservation"] for row in json.loads((root / f"{index}.json").read_text())]
            assert [item["kind"] for item in observations] == [
                "FIRST_SEEN", "FIELD_CHANGED", "OBSERVATION_ENDED"] * 2, observations
            assert [(item["oldInteger"], item["newInteger"]) for item in observations
                    if item["kind"] == "FIELD_CHANGED"] == [
                        (0, 20 + increment), (20 + increment, 39 + increment)], observations
            send(target, "g")
            app.expect(r"^WROTE$")
            target.stdin.close()
            assert target.wait(timeout=10) == 0
            print(f"{concrete}: {args.backend} prepare, preview, capture and replay passed", flush=True)
        send(repl, "%quit")
        assert repl.wait(timeout=15) == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "fixture"):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument("--backend", choices=("ptrace", "code-cache"), default="code-cache")
    run(parser.parse_args())
