#pragma once

#include <stdexcept>
#include <string>

#include <sqlite3.h>

#include "tracer.pb.h"

namespace tracer::client {

// Compatible with python/src/clang_tracer/storage.py. Every write commits
// before its caller is allowed to acknowledge consumption at the server.
class SQLiteEvents final {
public:
  explicit SQLiteEvents(const std::string &path) {
    if (path.empty() || path == ":memory:") {
      throw std::runtime_error(
          "SQLite capture requires a persistent file path");
    }
    const int result =
        sqlite3_open_v2(path.c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (result != SQLITE_OK) {
      const std::string error =
          database_ ? sqlite3_errmsg(database_) : "allocation failed";
      if (database_) {
        sqlite3_close(database_);
      }
      database_ = nullptr;
      throw std::runtime_error("SQLite: " + error);
    }
    try {
      sqlite3_busy_timeout(database_, 5000);
      execute("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;");
      execute(R"sql(
CREATE TABLE IF NOT EXISTS trace_events (
  server_instance_id TEXT NOT NULL, sequence TEXT NOT NULL,
  session_id TEXT NOT NULL, probe_id TEXT NOT NULL,
  pid INTEGER NOT NULL, tid INTEGER NOT NULL, monotonic_time_ns TEXT NOT NULL,
  phase INTEGER NOT NULL, function_name TEXT NOT NULL, event_name TEXT NOT NULL,
  agent_dropped_events TEXT NOT NULL, server_dropped_events TEXT NOT NULL,
  payload BLOB NOT NULL, PRIMARY KEY (server_instance_id, sequence));
CREATE INDEX IF NOT EXISTS trace_events_process ON trace_events(pid, session_id);
)sql");
      check(sqlite3_prepare_v2(
          database_,
          "INSERT INTO trace_events VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
          "?, ?) "
          "ON CONFLICT (server_instance_id, sequence) DO NOTHING",
          -1, &insert_, nullptr));
    } catch (...) {
      sqlite3_finalize(insert_);
      sqlite3_close(database_);
      throw;
    }
  }

  ~SQLiteEvents() {
    sqlite3_finalize(insert_);
    sqlite3_close(database_);
  }

  SQLiteEvents(const SQLiteEvents &) = delete;
  SQLiteEvents &operator=(const SQLiteEvents &) = delete;

  template <typename Events> void write(const Events &events) {
    execute("BEGIN IMMEDIATE");
    try {
      for (const auto &event : events) {
        append(event);
      }
      execute("COMMIT");
    } catch (...) {
      sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
      throw;
    }
  }

private:
  sqlite3 *database_ = nullptr;
  sqlite3_stmt *insert_ = nullptr;

  void check(int result) {
    if (result != SQLITE_OK) {
      throw std::runtime_error(std::string("SQLite: ") +
                               sqlite3_errmsg(database_));
    }
  }

  void execute(const char *sql) {
    check(sqlite3_exec(database_, sql, nullptr, nullptr, nullptr));
  }

  void text(int position, const std::string &value) {
    check(sqlite3_bind_text(insert_, position, value.data(),
                            static_cast<int>(value.size()), SQLITE_TRANSIENT));
  }

  void append(const clangtracer::v1::TraceEvent &event) {
    if (event.server_instance_id().empty() || event.sequence() == 0) {
      throw std::runtime_error(
          "event lacks server storage identity; upgrade the server");
    }
    sqlite3_reset(insert_);
    sqlite3_clear_bindings(insert_);
    text(1, event.server_instance_id());
    text(2, std::to_string(event.sequence()));
    text(3, event.session_id());
    text(4, std::to_string(event.probe_id()));
    check(sqlite3_bind_int64(insert_, 5, event.pid()));
    check(sqlite3_bind_int64(insert_, 6, event.tid()));
    text(7, std::to_string(event.monotonic_time_ns()));
    check(sqlite3_bind_int(insert_, 8, event.phase()));
    text(9, event.function_name());
    text(10, event.context().event().name());
    text(11, std::to_string(event.agent_dropped_events()));
    text(12, std::to_string(event.server_dropped_events()));
    std::string payload;
    if (!event.SerializeToString(&payload)) {
      throw std::runtime_error("cannot serialize trace event");
    }
    check(sqlite3_bind_blob(insert_, 13, payload.data(),
                            static_cast<int>(payload.size()),
                            SQLITE_TRANSIENT));
    if (sqlite3_step(insert_) != SQLITE_DONE) {
      throw std::runtime_error(std::string("SQLite: ") +
                               sqlite3_errmsg(database_));
    }
    check(sqlite3_reset(insert_));
  }
};

} // namespace tracer::client
