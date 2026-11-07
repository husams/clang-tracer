"""Opt-in tests against rebuilt native server/agent and real target processes."""

import os
import platform
import shlex
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
from clang_tracer.display import render

pytestmark = pytest.mark.native


def test_reprobe_keeps_old_hook_on_compile_error_and_displays_string_state(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock", timeout=60) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        first = client.inject(session.id, "add(int, int)", INTERCEPTOR)
        before = client.collect_traces(2, session_ids=[session.id], timeout=10)
        cursor = before[-1].sequence
        with pytest.raises(grpc.RpcError):
            client.inject(session.id, "add(int, int)", "struct Interceptor { broken };")
        assert next(
            p for p in client.list_sessions()[0].probes if p.id == first.id
        ).active
        alive = client.collect_traces(
            2, session_ids=[session.id], after_sequence=cursor, timeout=10
        )
        assert all(event.probe_id == first.id for event in alive)
        source = """#include <string>
struct Scope {};
struct Snapshot : state<Scope> { std::string message; };
struct Interceptor {
  Interceptor(State<Snapshot>);
  void pre(const Call &call, Snapshot &snapshot) {
    snapshot.message = "actual string value";
    call.context().event("snapshot.ready");
  }
};
"""
        callback = temporary / "interceptor.cpp"
        callback.write_text(source)
        console = Console(client, output=DummyOutput())
        console.session_id = session.id
        installed = []
        console.emit = installed.append
        console.execute(
            parse_command(f'probe inject "add(int, int)" --callback "{callback}"')
        )
        second = installed[-1]
        assert isinstance(second, pb.Probe) and second.active
        assert second.id != first.id
        previous = next(p for p in client.list_sessions()[0].probes if p.id == first.id)
        assert not previous.active and previous.original_bytes_restored
        events = client.collect_traces(
            2,
            session_ids=[session.id],
            names=["snapshot.ready"],
            after_sequence=alive[-1].sequence,
            timeout=10,
        )
        assert all(event.probe_id == second.id for event in events)
        text = "".join(fragment for _, fragment in render(events[-1]))
        assert 'message = "actual string value"' in text
        # Replace a shared-context hook too, then restore the true original bytes.
        third = client.inject(session.id, "add(int, int)", INTERCEPTOR)
        assert third.id != second.id
        assert client.remove_probe(session.id, third.id).original_bytes_restored
        client.detach(session.id)
        assert target.poll() is None


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


def test_deep_native_stacks_keep_all_frames_and_honor_requested_depth(tmp_path):
    configured = os.getenv("CLANG_TRACER_NATIVE_BUILD")
    if not configured or platform.system() != "Linux":
        pytest.skip("set CLANG_TRACER_NATIVE_BUILD to a Linux native build directory")
    build = Path(configured).resolve()
    with subprocess.Popen(
        [build / "deep-stack-target"], stdin=subprocess.PIPE, stdout=subprocess.PIPE
    ) as target:
        try:
            assert target.stdout is not None and target.stdin is not None
            assert target.stdout.readline() == b"READY\n"
            with Client(f"unix://{tmp_path}/control.sock", timeout=60) as client:
                client.start_server(
                    str(build / "clang-tracer-server"),
                    agent=str(build / "libclang_tracer_agent.so"),
                )
                session = client.attach(target.pid)
                cursor = 0
                for depth, status in (
                    (128, pb.StackTrace.TRUNCATED), ("full", pb.StackTrace.COMPLETE)
                ):
                    probe = client.inject(
                        session.id, "traced_deep_leaf(int)", INTERCEPTOR,
                        stack_depth=depth, include_internal_stack_frames=True,
                    )
                    target.stdin.write(b"x")
                    target.stdin.flush()
                    events = client.collect_traces(
                        2, session_ids=[session.id], after_sequence=cursor, timeout=15
                    )
                    for event in events:
                        assert event.probe_id == probe.id
                        stack = event.stack
                        assert stack.status == status, stack.detail
                        assert stack.frames[0].function == "traced_deep_leaf(int)"
                        if depth == "full":
                            assert len(stack.frames) > 1200
                            assert any(frame.function == "main" for frame in stack.frames)
                            assert event.ByteSize() > 64 * 1024
                        else:
                            assert len(stack.frames) == depth
                    client.remove_probe(session.id, probe.id)
                    cursor = client.read_events().through_sequence
                context_source = """
struct Interceptor {
  void pre(const Call &call) {
    call.capture_stack(tracer::kUnlimitedStackDepth, false);
    call.context().event("deep.stack");
  }
};
"""
                probe = client.inject(session.id, "traced_deep_leaf(int)", context_source)
                target.stdin.write(b"x")
                target.stdin.flush()
                event, = client.collect_traces(
                    1, session_ids=[session.id], after_sequence=cursor,
                    names=["deep.stack"], timeout=15,
                )
                assert event.probe_id == probe.id
                assert event.stack.status == pb.StackTrace.COMPLETE, event.stack.detail
                assert len(event.stack.frames) > 1200
                client.remove_probe(session.id, probe.id)
                client.detach(session.id)
        finally:
            target.terminate()
            try:
                target.wait(timeout=5)
            except subprocess.TimeoutExpired:
                target.kill()
                target.wait()


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


def test_real_native_add_probe_rejects_tampered_resolved_metadata(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/control.sock", timeout=30) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        resolved = client.resolve_probe(session.id, "add(int, int)")

        def assert_no_active_probes():
            current = next(
                value for value in client.list_sessions() if value.id == session.id
            )
            assert not any(probe.active for probe in current.probes)

        tampered_specs = []
        changed_debug_info = pb.ProbeSpec()
        changed_debug_info.CopyFrom(resolved)
        changed_debug_info.debug_info.entry_size += 1
        tampered_specs.append(changed_debug_info)

        changed_signature = pb.ProbeSpec()
        changed_signature.CopyFrom(resolved)
        changed_signature.signature.ClearField("arguments")
        changed_signature.signature.arguments.extend([pb.INT64, pb.INT32])
        tampered_specs.append(changed_signature)

        changed_argument_name = pb.ProbeSpec()
        changed_argument_name.CopyFrom(resolved)
        changed_argument_name.ClearField("argument_names")
        changed_argument_name.argument_names.extend(
            ["altered_argument", resolved.argument_names[1]]
        )
        tampered_specs.append(changed_argument_name)

        for spec in tampered_specs:
            spec.callback_source = INTERCEPTOR
            with pytest.raises(grpc.RpcError) as rejected:
                client.add_probe(session.id, spec)
            assert (
                "mapped debug image, entry or callable metadata changed"
                in rejected.value.details()
            )
            assert_no_active_probes()

        valid_spec = pb.ProbeSpec()
        valid_spec.CopyFrom(resolved)
        valid_spec.callback_source = INTERCEPTOR
        probe = client.add_probe(session.id, valid_spec)
        assert probe.active
        events = client.collect_traces(4, session_ids=[session.id], timeout=10)
        assert events
        assert all(event.probe_id == probe.id for event in events)
        assert client.remove_probe(session.id, probe.id).original_bytes_restored
        assert_no_active_probes()
        assert client.detach(session.id).state == pb.Session.DETACHED


def test_native_common_headers_preserve_callback_isolation_and_typed_receivers(native):
    build, target, temporary = native
    rejected_source = """
#define TRACER_FAILED_CALLBACK_MARKER 1
namespace callback_scope { struct RejectedType {}; }
struct Interceptor {
  void pre(const Call &) { missing_callback_symbol(); }
};
"""
    function_source = """
#ifdef TRACER_FAILED_CALLBACK_MARKER
#error rejected callback macros leaked
#endif
#define TRACER_PREVIOUS_CALLBACK_MARKER 1
namespace callback_scope { using RejectedType = int; }
static int probe_counter = 0;
static constexpr int identity = 11;
struct Interceptor {
  void pre(const Call &call) {
    call.field("identity", identity);
    call.field("counter", ++probe_counter);
    call.field("first", call.argument<0>());
  }
};
"""
    method_source = """
#ifdef TRACER_FAILED_CALLBACK_MARKER
#error rejected callback macros leaked
#endif
#ifdef TRACER_PREVIOUS_CALLBACK_MARKER
#error another callback's macros leaked
#endif
namespace callback_scope { struct RejectedType {}; }
static int probe_counter = 0;
static constexpr int identity = 22;
struct Interceptor {
  void pre(const Call &call) {
    call.field("identity", identity);
    call.field("counter", ++probe_counter);
    call.field(call.self()->bias);
  }
};
"""
    with Client(f"unix://{temporary}/control.sock", timeout=30) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)

        def reject_callback():
            with pytest.raises(grpc.RpcError) as rejected:
                client.inject(session.id, "add(int, int)", rejected_source)
            assert "missing_callback_symbol" in rejected.value.details()
            assert target.poll() is None

        # A failed cold compile must leave the reusable headers usable.
        reject_callback()
        function = client.inject(session.id, "add(int, int)", function_source)
        # Exercise rejection again after a successful compile, with a live probe.
        with pytest.raises(grpc.RpcError) as rejected:
            client.inject(
                session.id, "Calculator::multiply(int, int) const", rejected_source
            )
        assert "missing_callback_symbol" in rejected.value.details()
        method = client.inject(
            session.id, "Calculator::multiply(int, int) const", method_source
        )
        seen = set()
        with client.watch_events([session.id], timeout=10) as events:
            for event in events:
                if event.phase != pb.PRE:
                    continue
                fields = {
                    item.name: item.value.signed_value for item in event.object_fields
                }
                if event.probe_id == function.id:
                    assert fields["identity"] == 11
                    assert fields["first"] == event.arguments[0].signed_value
                elif event.probe_id == method.id:
                    assert fields["identity"] == 22
                    assert fields["bias"] == 7
                    assert event.object_address
                else:
                    pytest.fail(f"unexpected probe event: {event.probe_id}")
                if event.probe_id not in seen:
                    assert fields["counter"] == 1
                seen.add(event.probe_id)
                if seen == {function.id, method.id}:
                    break
            else:
                pytest.fail("did not observe both independent callbacks")
        assert client.remove_probe(session.id, method.id).original_bytes_restored
        assert client.remove_probe(session.id, function.id).original_bytes_restored
        assert target.poll() is None
        assert client.detach(session.id).state == pb.Session.DETACHED


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
        modules = client.list_modules(session.id)
        assert any(path.endswith("/cpp-interceptors-target") for path in modules)
        with pytest.raises(grpc.RpcError) as limited:
            client.resolve_probe(session.id, "add(int, int)", debug_max_entries=1)
        assert limited.value.code() == grpc.StatusCode.RESOURCE_EXHAUSTED
        assert "--debug-max-entries" in limited.value.details()
        spec = client.resolve_probe(
            session.id,
            "add(int, int)",
            module="cpp-interceptors-target",
            debug_max_entries=1_000_001,
        )
        assert spec.debug_info.module in modules
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
        with pytest.raises(grpc.RpcError) as detached_modules:
            client.list_modules(session.id)
        assert detached_modules.value.code() == grpc.StatusCode.FAILED_PRECONDITION
        with pytest.raises(grpc.RpcError) as detached:
            client.list_symbols(session.id)
        assert detached.value.code() == grpc.StatusCode.FAILED_PRECONDITION
        client.shutdown_server()


def test_native_symbol_listing_has_no_implicit_limits(native):
    build, _target, temporary = native
    source = temporary / "many_symbols.cpp"
    executable = temporary / "many-symbols"
    prefix = "symbol_" + "x" * 24_000
    count = 720
    namespaces = ["n"] * 160
    deep_selector = "::".join([*namespaces, "deep_callable(int)"])
    with source.open("w") as output:
        output.write(
            "#include <sys/prctl.h>\n#include <unistd.h>\n#include <dlfcn.h>\n"
        )
        output.write(
            "#ifndef PR_SET_PTRACER\n#define PR_SET_PTRACER 0x59616d61\n#endif\n"
            "#ifndef PR_SET_PTRACER_ANY\n"
            "#define PR_SET_PTRACER_ANY ((unsigned long)-1)\n"
            "#endif\n"
        )
        for i in range(count):
            output.write(f"void {prefix}{i:04}() {{}}\n")
        for namespace in namespaces:
            output.write(f"namespace {namespace} {{\n")
        output.write("int deep_callable(int value) { return value + 1; }\n")
        output.write("}\n" * len(namespaces))
        output.write(
            "int main(int argc, char **argv) { "
            "prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY); "
            "if (argc > 1 && !dlopen(argv[1], RTLD_NOW)) return 2; "
            "while (true) { "
            + "::".join([*namespaces, "deep_callable(1)"])
            + "; usleep(10000); } }\n"
        )
    unrelated_source = temporary / "unrelated.cpp"
    unrelated = temporary / "libunrelated.so"
    unrelated_source.write_text(
        "namespace u {\n" * 220 + "int unrelated() { return 0; }\n" + "}\n" * 220
    )
    compiler = [os.getenv("CXX", "clang++"), *shlex.split(os.getenv("CXXFLAGS", ""))]
    subprocess.run(
        [
            *compiler,
            "-g",
            "-O0",
            "-shared",
            "-fPIC",
            str(unrelated_source),
            "-o",
            str(unrelated),
        ],
        check=True,
        capture_output=True,
        timeout=120,
    )
    subprocess.run(
        [
            os.getenv("CXX", "clang++"),
            *shlex.split(os.getenv("CXXFLAGS", "")),
            "-g",
            "-O0",
            str(source),
            "-o",
            str(executable),
            "-ldl",
        ],
        check=True,
        capture_output=True,
        timeout=120,
    )
    target = subprocess.Popen([executable, unrelated])
    try:
        # Wait until main has enabled tracing, rather than racing the new child.
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if target.poll() is not None:
                pytest.fail("large-symbol target exited before readiness")
            status = Path(f"/proc/{target.pid}/status").read_text()
            if "State:\tS" in status:
                break
            time.sleep(0.01)
        else:
            pytest.fail("large-symbol target did not become ready")
        with Client(f"unix://{temporary}/unlimited.sock", timeout=120) as client:
            client.start_server(
                str(build / "clang-tracer-server"),
                agent=str(build / "libclang_tracer_agent.so"),
            )
            session = client.attach(target.pid)
            assert str(unrelated) in client.list_modules(session.id)
            cancelled_lookup = client.stub.ResolveProbe.future(
                pb.ResolveProbeRequest(
                    session_id=session.id,
                    selector="clang_tracer_cancel_missing_selector",
                ),
                timeout=120,
            )
            # Cancellation can race a fast lookup on another machine. Accept
            # either an observed cancellation or the expected missing-name
            # result, without requiring the scan to take a minimum duration.
            if cancelled_lookup.cancel():
                assert cancelled_lookup.cancelled()
                with pytest.raises(grpc.FutureCancelledError):
                    cancelled_lookup.result()
                assert cancelled_lookup.code() == grpc.StatusCode.CANCELLED
            else:
                with pytest.raises(grpc.RpcError) as completed_lookup:
                    cancelled_lookup.result(timeout=120)
                assert completed_lookup.value.code() == grpc.StatusCode.INTERNAL
                assert (
                    "no supported matching callable" in completed_lookup.value.details()
                )
            current = next(
                value for value in client.list_sessions() if value.id == session.id
            )
            assert not any(probe.active for probe in current.probes)
            # Use a raw default request too: unlimited is a server contract.
            all_symbols = client.stub.ListSymbols(
                pb.ListSymbolsRequest(session_id=session.id), timeout=120
            )
            assert not all_symbols.next_page_token
            matches = [s for s in all_symbols.symbols if s.selector.startswith(prefix)]
            assert len(matches) == count
            assert deep_selector in {s.selector for s in all_symbols.symbols}
            with pytest.raises(grpc.RpcError) as depth_limited:
                client.resolve_probe(session.id, deep_selector, debug_max_depth=128)
            assert depth_limited.value.code() == grpc.StatusCode.RESOURCE_EXHAUSTED
            # The unrelated image is deeper than this budget. It must never
            # consume lookup work for a symbol defined only in the executable.
            deep = client.resolve_probe(session.id, deep_selector, debug_max_depth=200)
            assert deep.debug_info.selector == deep_selector
            assert client.resolve_probe(session.id, deep_selector) == deep
            assert all_symbols.ByteSize() > 32 * 1024 * 1024
            first = client.list_symbols(session.id, page_size=501)
            assert len(first.symbols) == 501
            assert len(first.next_page_token) > 65536
            second = client.list_symbols(
                session.id, page_size=501, page_token=first.next_page_token
            )
            assert not second.next_page_token
            assert [*first.symbols, *second.symbols] == list(all_symbols.symbols)
            largest_page = client.list_symbols(session.id, page_size=2**32 - 1)
            assert largest_page == all_symbols
            filtered = client.list_symbols(session.id, query="x" * 5000)
            assert len(filtered.symbols) == count
            assert not client.list_symbols(session.id, module="m" * 5000).symbols
            probe = client.inject(
                session.id,
                deep_selector,
                "struct Interceptor { void pre(const Call &) {} };",
                debug_max_depth=200,
            )
            assert client.collect_traces(2, session_ids=[session.id], timeout=10)
            assert client.remove_probe(session.id, probe.id).original_bytes_restored
            client.detach(session.id)
    finally:
        target.terminate()
        target.wait(timeout=5)
