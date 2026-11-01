#!/usr/bin/env python3
"""Resolve and add a probe, then stream a bounded number of live events."""

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

from clang_tracer import Client, messages


def positive_count(value: str) -> int:
    count = int(value)
    if count <= 0:
        raise argparse.ArgumentTypeError("count must be positive")
    return count


def main() -> None:
    examples = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int, help="PID of the running C++ target")
    parser.add_argument("--endpoint", help="use an existing server at this endpoint")
    parser.add_argument("--selector", default="add(int, int)")
    parser.add_argument(
        "--callback",
        type=Path,
        default=examples / "cpp-interceptors" / "free_function.cpp",
        help="C++ interceptor source (defaults to the add example)",
    )
    parser.add_argument("--count", type=positive_count, default=6)
    parser.add_argument("--after-sequence", type=int, default=0)
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
                print(f"Attached session {session.id} to PID {session.pid}.")
                spec = client.resolve_probe(session.id, args.selector)
                spec.callback_source = args.callback.read_text(encoding="utf-8")
                spec.stack_depth = 16
                spec.print_arguments = True
                spec.print_result = True
                probe = client.add_probe(session.id, spec)
                print(
                    f"Added probe {probe.id}; "
                    f"watching from cursor {args.after_sequence}."
                )

                received = 0
                with client.watch_events(
                    [session.id],
                    after_sequence=args.after_sequence,
                    timeout=args.timeout,
                ) as stream:
                    for event in stream:
                        phase = messages.TracePhase.Name(event.phase)
                        print(
                            f"sequence={event.sequence} phase={phase} "
                            f"call={event.call_id}"
                        )
                        print(
                            "  stack:", [frame.function for frame in event.stack.frames]
                        )
                        received += 1
                        if received == args.count:
                            break
                    print(f"Last cursor: {stream.after_sequence}; events: {received}.")
                print("Event stream cancelled on context exit.")
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
