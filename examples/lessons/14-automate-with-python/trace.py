#!/usr/bin/env python3
"""Attach to the lesson 1 target, install hook.cpp, print a few events, clean up."""

import sys
import tempfile
from pathlib import Path

from clang_tracer import Client, messages
from google.protobuf import text_format

HERE = Path(__file__).resolve().parent


def main() -> None:
    pid = int(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix="lesson14-") as run_dir, Client(
        f"unix://{run_dir}/control.sock"
    ) as client:
        client.start_server()  # a private server for this script
        session = client.attach(pid)
        probe = client.inject(
            session.id,
            "add(int, int)",
            (HERE / "hook.cpp").read_text(encoding="utf-8"),
            print_arguments=True,
            print_result=True,
        )
        print(f"probe {probe.id} installed; press Enter in the target twice")
        try:
            for event in client.collect_traces(4, session_ids=[session.id], timeout=30):
                phase = messages.TracePhase.Name(event.phase)
                print(f"{phase} call={event.call_id}")
                for field in event.object_fields:
                    value = text_format.MessageToString(field.value, as_one_line=True)
                    print(f"  {field.name} = {value}")
        finally:
            client.remove_probe(session.id, probe.id)
            client.detach(session.id)


if __name__ == "__main__":
    main()
