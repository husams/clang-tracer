#!/usr/bin/env python3
"""Record live TraceEvent frames and read the length-prefixed file back."""

import argparse
import shutil
import struct
import sys
import tempfile
from collections.abc import Iterator
from pathlib import Path

from clang_tracer import Client, messages

MAX_FRAME_SIZE = 32 * 1024 * 1024


def positive_count(value: str) -> int:
    count = int(value)
    if count <= 0:
        raise argparse.ArgumentTypeError("count must be positive")
    return count


def read_recording(path: Path) -> Iterator[messages.TraceEvent]:
    with path.open("rb") as recording:
        while True:
            header = recording.read(4)
            if not header:
                return
            if len(header) != 4:
                raise ValueError("truncated TraceEvent length header")
            (size,) = struct.unpack("<I", header)
            if size > MAX_FRAME_SIZE:
                raise ValueError(f"TraceEvent frame exceeds {MAX_FRAME_SIZE} bytes")
            body = recording.read(size)
            if len(body) != size:
                raise ValueError("truncated TraceEvent frame")
            event = messages.TraceEvent()
            event.ParseFromString(body)
            yield event


def main() -> None:
    examples = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int, help="PID of the running C++ target")
    parser.add_argument("output", type=Path, help="new file to create exclusively")
    parser.add_argument("--endpoint", help="use an existing server at this endpoint")
    parser.add_argument("--selector", default="add(int, int)")
    parser.add_argument(
        "--callback",
        type=Path,
        default=examples / "cpp-interceptors" / "free_function.cpp",
        help="C++ interceptor source (defaults to the add example)",
    )
    parser.add_argument("--count", type=positive_count, default=6)
    parser.add_argument("--timeout", type=float, default=10)
    args = parser.parse_args()

    temporary = tempfile.TemporaryDirectory(prefix="clang-tracer-python-", delete=False)
    endpoint = args.endpoint or f"unix://{temporary.name}/control.sock"
    client = None
    try:
        with Client(endpoint) as client:
            if args.endpoint is None:
                client.start_server()

            session = None
            probe = None
            try:
                session = client.attach(args.pid)
                probe_spec = client.resolve_probe(session.id, args.selector)
                probe_spec.callback_source = args.callback.read_text(encoding="utf-8")
                probe = client.add_probe(session.id, probe_spec)
                print(f"Recording events for {args.selector} into {args.output}.")
                written = client.record_traces(
                    args.output,
                    args.count,
                    session_ids=[session.id],
                    timeout=args.timeout,
                )
                print(f"Recorded {written} events.")
                for event in read_recording(args.output):
                    phase = messages.TracePhase.Name(event.phase)
                    print(f"phase={phase} sequence={event.sequence}")
            finally:
                try:
                    if session is not None and probe is not None:
                        client.remove_probe(session.id, probe.id)
                        print(f"Removed probe {probe.id}.")
                finally:
                    if session is not None:
                        client.detach(session.id)
                        print(f"Detached session {session.id}.")
    finally:
        manager = client.server if client is not None else None
        if manager is not None and manager.running:
            print(
                f"Managed server is still running (PID {manager.pid}); "
                f"recovery endpoint: {endpoint}; "
                f"directory: {temporary.name}",
                file=sys.stderr,
            )
        else:
            shutil.rmtree(temporary.name)


if __name__ == "__main__":
    main()
