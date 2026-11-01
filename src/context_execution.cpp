#include "context_execution.hpp"

#include <algorithm>
#include <limits>

namespace tracer::context_execution {
namespace {
struct ThreadState {
  Mode mode = Mode::Application;
  void *hook = nullptr;
  std::uint32_t access = 0;
  std::uint32_t control = 0;
  std::uint32_t suppression = 0;
};
thread_local ThreadState tls;
Diagnostics global_diagnostics;

bool transition_allowed(Mode from, Mode to) noexcept {
  switch (to) {
  case Mode::Application:
    return from == Mode::Tracking;
  case Mode::Tracking:
    return from == Mode::Application && tls.hook == nullptr;
  case Mode::Support:
    return from == Mode::Tracking || from == Mode::Support;
  case Mode::LifecycleNoTrace:
    return (from == Mode::Tracking || from == Mode::Support ||
            from == Mode::Application) &&
           tls.access == 0 && tls.control == 0;
  case Mode::TransportNoTrace:
    return from == Mode::Application && tls.access == 0 && tls.control == 0 &&
           tls.hook == nullptr;
  }
  return false;
}
void saturating_increment(std::atomic<std::uint64_t> &counter) noexcept {
  auto value = counter.load(std::memory_order_relaxed);
  while (value != std::numeric_limits<std::uint64_t>::max() &&
         !counter.compare_exchange_weak(value, value + 1,
                                        std::memory_order_relaxed)) {}
}
std::size_t counter_index(Reason reason, Phase phase) noexcept {
  return static_cast<std::size_t>(reason) *
             static_cast<std::size_t>(Phase::count) +
         static_cast<std::size_t>(phase);
}
void set_event_name(Diagnostic &diagnostic, std::string_view name) noexcept {
  const auto size = std::min(name.size(), diagnostic.event_name.size() - 1);
  for (std::size_t i = 0; i < size; ++i) diagnostic.event_name[i] = name[i];
  diagnostic.event_name[size] = '\0';
  diagnostic.has_correlation = size != 0 || diagnostic.call_id != 0 ||
                               diagnostic.event_id != 0;
}
} // namespace

Snapshot current() noexcept {
  return {tls.mode, tls.hook, tls.access, tls.control, tls.suppression};
}
Diagnostics &runtime_diagnostics() noexcept { return global_diagnostics; }

EntryDecision check_entry(Callee callee) noexcept {
  switch (tls.mode) {
  case Mode::Application:
    if (callee == Callee::ApplicationEndpoint) return EntryDecision::Trace;
    if (callee == Callee::AdmittedSupport || callee == Callee::TransportWorker)
      return EntryDecision::Bypass;
    return EntryDecision::Reject;
  case Mode::Tracking:
  case Mode::Support:
    return callee == Callee::AdmittedSupport ? EntryDecision::Bypass
                                             : EntryDecision::Reject;
  case Mode::LifecycleNoTrace:
    return callee == Callee::AdmittedLifecycleBoundary
               ? EntryDecision::Bypass : EntryDecision::Reject;
  case Mode::TransportNoTrace:
    return EntryDecision::Bypass;
  }
  return EntryDecision::Reject;
}

bool context_access_allowed() noexcept {
  return tls.mode == Mode::Tracking && tls.hook != nullptr;
}

bool event_allowed(Diagnostics &diagnostics, std::string_view event_name) noexcept {
  if (context_access_allowed()) return true;
  diagnostics.no_active_hook(event_name);
  return false;
}

Scope::Scope(Mode next) noexcept : prior_(current()) {
  if (!transition_allowed(tls.mode, next)) {
    global_diagnostics.record(Severity::error, Reason::invalid_transition,
                              Phase::access, Action::scope_unchanged);
    return;
  }
  const bool adds_suppression = next == Mode::Tracking ||
      next == Mode::LifecycleNoTrace || next == Mode::TransportNoTrace;
  if (adds_suppression &&
      tls.suppression == std::numeric_limits<std::uint32_t>::max()) {
    global_diagnostics.record(Severity::critical,
                              Reason::invalid_transition, Phase::access,
                              Action::scope_unchanged);
    return;
  }
  if (next == Mode::LifecycleNoTrace || next == Mode::TransportNoTrace) {
    masked_hook_ = true;
    tls.hook = nullptr;
  }
  if (adds_suppression) ++tls.suppression;
  tls.mode = next;
  entered_ = true;
}
Scope::~Scope() noexcept {
  if (!entered_) return;
  tls.mode = prior_.mode;
  tls.suppression = prior_.suppression_depth;
  if (masked_hook_) tls.hook = prior_.current_hook;
}

OriginalDispatch::OriginalDispatch() noexcept : prior_(current()) {
  if (tls.mode != Mode::Tracking || tls.access != 0 || tls.control != 0) {
    global_diagnostics.record(Severity::error, Reason::invalid_transition,
                              Phase::target, Action::call_rejected);
    return;
  }
  tls.mode = Mode::Application;
  tls.hook = nullptr;
  tls.suppression = 0;
  entered_ = true;
}
OriginalDispatch::~OriginalDispatch() noexcept {
  if (entered_) {
    tls.mode = prior_.mode;
    tls.hook = prior_.current_hook;
    tls.suppression = prior_.suppression_depth;
  }
}

CurrentHook::CurrentHook(void *hook) noexcept : prior_(tls.hook) {
  if (tls.mode != Mode::Tracking || tls.hook || !hook) {
    global_diagnostics.record(Severity::error, Reason::invalid_transition,
                              Phase::pre, Action::scope_unchanged);
    return;
  }
  tls.hook = hook;
  installed_ = true;
}
CurrentHook::~CurrentHook() noexcept {
  if (installed_) tls.hook = prior_;
}

AccessLease::AccessLease() noexcept {
  if (tls.mode != Mode::Tracking) {
    global_diagnostics.record(Severity::error, Reason::lifecycle_access,
                              Phase::access, Action::call_rejected);
    return;
  }
  if (tls.access == std::numeric_limits<std::uint32_t>::max()) {
    global_diagnostics.record(Severity::critical, Reason::access_failure,
                              Phase::access, Action::call_rejected);
    return;
  }
  ++tls.access;
  entered_ = true;
}
AccessLease::~AccessLease() noexcept {
  if (entered_ && tls.access) --tls.access;
}
ControlLease::ControlLease() noexcept {
  if (tls.control == std::numeric_limits<std::uint32_t>::max()) {
    global_diagnostics.record(Severity::critical, Reason::invalid_transition,
                              Phase::access, Action::call_rejected);
    return;
  }
  ++tls.control;
  entered_ = true;
}
ControlLease::~ControlLease() noexcept {
  if (entered_ && tls.control) --tls.control;
}

Diagnostics::Diagnostics() noexcept {
  for (std::size_t i = 0; i < ring_.size(); ++i)
    ring_[i].sequence.store(i, std::memory_order_relaxed);
}

bool Diagnostics::record(Severity severity, Reason reason, Phase phase,
                         Action action, std::uint64_t call_id,
                         std::uint64_t event_id,
                         std::string_view event_name) noexcept {
  if (reason >= Reason::count || phase >= Phase::count) return false;
  saturating_increment(counters_[counter_index(reason, phase)]);
  Diagnostic sample;
  sample.severity = severity;
  sample.reason = reason;
  sample.phase = phase;
  sample.action = action;
  sample.call_id = call_id;
  sample.event_id = event_id;
  set_event_name(sample, event_name);
  sample.has_correlation = call_id != 0 || event_id != 0 || !event_name.empty();

  std::size_t position = enqueue_.load(std::memory_order_relaxed);
  Cell *cell = nullptr;
  bool reserved = false;
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    cell = &ring_[position & (kDiagnosticCapacity - 1)];
    const auto sequence = cell->sequence.load(std::memory_order_acquire);
    const auto difference = static_cast<std::intptr_t>(sequence) -
                            static_cast<std::intptr_t>(position);
    if (difference == 0) {
      if (enqueue_.compare_exchange_weak(position, position + 1,
                                          std::memory_order_relaxed)) {
        reserved = true;
        break;
      }
    } else if (difference < 0) {
      return false;
    } else {
      position = enqueue_.load(std::memory_order_relaxed);
    }
  }
  if (!reserved) return false;
  cell->value = sample;
  cell->sequence.store(position + 1, std::memory_order_release);
  return true;
}

