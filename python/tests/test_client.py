import pickle
import struct
import threading
from unittest.mock import Mock

import grpc
import pytest

from clang_tracer import Client
from clang_tracer import messages as pb
from clang_tracer.client import EventStream


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
        module="target",
        debug_max_entries=2_000_001,
        debug_max_depth=256,
        compile_options=["-std=c++17", "-DORDER=1", "-I/srv/include"],
    )
    assert probe.id == 7
    resolution, addition = service.requests[-2:]
    assert resolution.selector == "add(int, int)"
    assert list(resolution.include_headers) == ["/srv/add.hpp"]
    assert resolution.compilation_database == "/srv/build"
    assert resolution.module == "target"
    assert resolution.debug_max_entries == 2_000_001
    assert resolution.debug_max_depth == 256
    assert list(resolution.compile_options) == [
        "-std=c++17",
        "-DORDER=1",
        "-I/srv/include",
    ]
    assert addition.spec.stack_depth == 24
    assert addition.spec.print_arguments and addition.spec.print_result
    assert addition.spec.callback_package_path == "/srv/interceptor.cpp"
    assert list(addition.spec.compile_options) == [
        "-std=c++17",
        "-DORDER=1",
        "-I/srv/include",
    ]
    assert not addition.spec.include_internal_stack_frames
    assert client.remove_probe("s1", 7).original_bytes_restored
    assert client.detach("s1").state == pb.Session.DETACHED
    assert client.status().running


def test_list_symbols_uses_attached_session_and_returns_generated_page(peer):
    client, service = peer
    page = client.list_symbols(
        "s1", query="Calculator", module="target", page_size=20, page_token="opaque"
    )
    assert [item.selector for item in page.symbols] == [
        "Calculator::multiply(int, int) const"
    ]
    request = service.requests[-1]
    assert isinstance(request, pb.ListSymbolsRequest)
    assert request.session_id == "s1"
    assert request.query == "Calculator"
    assert request.module == "target"
    assert request.page_size == 20
    assert request.page_token == "opaque"


@pytest.mark.parametrize("page_size", [-1, 2**32, True, 1.5])
def test_list_symbols_rejects_invalid_page_size(peer, page_size):
    client, service = peer
    with pytest.raises(ValueError):
        client.list_symbols("s1", page_size=page_size)
    assert not any(isinstance(item, pb.ListSymbolsRequest) for item in service.requests)


@pytest.mark.parametrize("page_size", [None, 0, 501, 100_000, 2**32 - 1])
def test_list_symbols_accepts_unlimited_and_large_pages(peer, page_size):
    client, service = peer
    client.list_symbols("s1", page_size=page_size)
    assert service.requests[-1].page_size == (page_size or 0)


def test_list_symbols_defaults_to_unlimited(peer):
    client, service = peer
    client.list_symbols("s1")
    assert service.requests[-1].page_size == 0


def test_list_modules_and_default_lookup_limits(peer):
    client, service = peer
    assert client.list_modules("s1") == ["/srv/target", "/srv/libexample.so"]
    assert service.requests[-1].session_id == "s1"
    client.resolve_probe("s1", "add")
    request = service.requests[-1]
    assert not request.module
    assert request.debug_max_entries == request.debug_max_depth == 0


def test_resolve_probe_preserves_ordered_compiler_options(peer):
    client, service = peer
    spec = client.resolve_probe(
        "s1",
        "add(int, int)",
        compile_options=["-std=c++17", "-DVALUE=1", "-I/opt/app/include"],
    )
    request = service.requests[-1]
    assert list(request.compile_options) == [
        "-std=c++17",
        "-DVALUE=1",
        "-I/opt/app/include",
    ]
    assert list(spec.compile_options) == list(request.compile_options)


def test_add_probe_preserves_options_in_the_complete_spec(peer):
    client, service = peer
    spec = pb.ProbeSpec(
        symbol="_Z3addii",
        callback_source="struct Interceptor {};",
        compile_options=["-std=c++17", "-DVALUE=1"],
    )
    spec.signature.kind = pb.FUNCTION
    spec.signature.result = pb.INT32
    spec.signature.arguments.extend([pb.INT32, pb.INT32])
    client.add_probe("s1", spec)
    request = service.requests[-1]
    assert isinstance(request, pb.AddProbeRequest)
    assert list(request.spec.compile_options) == ["-std=c++17", "-DVALUE=1"]


def test_list_symbols_does_not_cap_filters_or_tokens(peer):
    client, service = peer
    client.list_symbols(
        "s1", query="q" * 5000, module="m" * 5000, page_token="a" * 70000
    )
    assert len(service.requests[-1].query) == 5000
    assert len(service.requests[-1].module) == 5000
    assert len(service.requests[-1].page_token) == 70000


