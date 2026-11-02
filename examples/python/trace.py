#!/usr/bin/env python3
"""Attach, inject a C++ interceptor, collect events, then remove and detach."""

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

from clang_tracer import Client, messages
from google.protobuf import text_format


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
                print("Sessions:")
                for listed_session in client.list_sessions():
                    print(f"  {listed_session.id}: PID {listed_session.pid}")

                callback_source = args.callback.read_text(encoding="utf-8")
                probe = client.inject(
                    session.id,
                    args.selector,
                    callback_source,
                    print_arguments=True,
                    print_result=True,
                )
                print(f"Installed probe {probe.id} for {args.selector}.")

                events = client.collect_traces(
                    args.count,
                    session_ids=[session.id],
                    timeout=args.timeout,
                )
                for event in events:
                    phase = messages.TracePhase.Name(event.phase)
                    print(
                        f"sequence={event.sequence} phase={phase} "
                        f"call={event.call_id} probe={event.probe_id}"
                    )
                    for argument in event.named_arguments:
                        value = text_format.MessageToString(
                            argument.value, as_one_line=True
                        )
                        print(f"  argument {argument.name}: {value}")
                    if event.phase == messages.POST:
                        value = text_format.MessageToString(
                            event.result, as_one_line=True
                        )
                        print(f"  result: {value}")
                    for field in event.object_fields:
                        value = text_format.MessageToString(
                            field.value, as_one_line=True
                        )
                        print(f"  field {field.name}: {value}")
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
