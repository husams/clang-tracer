import sqlite3
from concurrent.futures import ThreadPoolExecutor

import grpc
import pytest
from prompt_toolkit.output import DummyOutput

from clang_tracer import Client
from clang_tracer import messages as pb
from clang_tracer.command import parse_command
from clang_tracer.console import Console
from clang_tracer.proto import tracer_pb2_grpc as rpc
from clang_tracer.storage import SQLiteEventStore


def test_sqlite_roundtrip_deduplication_and_server_restart(tmp_path):
    path = tmp_path / "events.db"
    event = pb.TraceEvent(
        server_instance_id="server-a",
        sequence=2**64 - 1,
        session_id="s",
        probe_id=2**64 - 1,
        pid=123,
        tid=456,
        monotonic_time_ns=2**64 - 1,
        function_name="f",
        phase=pb.POST,
    )
    event.context.event.name = "snapshot"
    with SQLiteEventStore(path) as store:
        store.write([event])
        store.write([event])
        event.server_instance_id = "server-b"
        store.write([event])
    with sqlite3.connect(path) as db:
        rows = db.execute(
            "SELECT server_instance_id, sequence, payload FROM trace_events "
            "ORDER BY server_instance_id"
        ).fetchall()
    assert len(rows) == 2
    restored = pb.TraceEvent.FromString(rows[1][2])
    assert restored == event
    assert rows[0][1] == str(2**64 - 1)


def test_sqlite_rejects_events_without_durable_identity(tmp_path):
    with SQLiteEventStore(tmp_path / "events.db") as store:
        with pytest.raises(ValueError, match="identity"):
            store.write([pb.TraceEvent(sequence=1)])


@pytest.mark.parametrize("path", ["", ":memory:"])
def test_sqlite_rejects_temporary_storage(path):
    with pytest.raises(ValueError, match="persistent"):
        SQLiteEventStore(path)


def test_failed_batch_rolls_back(tmp_path):
    path = tmp_path / "events.db"
    with SQLiteEventStore(path) as store:
        with pytest.raises(ValueError):
            store.write(
                [pb.TraceEvent(server_instance_id="s", sequence=1), pb.TraceEvent()]
            )
    with sqlite3.connect(path) as db:
        assert db.execute("SELECT count(*) FROM trace_events").fetchone()[0] == 0


class StoragePeer(rpc.TracerServicer):
    def __init__(self):
        self.events = [self.event(i) for i in range(1, 601)]
        self.next_sequence = 601
        self.instance = "server-a"
        self.acks = []
        self.fail_ack = False
        self.generate_on_ack = False
        self.drops = 0

    def event(self, sequence):
        return pb.TraceEvent(
            server_instance_id="server-a",
            sequence=sequence,
            session_id="even" if sequence % 2 == 0 else "odd",
            pid=123,
            tid=456,
            function_name="f",
            phase=pb.PRE,
        )

    def ReadEvents(self, request, context):
        if request.server_instance_id and request.server_instance_id != self.instance:
            context.abort(grpc.StatusCode.FAILED_PRECONDITION, "instance changed")
        through = request.through_sequence or self.next_sequence - 1
        events = [
            e
            for e in self.events
            if request.filter.after_sequence < e.sequence <= through
            and (
                not request.filter.session_ids
                or e.session_id in request.filter.session_ids
            )
        ]
        return pb.ReadEventsResponse(
            server_instance_id=self.instance,
            through_sequence=through,
            events=events[: request.limit or 256],
            server_dropped_events=self.drops,
        )

    def AcknowledgeEvents(self, request, context):
        self.acks.append(list(request.sequences))
        if self.fail_ack:
            context.abort(grpc.StatusCode.UNAVAILABLE, "ack failed")
        with sqlite3.connect(self.database) as db:
            rows = db.execute("SELECT sequence FROM trace_events").fetchall()
        # The server can observe committed rows before consumption is requested.
        assert set(request.sequences) <= {int(row[0]) for row in rows}
        self.events = [e for e in self.events if e.sequence not in request.sequences]
        if self.generate_on_ack:
            self.events.append(self.event(self.next_sequence))
            self.next_sequence += 1
        return pb.AcknowledgeEventsResponse(removed=len(request.sequences))

    def WatchEvents(self, request, _context):
        yield from (e for e in self.events if e.sequence > request.after_sequence)


