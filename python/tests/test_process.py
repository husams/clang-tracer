import os
import signal
import socket
import subprocess
import sys
import time
from unittest.mock import Mock

import grpc
import pytest

from clang_tracer import Client, ServerProcess, ServerStatus
from clang_tracer.process import ServerProcessError


@pytest.fixture
def child(monkeypatch):
    monkeypatch.setattr("clang_tracer.process.platform.system", lambda: "Linux")
    monkeypatch.setattr("clang_tracer.process.platform.machine", lambda: "x86_64")
    process = Mock(pid=321)
    process.poll.return_value = None
    process.wait.return_value = 0
    spawn = Mock(return_value=process)
    monkeypatch.setattr("clang_tracer.process.subprocess.Popen", spawn)
    monkeypatch.setattr(
        ServerProcess, "status", lambda *a, **k: ServerStatus("local", False, False)
    )
    monkeypatch.setattr(
        Client, "status", lambda *a, **k: ServerStatus("local", True, False)
    )
    monkeypatch.setattr(Client, "_wait_for_ready", lambda *a, **k: None)
    return process, spawn


def test_launch_status_and_graceful_shutdown(child):
    process, spawn = child
    manager = ServerProcess("unix:///tmp/python-tracer/control.sock")
    manager.start(
        "/srv/path with spaces/server",
        agent="/srv/agent.so",
        operation_timeout=15,
        durable_directory="/srv/recordings",
    )
    assert manager.running and manager.pid == 321
    arguments = spawn.call_args.args[0]
    assert arguments == [
        "/srv/path with spaces/server",
        "--listen",
        "unix:///tmp/python-tracer/control.sock",
        "--timeout",
        "15",
        "--agent",
        "/srv/agent.so",
        "--context-durable-dir",
        "/srv/recordings",
    ]
    assert spawn.call_args.kwargs["shell"] is False
    assert spawn.call_args.kwargs["start_new_session"] is True
    manager.shutdown(timeout=8)
    process.terminate.assert_called_once()
    process.wait.assert_called_once_with(timeout=8)
    assert not manager.running
    manager.shutdown()


def test_launch_with_history_limits(child):
    _process, spawn = child
    manager = ServerProcess().start(
        event_history_limit=1000,
        context_history_limit=10,
        context_history_bytes=4096,
        context_event_bytes=4194304,
    )
    arguments = spawn.call_args.args[0]
    assert arguments[-8:] == [
        "--event-history-limit",
        "1000",
        "--context-history-limit",
        "10",
        "--context-history-bytes",
        "4096",
        "--context-event-bytes",
        "4194304",
    ]
    manager.shutdown()


@pytest.mark.parametrize("value", [0, -1, True, 1.5, 2**63])
def test_invalid_history_limit_does_not_launch(child, value):
    _process, spawn = child
    with pytest.raises(ValueError):
        ServerProcess().start(event_history_limit=value)
    spawn.assert_not_called()


def test_rejects_existing_endpoint(child, monkeypatch):
    process, spawn = child
    monkeypatch.setattr(
        ServerProcess, "status", lambda *a, **k: ServerStatus("local", True, False)
    )
    with pytest.raises(ServerProcessError, match="refusing to adopt"):
        ServerProcess().start()
    spawn.assert_not_called()


def test_shutdown_timeout_retains_ownership_and_never_kills(child):
    process, _ = child
    manager = ServerProcess().start()
    process.wait.side_effect = subprocess.TimeoutExpired("server", 0.01)
    with pytest.raises(ServerProcessError, match="retained"):
        manager.shutdown(timeout=0.01)
    assert manager.running
    process.kill.assert_not_called()
    process.wait.side_effect = None
    manager.shutdown()
    process.terminate.assert_called_once()