def test_list_symbols_surfaces_old_server_unimplemented(peer, monkeypatch):
    client, _service = peer

    class UnsupportedRpc(grpc.RpcError):
        def code(self):
            return grpc.StatusCode.UNIMPLEMENTED

    error = UnsupportedRpc()
    monkeypatch.setattr(client.stub, "ListSymbols", Mock(side_effect=error))
    with pytest.raises(UnsupportedRpc) as raised:
        client.list_symbols("s1")
    assert raised.value is error
    assert raised.value.code() == grpc.StatusCode.UNIMPLEMENTED


def test_all_public_rpcs_have_client_methods():
    methods = {
        "Attach": "attach",
        "AddProbe": "add_probe",
        "RemoveProbe": "remove_probe",
        "Detach": "detach",
        "ListSessions": "list_sessions",
        "ListSymbols": "list_symbols",
        "ListModules": "list_modules",
        "WatchEvents": "watch_events",
        "ResolveProbe": "resolve_probe",
        "ValidateProbe": "validate_probe",
        "ReadEvents": "read_events",
        "AcknowledgeEvents": "acknowledge_events",
    }
    assert set(methods) == {
        m.name for m in pb.DESCRIPTOR.services_by_name["Tracer"].methods
    }
    assert all(callable(getattr(Client, name)) for name in methods.values())


def test_validation_transmits_ordered_options_and_returns_diagnostics(
    peer, monkeypatch
):
    client, _service = peer
    report = pb.ValidationReport(success=False, level=pb.ValidateProbeRequest.SYNTAX)
    report.diagnostics.add(
        stage="parse",
        severity="error",
        file="callback.cpp",
        line=3,
        column=8,
        message="expected ;",
    )
    calls = []

    def validate(request, timeout=None):
        calls.append((request, timeout))
        return report

    monkeypatch.setattr(client.stub, "ValidateProbe", validate)
    result = client.validate_probe(
        "s1",
        "add(int, int)",
        "struct Interceptor {};",
        include_headers=["/srv/add.hpp"],
        compilation_database="/srv/build",
        compile_options=["-std=c++17", "-DORDER=1"],
        level="syntax",
        module="target",
        debug_max_entries=2000001,
        debug_max_depth=256,
    )
    assert result is report
    request, timeout = calls[0]
    assert request.session_id == "s1"
    assert request.selector == "add(int, int)"
    assert request.callback_source == "struct Interceptor {};"
    assert list(request.include_headers) == ["/srv/add.hpp"]
    assert request.compilation_database == "/srv/build"
    assert list(request.compile_options) == ["-std=c++17", "-DORDER=1"]
    assert request.level == pb.ValidateProbeRequest.SYNTAX
    assert request.module == "target"
    assert request.debug_max_entries == 2000001
    assert request.debug_max_depth == 256
    assert timeout == client.timeout


@pytest.mark.parametrize(
    "options",
    ["-std=c++17", [""], ["bad\0option"], ["x" * 4097], ["-D1"] * 129],
)
def test_validation_rejects_invalid_compile_options_before_rpc(peer, options):
    client, service = peer
    with pytest.raises(ValueError, match="compile_options|compile option"):
        client.validate_probe(
            "s1", "add", "struct Interceptor {};", compile_options=options
        )
    assert not any(
        isinstance(request, pb.ValidateProbeRequest) for request in service.requests
    )


def test_stream_filters_cursor_and_context(peer):
    client, service = peer
    with client.watch_events(["s1", "s2"], after_sequence=40, pid=123) as stream:
        assert stream.after_sequence == 40
        event = next(stream)
        assert event.sequence == stream.after_sequence == 41
        assert event.stack.frames[0].function == "add"
        assert event.context.event.name == "value"
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


def test_name_filters_match_function_or_context_event_names_and_compose(peer):
    client, service = peer
    names = ["Calculator::multiply(int, int) const", "snapshot"]
    events = client.collect_traces(
        2, names=names, session_ids=["s1"], pid=123, timeout=1
    )
    assert [event.sequence for event in events] == [2, 4]
    assert [event.function_name for event in events] == [
        "Calculator::multiply(int, int) const",
        "other()",
    ]
    request = service.requests[-1]
    assert list(request.names) == names
    assert list(request.session_ids) == ["s1"]
    assert request.pid == 123


def test_record_name_filter_records_only_matching_events(peer, tmp_path):
    client, _service = peer
    path = tmp_path / "selected.pb"
    assert client.record_traces(path, 2, names="add(int, int)", timeout=1) == 2
    raw = path.read_bytes()
    events = []
    while raw:
        size = struct.unpack("<I", raw[:4])[0]
        events.append(pb.TraceEvent.FromString(raw[4 : 4 + size]))
        raw = raw[4 + size :]
    assert [event.sequence for event in events] == [1, 3]


