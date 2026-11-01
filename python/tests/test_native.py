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
