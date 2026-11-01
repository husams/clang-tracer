"""Opt-in tests against rebuilt native server/agent and real target processes."""

import os
import platform
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
