#pragma once

#include "context_execution.hpp"

#include <algorithm>
#include <cstdio>
#include <cstddef>

namespace tracer::context_execution {

inline const char *diagnostic_reason_name(Reason reason) noexcept {
  switch (reason) {
  case Reason::no_active_hook: return "no_active_hook";
  case Reason::missing_state: return "missing_state";
  case Reason::incompatible_state: return "incompatible_state";
  case Reason::invalid_ownership: return "invalid_ownership";
  case Reason::access_failure: return "access_failure";
  case Reason::constructor_failure: return "constructor_failure";
  case Reason::hook_failure: return "hook_failure";
  case Reason::capture_failure: return "capture_failure";
  case Reason::export_queue_full: return "export_queue_full";
  case Reason::schema_failure: return "schema_failure";
  case Reason::transport_failure: return "transport_failure";
  case Reason::unsupported_effect: return "unsupported_effect";
  case Reason::lifecycle_access: return "lifecycle_access";
  case Reason::reentrant_application_call: return "reentrant_application_call";
  case Reason::invalid_transition: return "invalid_transition";
  case Reason::count: return "unknown";
  }
  return "unknown";
}

inline const char *diagnostic_phase_name(Phase phase) noexcept {
  switch (phase) {
  case Phase::registration: return "registration";
  case Phase::construction: return "construction";
  case Phase::pre: return "pre";
  case Phase::target: return "target";
  case Phase::post: return "post";
  case Phase::access: return "access";
  case Phase::capture: return "capture";
  case Phase::exporting: return "exporting";
  case Phase::lifecycle: return "lifecycle";
  case Phase::transport: return "transport";
  case Phase::count: return "unknown";
  }
  return "unknown";
}

inline const char *diagnostic_action_name(Action action) noexcept {
  switch (action) {
  case Action::continued: return "continued";
  case Action::callback_skipped: return "callback_skipped";
  case Action::event_dropped: return "event_dropped";
  case Action::scope_unchanged: return "scope_unchanged";
  case Action::call_rejected: return "call_rejected";
  case Action::lifecycle_ignored: return "lifecycle_ignored";
  case Action::transport_paused: return "transport_paused";
  }
  return "unknown";
}

inline int format_diagnostic(const Diagnostic &diagnostic, char *buffer,
                             std::size_t capacity) noexcept {
  if (!buffer || capacity == 0)
    return -1;
  const auto event_name_size = std::min(diagnostic.event_name.size(),
                                        kEventNameCapacity - 1);
  return std::snprintf(
      buffer, capacity,
      "clang-tracer context diagnostic reason=%s phase=%s action=%s "
      "count=%llu aggregate=%u call_id=%llu event_id=%llu event=%.*s\n",
      diagnostic_reason_name(diagnostic.reason),
      diagnostic_phase_name(diagnostic.phase),
      diagnostic_action_name(diagnostic.action),
      static_cast<unsigned long long>(diagnostic.occurrences),
      diagnostic.aggregate ? 1u : 0u,
      static_cast<unsigned long long>(diagnostic.call_id),
      static_cast<unsigned long long>(diagnostic.event_id),
      static_cast<int>(event_name_size), diagnostic.event_name.data());
}

} // namespace tracer::context_execution
