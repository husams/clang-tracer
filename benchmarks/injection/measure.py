#!/usr/bin/env python3
"""Time real name resolution/AddProbe and verify callbacks plus restoration."""

import argparse
import json
import select
import subprocess
import time
from pathlib import Path

from clang_tracer import Client, ServerProcess, messages

CALLBACK = """
class Interceptor {
public:
  void pre(const Call& call) { call.argument<int>("observed", 0); }
  void post(const Call& call) { call.argument<int>("observed", 0); }
};
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--agent", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 16:
        parser.error("--repeats must be between 1 and 16")
    args.server = args.server.resolve()
    args.agent = args.agent.resolve()
    args.fixture = args.fixture.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    endpoint = f"unix://{args.output.resolve()}/control.sock"
    # ServerProcess owns only this benchmark's child, including graceful cleanup.
    with (args.output / "target.log").open("w") as target_log:
        target = subprocess.Popen(
            [str(args.fixture / "target"), str(args.fixture / "module.so")],
            stdout=subprocess.PIPE,
            stderr=target_log,
            text=True,
            start_new_session=True,
        )
        try:
            if (
                target.stdout is None
                or not select.select([target.stdout], [], [], 10)[0]
            ):
                raise RuntimeError("target did not become ready")
            if target.stdout.readline().strip() != f"READY {target.pid}":
                raise RuntimeError("target startup failed")
            manager = ServerProcess(endpoint)
            try:
                manager.start(
                    executable=str(args.server),
                    agent=str(args.agent),
                    operation_timeout=120,
                )
                with Client(endpoint, timeout=120) as client:
                    begin = time.perf_counter()
                    session = client.attach(target.pid)
                    attached = time.perf_counter()
                    rows = []
                    cursor = 0
                    try:
                        for repeat in range(args.repeats):
                            begin_resolve = time.perf_counter()
                            spec = client.resolve_probe(
                                session.id,
                                "injection_target(int)",
                                module=str(args.fixture / "module.so"),
                            )
                            resolved = time.perf_counter()
                            spec.callback_source = CALLBACK
                            probe = client.add_probe(session.id, spec)
                            added = time.perf_counter()
                            events = []
                            with client.watch_events(
                                session_ids=[session.id],
                                after_sequence=cursor,
                                timeout=10,
                            ) as stream:
                                for event in stream:
                                    cursor = event.sequence
                                    if event.probe_id != probe.id:
                                        continue
                                    events.append(event)
                                    if len(events) == 4:
                                        break
                            posts = [
                                event
                                for event in events
                                if event.probe_id == probe.id
                                and event.phase == messages.POST
                            ]
                            if not posts:
                                raise RuntimeError("injected callback produced no POST")
                            for event in posts:
                                value = event.arguments[0].signed_value
                                if event.result.signed_value != value * 3 + 7:
                                    raise RuntimeError(
                                        "callback result disagrees with target"
                                    )
                                if not any(
                                    item.name == "observed"
                                    and item.value.signed_value == value
                                    for item in event.named_arguments
                                ):
                                    raise RuntimeError(
                                        "POST lacks the callback's recorded argument"
                                    )
                            removed = client.remove_probe(session.id, probe.id)
                            if not removed.original_bytes_restored:
                                raise RuntimeError("probe bytes were not restored")
                            if target.poll() is not None:
                                raise RuntimeError("target exited during injection")
                            row = {
                                "repeat": repeat,
                                "pid": target.pid,
                                "resolve_ms": (resolved - begin_resolve) * 1000,
                                "add_ms": (added - resolved) * 1000,
                                "injection_ms": (added - begin_resolve) * 1000,
                                "verified_posts": len(posts),
                                "restored": True,
                            }
                            rows.append(row)
                            print(json.dumps(row), flush=True)
                    finally:
                        client.detach(session.id)
                    (args.output / "timings.json").write_text(
                        json.dumps(
                            {
                                "attach_ms": (attached - begin) * 1000,
                                "rows": rows,
                                "server": str(args.server),
                                "agent": str(args.agent),
                                "fixture": str(args.fixture),
                            },
                            indent=2,
                        )
                        + "\n"
                    )
            finally:
                (args.output / "server.log").write_text(manager.diagnostics())
                manager.shutdown(timeout=120)
        finally:
            if target.poll() is None:
                target.terminate()
            target.wait(timeout=10)
            if target.stdout is not None:
                target.stdout.close()


if __name__ == "__main__":
    main()
