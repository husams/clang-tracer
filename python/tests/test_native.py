"""Opt-in tests against rebuilt native server/agent and real target processes."""

import os
import platform
import signal
import sqlite3
import subprocess
import time
from pathlib import Path

import grpc
import pytest
from prompt_toolkit.output import DummyOutput

from clang_tracer import Client
from clang_tracer import messages as pb
from clang_tracer.command import parse_command
from clang_tracer.console import Console

pytestmark = pytest.mark.native


def test_native_retention_snapshot_ack_and_cli_storage(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock") as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
            event_history_limit=3,
        )
        session = client.attach(target.pid)
        probe = client.inject(session.id, "add(int, int)", INTERCEPTOR)
        client.collect_traces(12, session_ids=[session.id], timeout=10)
        client.remove_probe(session.id, probe.id)
        batch = client.read_events()
        assert len(batch.events) == 3
        assert batch.server_dropped_events >= 9
        with pytest.raises(grpc.RpcError) as raised:
            client.read_events(
                through_sequence=2**64 - 1,
                server_instance_id=batch.server_instance_id,
            )
        assert raised.value.code() == grpc.StatusCode.INVALID_ARGUMENT
        assert all(
            e.server_instance_id == batch.server_instance_id for e in batch.events
        )
        with pytest.raises(grpc.RpcError) as raised:
            client.acknowledge_events("previous-server", [batch.events[0].sequence])
        assert raised.value.code() == grpc.StatusCode.FAILED_PRECONDITION
        assert client.read_events() == batch
        # Saving through Python leaves retained history available to the native writer.
        path = temporary / "capture.db"
        assert client.record_sqlite(path, 3, timeout=5) == 3
        assert len(client.read_events().events) == 3
        with sqlite3.connect(path) as db:
            db.execute(
                "CREATE TRIGGER reject_capture BEFORE INSERT ON trace_events "
                "BEGIN SELECT RAISE(ABORT, 'native storage failure'); END"
            )
        result = subprocess.run(
            [
                build / "clang-tracer",
                "--endpoint",
                client.endpoint,
                "drain",
                "--sqlite",
                path,
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode != 0
        assert "native storage failure" in result.stderr
        assert len(client.read_events().events) == 3
        with sqlite3.connect(path) as db:
            db.execute("DROP TRIGGER reject_capture")
        result = subprocess.run(
            [
                build / "clang-tracer",
                "--endpoint",
                client.endpoint,
                "drain",
                "--sqlite",
                path,
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode == 0, result.stderr
        assert "drained 3 events" in result.stdout
        assert not client.read_events().events
        with sqlite3.connect(path) as db:
            rows = db.execute("SELECT payload FROM trace_events").fetchall()
        assert (
            len(rows) == 3
        )  # Both writers use the same identity/deduplication format.
        assert {pb.TraceEvent.FromString(r[0]).sequence for r in rows} == {
            e.sequence for e in batch.events
        }
        assert (
            client.acknowledge_events(
                batch.server_instance_id, [batch.events[0].sequence]
            )
            == 0
        )
        with client.watch_events(
            after_sequence=batch.events[0].sequence, timeout=1
        ) as stream:
            with pytest.raises(grpc.RpcError) as raised:
                next(stream)
        assert raised.value.code() == grpc.StatusCode.OUT_OF_RANGE
        # An interrupted live drain leaves the independently managed server up.
        child = subprocess.Popen(
            [
                build / "clang-tracer",
                "--endpoint",
                client.endpoint,
                "drain",
                "--sqlite",
                path,
                "--listen",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            time.sleep(0.2)
            child.send_signal(signal.SIGINT)
            _output, error = child.communicate(timeout=5)
            assert child.returncode == 0, error
            assert client.status().running
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=5)
        client.detach(session.id)


def test_native_cli_live_storage_and_console_drain(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock") as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        probe = client.inject(session.id, "add(int, int)", INTERCEPTOR)
        path = temporary / "live.db"
        result = subprocess.run(
            [
                build / "clang-tracer",
                "--endpoint",
                client.endpoint,
                "drain",
                str(target.pid),
                "--sqlite",
                path,
                "--listen",
                "--count",
                "6",
                "--timeout",
                "10",
            ],
            capture_output=True,
            text=True,
            timeout=15,
        )
        assert result.returncode == 0, result.stderr
        assert "drained 6 events" in result.stdout
        watch_path = temporary / "watch.db"
        result = subprocess.run(
            [
                build / "clang-tracer",
                "--endpoint",
                client.endpoint,
                "events",
                str(target.pid),
                "--sqlite",
                watch_path,
                "--count",
                "3",
                "--timeout",
                "10",
            ],
            capture_output=True,
            text=True,
            timeout=15,
        )
        assert result.returncode == 0, result.stderr
        client.remove_probe(session.id, probe.id)
        console = Console(client, output=DummyOutput())
        console.session_id = session.id
        console.execute(parse_command(f'drain "{path}"'))
        assert not client.read_events().events
        for database, minimum in [(path, 6), (watch_path, 3)]:
            with sqlite3.connect(database) as db:
                assert (
                    db.execute("SELECT count(*) FROM trace_events").fetchone()[0]
                    >= minimum
                )
        client.detach(session.id)


@pytest.mark.parametrize("context_bytes, expected", [(16777216, True), (1, False)])
def test_native_context_history_limits(native, context_bytes, expected):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock") as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
            context_history_limit=2,
            context_history_bytes=context_bytes,
        )
        session = client.attach(target.pid)
        probe = client.inject(session.id, "add(int, int)", CONTEXT_INTERCEPTOR)
        if expected:
            client.collect_traces(16, session_ids=[session.id], timeout=10)
        else:
            # Context probes publish via the context transport, so a one-byte
            # budget can reject every event, including PRE/POST notifications.
            deadline = time.monotonic() + 10
            while client.read_events().server_dropped_events < 16:
                assert time.monotonic() < deadline
                assert target.poll() is None
                time.sleep(0.05)
        client.remove_probe(session.id, probe.id)
        batch = client.read_events()
        contexts = [event for event in batch.events if event.HasField("context")]
        assert len(contexts) <= 2
        assert bool(contexts) is expected
        assert sum(e.ByteSize() for e in contexts) <= context_bytes
        assert batch.server_dropped_events > 0
        client.detach(session.id)


@pytest.mark.parametrize("value", ["0", "-1", "18446744073709551615", "1x"])
def test_native_invalid_retention_limit(native, value):
    build, _target, _temporary = native
    result = subprocess.run(
        [build / "clang-tracer-server", "--event-history-limit", value],
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert result.returncode != 0
    assert "invalid value" in result.stderr


INTERCEPTOR = """class Interceptor {
public:
  void pre(const Call &call) { call.argument<int>(0); }
  void post(const Call &call) { call.argument<int>(0); }
};
"""

CONTEXT_INTERCEPTOR = """class Interceptor {
public:
  void pre(const Call &call) { call.context().event("context.add"); }
};
"""


@pytest.fixture
def native(tmp_path):
    configured = os.getenv("CLANG_TRACER_NATIVE_BUILD")
    if not configured or platform.system() != "Linux":
        pytest.skip("set CLANG_TRACER_NATIVE_BUILD to a Linux native build directory")
    build = Path(configured).resolve()
    with (tmp_path / "target.log").open("w+b") as log:
        target = subprocess.Popen(
            [build / "cpp-interceptors-target"], stdout=log, stderr=log
        )
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if target.poll() is not None:
                    pytest.fail("native target exited before readiness")
                if b"TARGET pid=" in os.pread(log.fileno(), 4096, 0):
                    break
                time.sleep(0.02)
            else:
                pytest.fail("native target did not become ready")
            yield build, target, tmp_path
        finally:
            target.terminate()
            target.wait(timeout=5)


def test_real_native_api_resolution_injection_and_shutdown_restore(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock", timeout=30) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        assert client.status().running and client.status().managed
        session = client.attach(target.pid)
        spec = client.resolve_probe(session.id, "add(int, int)")
        assert spec.signature.result == pb.INT32
        assert list(spec.signature.arguments) == [pb.INT32, pb.INT32]
        assert spec.debug_info.runtime_entry
        probe = client.inject(session.id, "add(int, int)", INTERCEPTOR, stack_depth=16)
        events = client.collect_traces(8, session_ids=[session.id], timeout=10)
        assert {event.phase for event in events} == {pb.PRE, pb.POST}
        assert all(event.probe_id == probe.id for event in events)
        assert any(event.stack.frames for event in events)
        assert (
            client.record_traces(
                temporary / "capture.pb", 4, session_ids=[session.id], timeout=10
            )
            == 4
        )
        assert client.remove_probe(session.id, probe.id).original_bytes_restored
        method = client.inject(
            session.id, "Calculator::multiply(int, int) const", INTERCEPTOR
        )
        assert method.active
        with client.watch_events(
            [session.id], after_sequence=events[-1].sequence, timeout=10
        ) as stream:
            for event in stream:
                if event.probe_id == method.id:
                    assert event.object_address
                    break
            else:
                pytest.fail("method event stream ended early")
        # Leave a live probe for the server's native shutdown restoration path.
        client.shutdown_server()
        assert target.poll() is None
        assert not client.status(timeout=0.1).running
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        probe = client.inject(
            session.id, "Calculator::multiply(int, int) const", INTERCEPTOR
        )
        assert client.collect_traces(2, session_ids=[session.id], timeout=10)
        assert client.remove_probe(session.id, probe.id).original_bytes_restored
        assert client.detach(session.id).state == pb.Session.DETACHED
        with pytest.raises(grpc.RpcError) as detached:
            client.resolve_probe(session.id, "add(int, int)")
        assert detached.value.code() == grpc.StatusCode.FAILED_PRECONDITION
        with pytest.raises(grpc.RpcError) as missing:
            client.resolve_probe("missing", "add(int, int)")
        assert missing.value.code() == grpc.StatusCode.NOT_FOUND


def test_real_native_console_commands(native):
    build, target, temporary = native
    callback = temporary / "callback.cpp"
    callback.write_text(INTERCEPTOR)
    client = Client(f"unix://{temporary}/control.sock")
    console = Console(client, output=DummyOutput())
    try:
        for command in [
            f'server start --executable "{build / "clang-tracer-server"}" '
            f'--agent "{build / "libclang_tracer_agent.so"}"',
            "server status",
            f"attach {target.pid}",
            "sessions",
            'probe resolve "add(int, int)"',
            f'probe inject "add(int, int)" --callback "{callback}"',
            "events --count 4 --timeout 10",
            f'record "{temporary / "console.pb"}" 4 --timeout 10',
            "detach",
            "server stop",
        ]:
            assert console.execute(parse_command(command))
        assert (temporary / "console.pb").stat().st_size > 0
        assert target.poll() is None
    finally:
        client.close()


def test_native_endpoint_lock_reports_path_errors_separately_from_contention(native):
    import fcntl

    build, _, temporary = native
    server = build / "clang-tracer-server"
    agent = build / "libclang_tracer_agent.so"

    missing_parent_endpoint = temporary / "missing-parent" / "control.sock"
    path_failure = subprocess.run(
        [
            server,
            "--agent",
            agent,
            "--listen",
            f"unix://{missing_parent_endpoint}",
        ],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert path_failure.returncode != 0
    assert str(missing_parent_endpoint) + ".lock" in path_failure.stderr
    assert "errno 2" in path_failure.stderr
    assert "No such file or directory" in path_failure.stderr
    assert "another clang-tracer server owns" not in path_failure.stderr

    if os.geteuid() != 0:
        unwritable = temporary / "unwritable"
        unwritable.mkdir(mode=0o500)
        try:
            denied = subprocess.run(
                [
                    server,
                    "--agent",
                    agent,
                    "--listen",
                    f"unix://{unwritable}/control.sock",
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert denied.returncode != 0
            assert "errno 13" in denied.stderr
            assert "Permission denied" in denied.stderr
            assert "another clang-tracer server owns" not in denied.stderr
        finally:
            unwritable.chmod(0o700)

    endpoint_path = temporary / "contended.sock"
    lock_path = Path(f"{endpoint_path}.lock")
    with lock_path.open("w") as held_lock:
        fcntl.flock(held_lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        contention = subprocess.run(
            [server, "--agent", agent, "--listen", f"unix://{endpoint_path}"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert contention.returncode != 0
        assert "another clang-tracer server owns this endpoint" in contention.stderr


def test_real_native_name_filters_cover_function_and_context_history(native):
    build, target, temporary = native
    endpoint = f"unix://{temporary}/control.sock"
    function_name = "add(int, int)"
    context_name = "context.add"
    with Client(endpoint, timeout=30) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        probe = client.inject(session.id, function_name, CONTEXT_INTERCEPTOR)
        selected = client.collect_traces(
            6,
            names=[context_name],
            session_ids=[session.id],
            pid=target.pid,
            timeout=10,
        )
        assert selected
        assert all(event.context.event.name == context_name for event in selected)
        assert all(event.function_name == function_name for event in selected)
        function_events = client.collect_traces(
            4,
            names=function_name,
            session_ids=[session.id],
            pid=target.pid,
            timeout=10,
        )
        assert function_events
        assert all(event.function_name == function_name for event in function_events)
        assert client.remove_probe(session.id, probe.id).original_bytes_restored
        assert client.detach(session.id).state == pb.Session.DETACHED

        # Event identity is stored in retained history, so filtering still
        # works after the active session and probe metadata have been removed.
        retained = client.collect_traces(
            1,
            names=[context_name],
            session_ids=[session.id],
            after_sequence=0,
            timeout=2,
        )
        assert retained[0].context.event.name == context_name
        assert retained[0].function_name == function_name
        client.shutdown_server()


def test_real_native_symbol_listing_is_stable_and_paginated(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock", timeout=30) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        first = client.list_symbols(session.id, page_size=1)
        assert len(first.symbols) == 1
        assert first.next_page_token
        second = client.list_symbols(
            session.id, page_size=1, page_token=first.next_page_token
        )
        assert len(second.symbols) == 1
        assert second.symbols[0].selector != first.symbols[0].selector
        assert second.symbols[0].linkage_symbol
        assert second.symbols[0].module_path.endswith("/cpp-interceptors-target")
        with pytest.raises(grpc.RpcError) as mismatched_token:
            client.list_symbols(
                session.id,
                query="Calculator",
                page_size=1,
                page_token=first.next_page_token,
            )
        assert mismatched_token.value.code() == grpc.StatusCode.INVALID_ARGUMENT
        filtered = client.list_symbols(session.id, query="Calculator::multiply")
        assert any(
            item.selector == "Calculator::multiply(int, int) const"
            for item in filtered.symbols
        )
        with pytest.raises(grpc.RpcError) as invalid_token:
            client.list_symbols(session.id, page_token="not-a-token")
        assert invalid_token.value.code() == grpc.StatusCode.INVALID_ARGUMENT
        client.detach(session.id)
        with pytest.raises(grpc.RpcError) as detached:
            client.list_symbols(session.id)
        assert detached.value.code() == grpc.StatusCode.FAILED_PRECONDITION
        client.shutdown_server()
