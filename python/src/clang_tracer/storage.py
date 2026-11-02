"""SQLite capture format shared with the native command-line client."""

import sqlite3
from collections.abc import Iterable
from pathlib import Path
from types import TracebackType
from typing import Self

from .proto import tracer_pb2 as pb

SCHEMA = """
CREATE TABLE IF NOT EXISTS trace_events (
  server_instance_id TEXT NOT NULL,
  sequence TEXT NOT NULL,
  session_id TEXT NOT NULL,
  probe_id TEXT NOT NULL,
  pid INTEGER NOT NULL,
  tid INTEGER NOT NULL,
  monotonic_time_ns TEXT NOT NULL,
  phase INTEGER NOT NULL,
  function_name TEXT NOT NULL,
  event_name TEXT NOT NULL,
  agent_dropped_events TEXT NOT NULL,
  server_dropped_events TEXT NOT NULL,
  payload BLOB NOT NULL,
  PRIMARY KEY (server_instance_id, sequence)
);
CREATE INDEX IF NOT EXISTS trace_events_process ON trace_events(pid, session_id);
"""
INSERT = """INSERT INTO trace_events VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT (server_instance_id, sequence) DO NOTHING"""


class SQLiteEventStore:
    def __init__(self, path: str | Path) -> None:
        if str(path) in {"", ":memory:"}:
            raise ValueError("SQLite capture requires a persistent file path")
        self.connection = sqlite3.connect(path)
        try:
            self.connection.execute("PRAGMA journal_mode=WAL")
            self.connection.execute("PRAGMA synchronous=FULL")
            self.connection.executescript(SCHEMA)
        except BaseException:
            self.connection.close()
            raise

    def write(self, events: Iterable[pb.TraceEvent]) -> None:
        with self.connection:
            for event in events:
                if not event.server_instance_id or event.sequence == 0:
                    raise ValueError("event lacks server storage identity")
                self.connection.execute(
                    INSERT,
                    (
                        event.server_instance_id,
                        str(event.sequence),
                        event.session_id,
                        str(event.probe_id),
                        event.pid,
                        event.tid,
                        str(event.monotonic_time_ns),
                        event.phase,
                        event.function_name,
                        event.context.event.name,
                        str(event.agent_dropped_events),
                        str(event.server_dropped_events),
                        event.SerializeToString(),
                    ),
                )

    def __enter__(self) -> Self:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.connection.close()