@pytest.mark.parametrize("names", ["", [""], ["name\0bad"], ["x" * 4097], [1]])
def test_invalid_name_filters_are_rejected_before_rpc(peer, names):
    client, service = peer
    with pytest.raises(ValueError, match="name"):
        client.watch_events(names=names)
    assert not any(
        isinstance(request, pb.WatchEventsRequest) for request in service.requests
    )


def test_name_filter_accepts_up_to_4096_utf8_bytes(peer):
    client, service = peer
    accepted = "é" * 2048
    assert len(accepted.encode("utf-8")) == 4096
    assert client.collect_traces(1, names=accepted, timeout=0.02) == []
    request = next(
        request
        for request in service.requests
        if isinstance(request, pb.WatchEventsRequest)
    )
    assert list(request.names) == [accepted]


def test_filtered_stream_rejects_old_server_nonmatches_without_advancing_cursor():
    class FakeCall:
        def __init__(self, events):
            self.events = iter(events)
            self.cancelled = False

        def __iter__(self):
            return self

        def __next__(self):
            return next(self.events)

        def cancel(self):
            self.cancelled = True

    event = pb.TraceEvent(sequence=20, function_name="unrequested()")
    event.context.event.name = "unrequested.event"
    call = FakeCall([event])
    stream = EventStream(call, after_sequence=19, names=("requested()",))
    with pytest.raises(RuntimeError, match="update clang-tracer-server"):
        next(stream)
    assert stream.after_sequence == 19
    assert call.cancelled


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


def test_owned_channel_uses_bounded_local_reconnect_policy(monkeypatch):
    created = {}

    def make_channel(endpoint, *, options):
        created.update(endpoint=endpoint, options=dict(options))
        return Mock(spec=grpc.Channel)

    monkeypatch.setattr(grpc, "insecure_channel", make_channel)
    _client = Client("127.0.0.1:50051")
    assert created["options"]["grpc.use_local_subchannel_pool"] == 1
    assert created["options"]["grpc.initial_reconnect_backoff_ms"] == 100
    assert "grpc.min_reconnect_backoff_ms" not in created["options"]
    assert created["options"]["grpc.max_reconnect_backoff_ms"] == 500
    assert created["options"]["grpc.max_receive_message_length"] == -1
    assert created["options"]["grpc.max_send_message_length"] == -1
    caller_channel = Mock(spec=grpc.Channel)
    assert Client("127.0.0.1:50051", channel=caller_channel).channel is caller_channel


def test_start_server_uses_one_readiness_deadline(monkeypatch):
    from clang_tracer import client as client_module
    from clang_tracer import process as process_module

    values = {}
    now = [100.0]
    monkeypatch.setattr(client_module.time, "monotonic", lambda: now[0])

    class FakeServerProcess:
        pid = 99

        def __init__(self, endpoint):
            values["endpoint"] = endpoint

        def start(self, *_args, **kwargs):
            values["manager_timeout"] = kwargs["timeout"]
            now[0] += 0.04

    monkeypatch.setattr(process_module, "ServerProcess", FakeServerProcess)
    client = Client("127.0.0.1:50051", channel=Mock(spec=grpc.Channel))
    monkeypatch.setattr(
        client,
        "_wait_for_ready",
        lambda *, timeout: values.update(channel_timeout=timeout),
    )
    client.start_server(timeout=0.15)
    assert values["manager_timeout"] == pytest.approx(0.15)
    assert 0 < values["channel_timeout"] < values["manager_timeout"]


def test_start_server_reports_exhausted_shared_deadline(monkeypatch):
    from clang_tracer import client as client_module
    from clang_tracer import process as process_module

    now = [200.0]
    monkeypatch.setattr(client_module.time, "monotonic", lambda: now[0])

    class FakeServerProcess:
        pid = 99

        def __init__(self, _endpoint):
            pass

        def start(self, *_args, **_kwargs):
            now[0] += 0.06

    monkeypatch.setattr(process_module, "ServerProcess", FakeServerProcess)
    client = Client("127.0.0.1:50051", channel=Mock(spec=grpc.Channel))
    monkeypatch.setattr(
        client,
        "_wait_for_ready",
        lambda **_kwargs: pytest.fail("readiness call must not exceed the deadline"),
    )
    with pytest.raises(TimeoutError, match="server remains running and owned"):
        client.start_server(timeout=0.05)
    assert isinstance(client.server, FakeServerProcess)