@pytest.fixture
def storage_peer(tmp_path):
    service = StoragePeer()
    service.database = tmp_path / "events.db"
    with ThreadPoolExecutor(max_workers=2) as pool:
        server = grpc.server(pool)
        rpc.add_TracerServicer_to_server(service, server)
        port = server.add_insecure_port("127.0.0.1:0")
        server.start()
        try:
            with Client(f"127.0.0.1:{port}") as client:
                yield client, service
        finally:
            server.stop(0).wait()


def test_drain_commits_then_acknowledges_bounded_snapshot(storage_peer):
    client, peer = storage_peer
    peer.generate_on_ack = True
    assert client.drain_traces(peer.database) == 600
    assert [len(batch) for batch in peer.acks] == [256, 256, 88]
    assert [e.sequence for e in peer.events] == [601, 602, 603]


def test_filtered_drain_preserves_other_sessions(storage_peer):
    client, peer = storage_peer
    assert client.drain_traces(peer.database, session_ids=["even"]) == 300
    assert len(peer.events) == 300
    assert all(e.session_id == "odd" for e in peer.events)


def test_database_failure_never_acknowledges(storage_peer):
    client, peer = storage_peer
    with SQLiteEventStore(peer.database) as store:
        store.connection.execute(
            "CREATE TRIGGER reject_capture BEFORE INSERT ON trace_events "
            "BEGIN SELECT RAISE(ABORT, 'full disk simulation'); END"
        )
    with pytest.raises(sqlite3.Error, match="full disk"):
        client.drain_traces(peer.database)
    assert peer.acks == []
    assert len(peer.events) == 600


def test_ack_failure_can_retry_same_database_without_duplicates(storage_peer):
    client, peer = storage_peer
    peer.fail_ack = True
    with pytest.raises(grpc.RpcError):
        client.drain_traces(peer.database)
    assert len(peer.events) == 600
    peer.fail_ack = False
    assert client.drain_traces(peer.database) == 600
    with sqlite3.connect(peer.database) as db:
        assert db.execute("SELECT count(*) FROM trace_events").fetchone()[0] == 600


def test_restarted_server_rejects_snapshot_continuation(storage_peer, monkeypatch):
    client, peer = storage_peer
    acknowledge = client.acknowledge_events

    def restart(instance, sequences, **kwargs):
        result = acknowledge(instance, sequences, **kwargs)
        peer.instance = "server-b"
        return result

    monkeypatch.setattr(client, "acknowledge_events", restart)
    with pytest.raises(grpc.RpcError) as raised:
        client.drain_traces(peer.database)
    assert raised.value.code() == grpc.StatusCode.FAILED_PRECONDITION
    assert len(peer.events) == 344


def test_live_drain_refreshes_snapshots_and_stops_at_count(storage_peer):
    client, peer = storage_peer
    peer.generate_on_ack = True
    assert client.drain_traces(peer.database, listen=True, count=602, timeout=2) == 602


def test_empty_live_drain_times_out_without_acknowledging(storage_peer):
    client, peer = storage_peer
    peer.events = []
    assert client.drain_traces(peer.database, listen=True, timeout=0.02) == 0
    assert peer.acks == []


def test_sqlite_recording_does_not_consume_history(storage_peer):
    client, peer = storage_peer
    assert client.record_sqlite(peer.database, 2, timeout=2) == 2
    assert len(peer.events) == 600
    assert peer.acks == []


def test_drain_reports_eviction_even_if_no_events_remain(storage_peer):
    client, peer = storage_peer
    peer.events = []
    peer.drops = 42
    with pytest.warns(RuntimeWarning, match="42 history drops"):
        assert client.drain_traces(peer.database) == 0


def test_console_reaches_sqlite_record_and_drain(storage_peer):
    client, peer = storage_peer
    console = Console(client, output=DummyOutput())
    assert console.execute(parse_command(f'record "{peer.database}" 2 --sqlite'))
    assert len(peer.events) == 600
    console.session_id = "even"
    assert console.execute(parse_command(f'drain "{peer.database}" --count 3'))
    assert len(peer.events) == 597


def test_limit_options_parse():
    command = parse_command(
        "server start --event-history-limit 1000 "
        "--context-history-limit 10 --context-history-bytes 4096"
    )
    assert command.options == {
        "event_history_limit": 1000,
        "context_history_limit": 10,
        "context_history_bytes": 4096,
    }