@pytest.mark.skipif(os.name != "posix", reason="POSIX process groups")
def test_parent_group_sigint_does_not_interrupt_managed_child(tmp_path):
    server = tmp_path / "fake-server"
    ready = tmp_path / "server-ready"
    server.write_text(
        "#!/usr/bin/env python3\n"
        "import signal, time\n"
        "signal.signal(signal.SIGTERM, lambda *_: exit(0))\n"
        f"open({str(ready)!r}, 'w').close()\n"
        "while True: time.sleep(1)\n"
    )
    server.chmod(0o755)
    helper = r"""
import os, signal, sys, threading, time
import clang_tracer.process as process_module
from clang_tracer import ServerProcess, ServerStatus

interrupted = threading.Event()
signal.signal(signal.SIGINT, lambda *_: interrupted.set())
process_module.platform.system = lambda: "Linux"
process_module.platform.machine = lambda: "x86_64"
class ReadyClient:
    def __init__(self, *_args, **_kwargs): pass
    def __enter__(self): return self
    def __exit__(self, *_args): pass
    def status(self, **_kwargs): return ServerStatus("local", True, False)
    def _wait_for_ready(self, **_kwargs): return None
process_module.Client = ReadyClient
ServerProcess.status = lambda self, **_kwargs: ServerStatus(self.endpoint, False, False)
manager = ServerProcess("unix:///tmp/not-used.sock")
try:
    manager.start(sys.argv[1], timeout=2)
    deadline = time.monotonic() + 2
    while not os.path.exists(sys.argv[2]) and time.monotonic() < deadline:
        time.sleep(0.01)
    if not os.path.exists(sys.argv[2]):
        raise RuntimeError("fake server did not start")
    print("READY", flush=True)
    if not interrupted.wait(3):
        raise RuntimeError("parent group SIGINT was not received")
    alive_after_interrupt = manager.running
    print(f"ALIVE={alive_after_interrupt}", flush=True)
finally:
    manager.shutdown(timeout=2)
"""
    parent = subprocess.Popen(
        [sys.executable, "-c", helper, str(server), str(ready)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    assert parent.stdout is not None
    try:
        assert parent.stdout.readline().strip() == "READY"
        os.killpg(parent.pid, signal.SIGINT)
        output, error = parent.communicate(timeout=5)
    finally:
        if parent.poll() is None:
            parent.kill()
            parent.wait()
    assert parent.returncode == 0, error
    assert "ALIVE=True" in output


def test_early_exit_reports_failure_and_reaps(child):
    process, _ = child
    process.poll.return_value = 1
    process.wait.return_value = 1
    manager = ServerProcess()
    with pytest.raises(ServerProcessError, match="status 1"):
        manager.start()
    assert not manager.running


@pytest.mark.skipif(os.name != "posix", reason="POSIX child process")
def test_start_waits_through_primed_client_channel_backoff(tmp_path, monkeypatch):
    monkeypatch.setattr("clang_tracer.process.platform.system", lambda: "Linux")
    monkeypatch.setattr("clang_tracer.process.platform.machine", lambda: "x86_64")
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    endpoint = f"127.0.0.1:{port}"
    executable = tmp_path / "delayed_server.py"
    executable.write_text(
        f"#!{sys.executable}\n"
        "import signal, sys, threading, time\n"
        "from concurrent.futures import ThreadPoolExecutor\n"
        "import grpc\n"
        "from clang_tracer.proto import tracer_pb2 as pb, tracer_pb2_grpc as rpc\n"
        "stopping = threading.Event()\n"
        "signal.signal(signal.SIGTERM, lambda *_: stopping.set())\n"
        "time.sleep(2.8)\n"
        "if stopping.is_set(): sys.exit(0)\n"
        "class Service(rpc.TracerServicer):\n"
        "    def ListSessions(self, request, context):\n"
        "        return pb.ListSessionsResponse()\n"
        "server = grpc.server(ThreadPoolExecutor(max_workers=2))\n"
        "rpc.add_TracerServicer_to_server(Service(), server)\n"
        f"server.add_insecure_port({endpoint!r})\n"
        "server.start()\n"
        "stopping.wait(30)\n"
        "server.stop(0).wait()\n"
    )
    executable.chmod(0o755)

    client = Client(endpoint)
    try:
        assert not client.status(timeout=0.1).running
        time.sleep(0.6)
        assert not client.status(timeout=0.1).running
        status = client.start_server(str(executable), timeout=4)
        assert status.running and status.managed
        assert client.server is not None and client.server.running
    finally:
        client.close()


def test_startup_timeout_includes_last_rpc_status(child, monkeypatch):
    class DeadlineError(grpc.RpcError):
        def code(self):
            return grpc.StatusCode.DEADLINE_EXCEEDED

        def details(self):
            return "readiness request deadline"

    def timeout(*_args, **_kwargs):
        raise DeadlineError()

    monkeypatch.setattr(Client, "_wait_for_ready", timeout)
    with pytest.raises(
        ServerProcessError,
        match="last RPC status: DEADLINE_EXCEEDED: readiness request deadline",
    ):
        ServerProcess().start(timeout=0.06)


def test_nonzero_shutdown_reports_incomplete_native_cleanup(child):
    process, _ = child
    manager = ServerProcess().start()
    process.wait.return_value = 1
    with pytest.raises(ServerProcessError, match="status 1"):
        manager.shutdown()


def test_unsupported_platform(monkeypatch):
    monkeypatch.setattr("clang_tracer.process.platform.system", lambda: "Darwin")
    with pytest.raises(ServerProcessError, match="Linux x86_64"):
        ServerProcess().start()


@pytest.mark.parametrize("value", [0, 301, 1.5, True])
def test_bad_operation_timeout(value):
    with pytest.raises(ValueError):
        ServerProcess().start(operation_timeout=value)