void Diagnostics::no_active_hook(std::string_view event_name) noexcept {
  const auto phase = tls.mode == Mode::TransportNoTrace
                         ? Phase::transport : Phase::lifecycle;
  record(Severity::warning, Reason::no_active_hook, phase,
         Action::lifecycle_ignored, 0, 0, event_name);
}

std::uint64_t Diagnostics::count(Reason reason, Phase phase) const noexcept {
  if (reason >= Reason::count || phase >= Phase::count) return 0;
  return counters_[counter_index(reason, phase)].load(std::memory_order_relaxed);
}

std::size_t Diagnostics::drain_coalesced(DiagnosticSink sink, void *argument,
                                           std::size_t max_samples) noexcept {
  if (!sink || tls.mode != Mode::TransportNoTrace || tls.access || tls.control)
    return 0;
  std::array<Diagnostic, kDiagnosticCapacity> samples{};
  std::size_t sample_count = 0;
  std::size_t position = dequeue_.load(std::memory_order_relaxed);
  while (sample_count < std::min(max_samples, samples.size())) {
    Cell *cell = &ring_[position & (kDiagnosticCapacity - 1)];
    const auto sequence = cell->sequence.load(std::memory_order_acquire);
    const auto difference = static_cast<std::intptr_t>(sequence) -
                            static_cast<std::intptr_t>(position + 1);
    if (difference == 0) {
      if (!dequeue_.compare_exchange_weak(position, position + 1,
                                          std::memory_order_relaxed)) continue;
      samples[sample_count++] = cell->value;
      cell->sequence.store(position + kDiagnosticCapacity,
                           std::memory_order_release);
      ++position;
    } else if (difference < 0) {
      break;
    } else {
      position = dequeue_.load(std::memory_order_relaxed);
    }
  }

  std::array<bool, kCounterCount> emitted{};
  std::size_t reports = 0;
  for (std::size_t i = 0; i < sample_count; ++i) {
    const auto &sample = samples[i];
    const auto index = counter_index(sample.reason, sample.phase);
    if (emitted[index]) continue;
    emitted[index] = true;
    Diagnostic aggregate = sample;
    aggregate.aggregate = true;
    aggregate.has_correlation = false;
    aggregate.call_id = aggregate.event_id = 0;
    aggregate.event_name.fill('\0');
    aggregate.occurrences = counters_[index].exchange(0, std::memory_order_relaxed);
    if (aggregate.occurrences) { sink(argument, aggregate); ++reports; }
  }
  // Include categories whose sample was dropped because the ring was full.
  for (std::size_t i = 0; i < emitted.size(); ++i) {
    if (emitted[i]) continue;
    const auto occurrences = counters_[i].exchange(0, std::memory_order_relaxed);
    if (!occurrences) continue;
    Diagnostic aggregate;
    aggregate.reason = static_cast<Reason>(i / static_cast<std::size_t>(Phase::count));
    aggregate.phase = static_cast<Phase>(i % static_cast<std::size_t>(Phase::count));
    aggregate.action = Action::continued;
    aggregate.aggregate = true;
    aggregate.has_correlation = false;
    aggregate.occurrences = occurrences;
    sink(argument, aggregate);
    ++reports;
  }
  // One correlated example per key. Aggregate counts never attribute all
  // occurrences to this one call/event sample.
  for (std::size_t i = 0; i < sample_count; ++i) {
    auto sample = samples[i];
    const auto index = counter_index(sample.reason, sample.phase);
    bool first = true;
    for (std::size_t prior = 0; prior < i; ++prior)
      first &= counter_index(samples[prior].reason, samples[prior].phase) != index;
    if (!first || !sample.has_correlation) continue;
    sample.aggregate = false;
    sample.occurrences = 1;
    sink(argument, sample);
    ++reports;
  }
  return reports;
}

} // namespace tracer::context_execution
