"""Typed public control API and cancellable Protobuf trace streaming."""

import os
import struct
import time
import warnings
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from pathlib import Path
from types import TracebackType
from typing import TYPE_CHECKING, Any, Self

import grpc

from .proto import tracer_pb2 as pb
from .proto import tracer_pb2_grpc as rpc
from .storage import SQLiteEventStore

if TYPE_CHECKING:
    from .process import ServerProcess


_CHANNEL_OPTIONS = [
    ("grpc.max_receive_message_length", -1),
    ("grpc.max_send_message_length", -1),
    ("grpc.use_local_subchannel_pool", 1),
    ("grpc.initial_reconnect_backoff_ms", 100),
    ("grpc.max_reconnect_backoff_ms", 500),
]


def positive_timeout(value: float) -> float:
    import math

    if not math.isfinite(value) or value <= 0:
        raise ValueError("timeout must be finite and positive")
    return value


def uint(value: int, bits: int, name: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{name} must be an integer")
    if not minimum <= value < 2**bits:
        raise ValueError(f"{name} outside {minimum}..{2**bits - 1}")
    return value


def _name_filters(names: Iterable[str]) -> tuple[str, ...]:
    if isinstance(names, str):
        values: tuple[str, ...] = (names,)
    else:
        collected: list[str] = []
        try:
            for name in names:
                if len(collected) == 64:
                    raise ValueError("names accepts at most 64 filters")
                collected.append(name)
        except TypeError as error:
            raise ValueError(
                "names must be a string or an iterable of strings"
            ) from error
        values = tuple(collected)
    for name in values:
        if not isinstance(name, str) or not name or "\0" in name:
            raise ValueError("each name filter must be a nonempty string without NUL")
        if len(name.encode("utf-8")) > 4096:
            raise ValueError("each name filter must be at most 4096 UTF-8 bytes")
    return values


def _compiler_options(options: Iterable[str]) -> tuple[str, ...]:
    if isinstance(options, str):
        raise ValueError("compile_options must be an iterable of strings")
    values: list[str] = []
    total = 0
    try:
        for option in options:
            if len(values) == 128:
                raise ValueError("compile_options accepts at most 128 options")
            if not isinstance(option, str) or not option or "\0" in option:
                raise ValueError(
                    "each compile option must be nonempty and contain no NUL"
                )
            size = len(option.encode("utf-8"))
            if size > 4096:
                raise ValueError("each compile option must be at most 4096 UTF-8 bytes")
            total += size + 1
            if total > 65536:
                raise ValueError("compile_options must total at most 64 KiB")
            values.append(option)
    except TypeError as error:
        raise ValueError("compile_options must be an iterable of strings") from error
    return tuple(values)


@dataclass(frozen=True)
class ServerStatus:
    endpoint: str
    running: bool
    managed: bool
    pid: int | None = None
    detail: str = ""


class EventStream(Iterator[pb.TraceEvent]):
    """A stream has no deadline unless supplied; close it to cancel blocked reads."""

    def __init__(
        self,
        call: Any,
        after_sequence: int = 0,
        names: Iterable[str] = (),
    ) -> None:
        self._call = call
        self.after_sequence = after_sequence
        self.names = _name_filters(names)

    def __iter__(self) -> Self:
        return self

    def __next__(self) -> pb.TraceEvent:
        event: pb.TraceEvent = next(self._call)
        if self.names:
            context_name = ""
            if event.HasField("context") and event.context.HasField("event"):
                context_name = event.context.event.name
            if event.function_name not in self.names and context_name not in self.names:
                self.close()
                raise RuntimeError(
                    "server returned an event outside the requested name filters; "
                    "update clang-tracer-server to support server-side name filtering"
                )
        self.after_sequence = event.sequence
        return event

    def close(self) -> None:
        self._call.cancel()

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


class Client:
    """All public Tracer RPCs, with explicit deadlines and no automatic retries.

    Returned values are generated Protobuf messages. Supply a secure gRPC channel
    for TLS endpoints; otherwise the default is an insecure connection with a
    local subchannel pool and bounded reconnect backoff. Caller-supplied channels
    keep their own connectivity policy. Closing the client shuts down a server
    started through this instance.
    """

    def __init__(
        self,
        endpoint: str | None = None,
        *,
        timeout: float = 30,
        channel: Any = None,
    ) -> None:
        self.endpoint = (
            endpoint or os.getenv("CLANG_TRACER_ENDPOINT") or "127.0.0.1:50051"
        )
        self.timeout = positive_timeout(timeout)
        self._owns_channel = channel is None
        self.channel = (
            channel
            if channel is not None
            else grpc.insecure_channel(
                self.endpoint,
                options=_CHANNEL_OPTIONS,
            )
        )
        self.stub = rpc.TracerStub(self.channel)
        self.server: ServerProcess | None = None

    def attach(self, pid: int) -> pb.Session:
        response: pb.Session = self.stub.Attach(
            pb.AttachRequest(pid=uint(pid, 32, "pid", 1)),
            timeout=self.timeout,
        )
        return response

    def list_sessions(self) -> list[pb.Session]:
        return list(
            self.stub.ListSessions(
                pb.ListSessionsRequest(), timeout=self.timeout
            ).sessions
        )

    def list_modules(self, session_id: str) -> list[str]:
        """List current eligible mapped application image paths on the server."""
        response = self.stub.ListModules(
            pb.ListModulesRequest(session_id=session_id), timeout=self.timeout
        )
        return list(response.modules)

    def list_symbols(
        self,
        session_id: str,
        *,
        query: str = "",
        module: str = "",
        page_size: int | None = None,
        page_token: str = "",
    ) -> pb.ListSymbolsResponse:
        """List all mapped callables; a positive page_size enables pagination."""
        if not isinstance(session_id, str) or not session_id or "\0" in session_id:
            raise ValueError("session_id must be a nonempty string without NUL")
        for label, value in (("query", query), ("module", module)):
            if not isinstance(value, str) or "\0" in value:
                raise ValueError(f"{label} must be a string without NUL")
        if not isinstance(page_token, str) or "\0" in page_token:
            raise ValueError("page_token must be a string without NUL")
        size = 0 if page_size is None else uint(page_size, 32, "page_size")
        response: pb.ListSymbolsResponse = self.stub.ListSymbols(
            pb.ListSymbolsRequest(
                session_id=session_id,
                query=query,
                module=module,
                page_size=size,
                page_token=page_token,
            ),
            timeout=self.timeout,
        )
        return response

    def resolve_probe(
        self,
        session_id: str,
        selector: str,
        *,
        include_headers: Iterable[str] = (),
        compilation_database: str = "",
        module: str = "",
        debug_max_entries: int = 0,
        debug_max_depth: int = 0,
        compile_options: Iterable[str] = (),
    ) -> pb.ProbeSpec:
        """Infer the signature from DWARF on the server (requires ResolveProbe)."""
        options = _compiler_options(compile_options)
        response: pb.ProbeSpec = self.stub.ResolveProbe(
            pb.ResolveProbeRequest(
                session_id=session_id,
                selector=selector,
                include_headers=list(include_headers),
                compilation_database=compilation_database,
                module=module,
                debug_max_entries=uint(debug_max_entries, 64, "debug_max_entries"),
                debug_max_depth=uint(debug_max_depth, 32, "debug_max_depth"),
                compile_options=options,
            ),
            timeout=self.timeout,
        )
        response.compile_options[:] = options
        return response

    def validate_probe(
        self,
        session_id: str,
        selector: str,
        callback_source: str,
        *,
        include_headers: Iterable[str] = (),
        compilation_database: str = "",
        compile_options: Iterable[str] = (),
        level: str = "compile",
        module: str = "",
        debug_max_entries: int = 0,
        debug_max_depth: int = 0,
    ) -> pb.ValidationReport:
        """Check callback syntax or full compilation without installing it."""
        if not isinstance(session_id, str) or not session_id or "\0" in session_id:
            raise ValueError("session_id must be a nonempty string without NUL")
        if not isinstance(selector, str) or not selector or "\0" in selector:
            raise ValueError("selector must be a nonempty string without NUL")
        if (
            not isinstance(callback_source, str)
            or not callback_source.strip()
            or "\0" in callback_source
        ):
            raise ValueError("callback_source must be nonempty and contain no NUL")
        if len(callback_source.encode("utf-8")) > 64 * 1024:
            raise ValueError("callback_source exceeds 64 KiB")
        if level not in {"syntax", "compile"}:
            raise ValueError("level must be 'syntax' or 'compile'")
        if not isinstance(compilation_database, str) or "\0" in compilation_database:
            raise ValueError("compilation_database must be a string without NUL")
        if not isinstance(module, str) or "\0" in module:
            raise ValueError("module must be a string without NUL")
        options = _compiler_options(compile_options)
        response: pb.ValidationReport = self.stub.ValidateProbe(
            pb.ValidateProbeRequest(
                session_id=session_id,
                selector=selector,
                callback_source=callback_source,
                include_headers=list(include_headers),
                compilation_database=compilation_database,
                compile_options=options,
                level=(
                    pb.ValidateProbeRequest.SYNTAX
                    if level == "syntax"
                    else pb.ValidateProbeRequest.COMPILE
                ),
                module=module,
                debug_max_entries=uint(debug_max_entries, 64, "debug_max_entries"),
                debug_max_depth=uint(debug_max_depth, 32, "debug_max_depth"),
            ),
            timeout=self.timeout,
        )
        return response

    def add_probe(self, session_id: str, spec: pb.ProbeSpec) -> pb.Probe:
        response: pb.Probe = self.stub.AddProbe(
            pb.AddProbeRequest(
                session_id=session_id,
                spec=spec,
            ),
            timeout=self.timeout,
        )
        return response

    def inject(
        self,
        session_id: str,
        selector: str,
        callback_source: str,
        *,
        include_headers: Iterable[str] = (),
        compilation_database: str = "",
        stack_depth: int = 0,
        include_internal_stack_frames: bool = False,
        print_arguments: bool = False,
        print_result: bool = False,
        callback_package_path: str = "",
        module: str = "",
        debug_max_entries: int = 0,
        debug_max_depth: int = 0,
        compile_options: Iterable[str] = (),
    ) -> pb.Probe:
        """Compile and install a C++ interceptor without manual ABI declarations.

        Header, compilation database and optional package paths refer to the
        server host. Callback source is sent by value, so it can be local.
        """
        if not callback_source.strip() or "\0" in callback_source:
            raise ValueError("callback source must be nonempty and contain no NUL")
        if len(callback_source.encode()) > 64 * 1024:
            raise ValueError("callback source exceeds 64 KiB")
        uint(stack_depth, 32, "stack_depth")
        if stack_depth > 64:
            raise ValueError("stack_depth must be between 0 and 64")
        options = _compiler_options(compile_options)
        spec = self.resolve_probe(
            session_id,
            selector,
            include_headers=include_headers,
            compilation_database=compilation_database,
            module=module,
            debug_max_entries=debug_max_entries,
            debug_max_depth=debug_max_depth,
            compile_options=options,
        )
        spec.compile_options[:] = options
        spec.callback_source = callback_source
        spec.stack_depth = stack_depth
        spec.include_internal_stack_frames = include_internal_stack_frames
        spec.print_arguments = print_arguments
        spec.print_result = print_result
        spec.callback_package_path = callback_package_path
        return self.add_probe(session_id, spec)

    def remove_probe(self, session_id: str, probe_id: int) -> pb.Probe:
        response: pb.Probe = self.stub.RemoveProbe(
            pb.RemoveProbeRequest(
                session_id=session_id,
                probe_id=uint(probe_id, 64, "probe_id", 1),
            ),
            timeout=self.timeout,
        )
        return response

    def detach(self, session_id: str) -> pb.Session:
        response: pb.Session = self.stub.Detach(
            pb.DetachRequest(
                session_id=session_id,
            ),
            timeout=self.timeout,
        )
        return response

    def watch_events(
        self,
        session_ids: Iterable[str] = (),
        *,
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        timeout: float | None = None,
    ) -> EventStream:
        """Subscribe to all sessions or filter; timeout is a total stream deadline.

        Errors (including expired cursors and deadline expiry) remain grpc.RpcError.
        No reconnection or cursor reset hides trace loss.
        """
        if timeout is not None:
            positive_timeout(timeout)
        name_filters = _name_filters(names)
        return EventStream(
            self.stub.WatchEvents(
                pb.WatchEventsRequest(
                    session_ids=list(session_ids),
                    after_sequence=uint(after_sequence, 64, "cursor"),
                    pid=uint(pid, 32, "pid"),
                    names=name_filters,
                ),
                timeout=timeout,
            ),
            after_sequence=after_sequence,
            names=name_filters,
        )

    def collect_traces(
        self,
        count: int,
        *,
        session_ids: Iterable[str] = (),
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        timeout: float = 30,
    ) -> list[pb.TraceEvent]:
        """Collect at most count events; cancel even on error or interruption."""
        uint(count, 32, "count", 1)
        name_filters = _name_filters(names)
        events = []
        with self.watch_events(
            session_ids,
            after_sequence=after_sequence,
            pid=pid,
            names=name_filters,
            timeout=timeout,
        ) as stream:
            for event in stream:
                events.append(event)
                if len(events) == count:
                    break
        return events

    def record_traces(
        self,
        path: str | Path,
        count: int,
        *,
        session_ids: Iterable[str] = (),
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        timeout: float = 30,
    ) -> int:
        """Write length-prefixed binary Protobuf without keeping a list in memory.

        Create exclusively; a partial file remains readable if the stream fails.
        Each event is framed as uint32 little-endian byte length + serialized event,
        compatible with the native client's --format protobuf event output.
        """
        uint(count, 32, "count", 1)
        name_filters = _name_filters(names)
        written = 0
        with Path(path).open("xb") as output:
            with self.watch_events(
                session_ids,
                after_sequence=after_sequence,
                pid=pid,
                names=name_filters,
                timeout=timeout,
            ) as stream:
                for event in stream:
                    payload = event.SerializeToString()
                    output.write(struct.pack("<I", len(payload)))
                    output.write(payload)
                    written += 1
                    if written == count:
                        break
        return written

    def read_events(
        self,
        *,
        session_ids: Iterable[str] = (),
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        limit: int = 256,
        through_sequence: int = 0,
        server_instance_id: str = "",
        timeout: float | None = None,
    ) -> pb.ReadEventsResponse:
        """Read a bounded snapshot without removing it from server history."""
        uint(limit, 32, "limit", 1)
        if limit > 256:
            raise ValueError("limit must be at most 256")
        response: pb.ReadEventsResponse = self.stub.ReadEvents(
            pb.ReadEventsRequest(
                filter=pb.WatchEventsRequest(
                    session_ids=session_ids,
                    after_sequence=uint(after_sequence, 64, "after_sequence"),
                    pid=uint(pid, 32, "pid"),
                    names=_name_filters(names),
                ),
                limit=limit,
                through_sequence=uint(through_sequence, 64, "through_sequence"),
                server_instance_id=server_instance_id,
            ),
            timeout=self.timeout if timeout is None else positive_timeout(timeout),
        )
        return response

    def acknowledge_events(
        self,
        server_instance_id: str,
        sequences: Iterable[int],
        *,
        timeout: float | None = None,
    ) -> int:
        """Consume exact committed event IDs from this server lifetime."""
        values = tuple(uint(value, 64, "sequence", 1) for value in sequences)
        if not server_instance_id or len(values) > 256:
            raise ValueError("server instance and at most 256 sequences required")
        return int(
            self.stub.AcknowledgeEvents(
                pb.AcknowledgeEventsRequest(
                    server_instance_id=server_instance_id,
                    sequences=values,
                ),
                timeout=self.timeout if timeout is None else positive_timeout(timeout),
            ).removed
        )

    def record_sqlite(
        self,
        path: str | Path,
        count: int,
        *,
        session_ids: Iterable[str] = (),
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        timeout: float = 30,
    ) -> int:
        """Save a bounded live stream without consuming retained history."""
        uint(count, 32, "count", 1)
        written = 0
        with SQLiteEventStore(path) as store:
            with self.watch_events(
                session_ids,
                after_sequence=after_sequence,
                pid=pid,
                names=names,
                timeout=timeout,
            ) as stream:
                for event in stream:
                    store.write([event])
                    written += 1
                    if written >= count:
                        break
        return written

    def drain_traces(
        self,
        path: str | Path,
        *,
        listen: bool = False,
        count: int | None = None,
        session_ids: Iterable[str] = (),
        after_sequence: int = 0,
        pid: int = 0,
        names: Iterable[str] = (),
        timeout: float | None = None,
    ) -> int:
        """Commit retained events to SQLite, then consume them; optionally poll live.

        Ctrl-C stops polling. Failed acknowledgements can be retried by calling
        again with the same database. Overflow can still lose uncollected events.
        """
        if count is not None:
            uint(count, 32, "count", 1)
        deadline = (
            None if timeout is None else time.monotonic() + positive_timeout(timeout)
        )
        session_ids, names = tuple(session_ids), _name_filters(names)
        written, through, instance, reported_drops = 0, 0, "", 0
        with SQLiteEventStore(path) as store:
            while True:
                if deadline is not None and time.monotonic() >= deadline:
                    return written
                batch = self.read_events(
                    session_ids=session_ids,
                    after_sequence=after_sequence,
                    pid=pid,
                    names=names,
                    limit=256 if count is None else min(256, count - written),
                    through_sequence=through,
                    server_instance_id=instance,
                    timeout=None
                    if deadline is None
                    else max(0.001, min(self.timeout, deadline - time.monotonic())),
                )
                if not instance:
                    instance = batch.server_instance_id
                if not instance or batch.server_instance_id != instance:
                    raise RuntimeError("snapshot server instance changed")
                if through == 0:
                    through = batch.through_sequence
                if batch.server_dropped_events > reported_drops:
                    reported_drops = batch.server_dropped_events
                    warnings.warn(
                        f"server reports {reported_drops} history drops",
                        RuntimeWarning,
                        stacklevel=2,
                    )
                if batch.events:
                    if any(
                        event.server_instance_id != instance for event in batch.events
                    ):
                        raise RuntimeError(
                            "event server instance differs from snapshot"
                        )
                    store.write(batch.events)
                    self.acknowledge_events(
                        instance,
                        (e.sequence for e in batch.events),
                        timeout=None
                        if deadline is None
                        else max(0.001, min(self.timeout, deadline - time.monotonic())),
                    )
                    written += len(batch.events)
                    if count is not None and written >= count:
                        return written
                elif not listen:
                    return written
                else:
                    # Refresh only after finishing the previous snapshot. Pin the
                    # instance even between snapshots so restarts fail explicitly.
                    through = 0
                    remaining = (
                        0.1
                        if deadline is None
                        else max(0, min(0.1, deadline - time.monotonic()))
                    )
                    time.sleep(remaining)

    def status(self, *, timeout: float = 1) -> ServerStatus:
        managed = self.server is not None and self.server.running
        pid = self.server.pid if managed and self.server else None
        try:
            self.stub.ListSessions(
                pb.ListSessionsRequest(), timeout=positive_timeout(timeout)
            )
        except grpc.RpcError as error:
            return ServerStatus(
                self.endpoint,
                False,
                managed,
                pid,
                f"{error.code().name}: {error.details()}",
            )
        return ServerStatus(self.endpoint, True, managed, pid)

    def _wait_for_ready(self, *, timeout: float) -> None:
        """Wait for the endpoint using an idempotent unary readiness RPC."""
        self.stub.ListSessions(
            pb.ListSessionsRequest(),
            timeout=positive_timeout(timeout),
            wait_for_ready=True,
        )

    def start_server(
        self,
        executable: str = "clang-tracer-server",
        *,
        agent: str | None = None,
        timeout: float = 30,
        operation_timeout: int = 10,
        durable_directory: str | None = None,
        event_history_limit: int | None = None,
        context_history_limit: int | None = None,
        context_history_bytes: int | None = None,
    ) -> ServerStatus:
        """Start a local child and confirm both managed and client readiness.

        Caller-supplied channels retain their connection policy; if such a
        channel misses the shared deadline, the started child remains owned by
        this client and can be stopped with ``shutdown_server``.
        """
        from .process import ServerProcess

        deadline = time.monotonic() + positive_timeout(timeout)
        if self.server is None:
            self.server = ServerProcess(self.endpoint)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(
                "server startup deadline expired before launch; no server was started"
            )
        self.server.start(
            executable,
            agent=agent,
            timeout=remaining,
            operation_timeout=operation_timeout,
            durable_directory=durable_directory,
            event_history_limit=event_history_limit,
            context_history_limit=context_history_limit,
            context_history_bytes=context_history_bytes,
        )
        # A channel used before a restart may still be reconnecting after a
        # failed status check; wait until this client's channel is usable too.
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(
                "managed server is ready, but this client's channel did not "
                "become ready before the startup deadline; the server remains "
                "running and owned by this Client"
            )
        self._wait_for_ready(timeout=remaining)
        return ServerStatus(self.endpoint, True, True, self.server.pid)

    def shutdown_server(self, *, timeout: float = 30) -> None:
        if self.server is None:
            raise RuntimeError("this client did not launch the server")
        self.server.shutdown(timeout=timeout)

    def close(self) -> None:
        try:
            if self.server is not None:
                self.server.shutdown()
        finally:
            if self._owns_channel:
                self.channel.close()

    def __enter__(self) -> Self:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self.close()
