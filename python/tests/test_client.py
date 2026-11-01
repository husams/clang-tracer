import pickle
import struct
import threading

import grpc
import pytest

from clang_tracer import Client
from clang_tracer import messages as pb


def test_generated_messages_keep_package_identity():
    event = pb.TraceEvent(sequence=42)
    event.context.event.name = "snapshot"
    assert pickle.loads(pickle.dumps(event)) == event


def test_public_control_and_injection(peer):
    client, service = peer
    assert client.attach(123).id == "s1"
    assert client.list_sessions()[0].pid == 123
    probe = client.inject(
        "s1",
        "add(int, int)",
        "struct Interceptor {};",
        stack_depth=24,
        include_headers=["/srv/add.hpp"],
        compilation_database="/srv/build",
        print_arguments=True,
        print_result=True,
        callback_package_path="/srv/interceptor.cpp",
    )
    assert probe.id == 7
    resolution, addition = service.requests[-2:]
    assert resolution.selector == "add(int, int)"
    assert list(resolution.include_headers) == ["/srv/add.hpp"]
    assert resolution.compilation_database == "/srv/build"
    assert addition.spec.stack_depth == 24
    assert addition.spec.print_arguments and addition.spec.print_result
    assert addition.spec.callback_package_path == "/srv/interceptor.cpp"
    assert not addition.spec.include_internal_stack_frames
    assert client.remove_probe("s1", 7).original_bytes_restored
    assert client.detach("s1").state == pb.Session.DETACHED
    assert client.status().running


def test_all_public_rpcs_have_client_methods():
    methods = {
        "Attach": "attach",
        "AddProbe": "add_probe",
        "RemoveProbe": "remove_probe",
        "Detach": "detach",
        "ListSessions": "list_sessions",
        "WatchEvents": "watch_events",
        "ResolveProbe": "resolve_probe",
    }
    assert set(methods) == {
        m.name for m in pb.DESCRIPTOR.services_by_name["Tracer"].methods
    }
    assert all(callable(getattr(Client, name)) for name in methods.values())


def test_stream_filters_cursor_and_context(peer):
    client, service = peer
    with client.watch_events(["s1", "s2"], after_sequence=40, pid=123) as stream:
        assert stream.after_sequence == 40
        event = next(stream)
        assert event.sequence == stream.after_sequence == 41
        assert event.stack.frames[0].function == "add"
        assert event.context.event.name == "snapshot"
    request = service.requests[-1]
    assert list(request.session_ids) == ["s1", "s2"]
    assert request.pid == 123 and request.after_sequence == 40
    assert service.cancelled.wait(1)


def test_collection_and_binary_recording(peer, tmp_path):
    client, service = peer
    assert len(client.collect_traces(2)) == 2
    path = tmp_path / "events.pb"
    assert client.record_traces(path, 3, after_sequence=8) == 3
    raw = path.read_bytes()
    events = []
    while raw:
        size = struct.unpack("<I", raw[:4])[0]
        events.append(pb.TraceEvent.FromString(raw[4 : 4 + size]))
        raw = raw[4 + size :]
    assert [event.sequence for event in events] == [9, 10, 11]
    with pytest.raises(FileExistsError):
        client.record_traces(path, 1)


def test_rpc_failures_and_deadlines_are_not_hidden(peer):
    client, service = peer
    with pytest.raises(grpc.RpcError) as denied:
        client.attach(999)
    assert denied.value.code() == grpc.StatusCode.PERMISSION_DENIED
    with pytest.raises(grpc.RpcError) as stale:
        client.collect_traces(1, after_sequence=999)
    assert stale.value.code() == grpc.StatusCode.OUT_OF_RANGE
    with pytest.raises(grpc.RpcError) as expired:
        client.collect_traces(1, pid=999, timeout=0.05)
    assert expired.value.code() == grpc.StatusCode.DEADLINE_EXCEEDED
    assert service.cancelled.wait(1)


def test_blocked_read_can_be_cancelled_from_another_thread(peer):
    client, service = peer
    stream = client.watch_events(pid=999)
    errors = []

    def consume():
        try:
            next(stream)
        except grpc.RpcError as error:
            errors.append(error.code())

    worker = threading.Thread(target=consume)
    worker.start()
    stream.close()
    worker.join(2)
    assert not worker.is_alive()
    assert errors == [grpc.StatusCode.CANCELLED]


@pytest.mark.parametrize("pid", [0, -1, 2**32, True, 1.5])
def test_invalid_pid_is_rejected_before_rpc(peer, pid):
    client, service = peer
    with pytest.raises(ValueError):
        client.attach(pid)
    assert not service.requests


@pytest.mark.parametrize("depth", [-1, 65, True])
def test_invalid_stack_depth_is_rejected_before_resolution(peer, depth):
    client, service = peer
    with pytest.raises(ValueError):
        client.inject("s1", "add(int,int)", "struct Interceptor {};", stack_depth=depth)
    assert not service.requests


@pytest.mark.parametrize("timeout", [0, -1, float("inf"), float("nan")])
def test_invalid_timeout(timeout):
    with pytest.raises(ValueError):
        Client(timeout=timeout)


def test_shutdown_external_server_is_rejected(peer):
    client, _ = peer
    with pytest.raises(RuntimeError, match="did not launch"):
        client.shutdown_server()
    assert client.status().running
