#include "context_execution.hpp"

#include <stdexcept>

namespace {
using namespace tracer::context_execution;
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
struct Reports {
  std::uint64_t aggregate_count = 0;
  std::uint64_t sample_count = 0;
  std::uint64_t sample_call_id = 0;
  std::uint64_t event_id = 0;
  bool aggregate_unattributed = false;
  bool sample_correlated = false;
};
void collect(void *opaque, const Diagnostic &diagnostic) noexcept {
  auto &reports = *static_cast<Reports *>(opaque);
  if (diagnostic.aggregate) {
    reports.aggregate_count += diagnostic.occurrences;
    reports.aggregate_unattributed |= !diagnostic.has_correlation &&
                                      diagnostic.event_id == 0;
  } else {
    ++reports.sample_count;
    reports.sample_call_id = diagnostic.call_id;
    reports.event_id = diagnostic.event_id;
    reports.sample_correlated = diagnostic.has_correlation;
  }
}
} // namespace

void verify_context_execution() {
  require(current().mode == Mode::Application,
          "execution mode did not start as application");
  require(check_entry(Callee::ApplicationEndpoint) == EntryDecision::Trace,
          "normal application dispatch was not traceable");
  require(check_entry(Callee::AdmittedSupport) == EntryDecision::Bypass,
          "engine support entry was traced from application provenance");
  {
    AccessLease access;
    require(!access.entered() && current().access_depth == 0,
            "application code acquired a tracking state-access lease");
  }
  {
    ControlLease control;
    Scope lifecycle(Mode::LifecycleNoTrace);
    require(control.entered() && !lifecycle.entered(),
            "LifecycleNoTrace started while an engine control gate was held");
  }
  int hook = 0;
  int nested = 0;
  {
    Scope tracking(Mode::Tracking);
    require(tracking.entered() && current().mode == Mode::Tracking,
            "tracking provenance was not installed");
    require(current().suppression_depth == 1,
            "tracking suppression depth was not incremented");
    require(!context_access_allowed(), "context access escaped CurrentHook");
    {
      CurrentHook current_hook(&hook);
      require(current_hook.installed() && context_access_allowed(),
              "CurrentHook did not authorize tracking state access");
      require(event_allowed(runtime_diagnostics(), "tracking.event"),
              "event() was rejected inside an active tracking hook");
      require(check_entry(Callee::ApplicationEndpoint) == EntryDecision::Reject,
              "tracking callback reentered an application endpoint");
      require(check_entry(Callee::AdmittedSupport) == EntryDecision::Bypass,
              "admitted support call was not bypassed");
      {
        AccessLease access;
        OriginalDispatch rejected;
        require(!rejected.entered(),
                "original dispatch started while an access guard was held");
      }
      {
        OriginalDispatch dispatch;
        require(dispatch.entered() && current().mode == Mode::Application &&
                    current().current_hook == nullptr && !context_access_allowed() &&
                    current().suppression_depth == 0,
                "unguarded original dispatch did not suspend tracking");
        require(check_entry(Callee::ApplicationEndpoint) == EntryDecision::Trace,
                "nested application call from original target was suppressed");
        {
          Scope nested_tracking(Mode::Tracking);
          CurrentHook nested_hook(&nested);
          require(nested_tracking.entered() && nested_hook.installed() &&
                      current().current_hook == &nested,
                  "nested target callback could not install its own hook");
        }
      }
      require(current().mode == Mode::Tracking && context_access_allowed(),
              "tracking provenance was not restored after target dispatch");
      require(current().suppression_depth == 1,
              "tracking suppression depth was not restored after target dispatch");
      {
        Scope support(Mode::Support);
        require(support.entered() && current().mode == Mode::Support,
                "trusted support provenance was not installed");
        require(check_entry(Callee::ApplicationEndpoint) == EntryDecision::Reject,
                "support mode admitted application endpoint reentry");
        require(check_entry(Callee::AdmittedSupport) == EntryDecision::Bypass,
                "support mode rejected its admitted callees");
      }
      const auto before_no_hook = runtime_diagnostics().count(
          Reason::no_active_hook, Phase::lifecycle);
      {
        Scope lifecycle(Mode::LifecycleNoTrace);
        require(lifecycle.entered() && current().mode == Mode::LifecycleNoTrace &&
                    current().current_hook == nullptr && !context_access_allowed() &&
                    current().suppression_depth == 2,
                "LifecycleNoTrace did not mask the active hook");
        require(check_entry(Callee::AdmittedLifecycleBoundary) == EntryDecision::Bypass &&
                    check_entry(Callee::ApplicationEndpoint) == EntryDecision::Reject,
                "lifecycle boundary admission did not enforce provenance");
        require(!event_allowed(runtime_diagnostics(), "lifecycle.event"),
                "lifecycle event() captured without an active hook");
      }
      require(context_access_allowed() && current().current_hook == &hook &&
                  runtime_diagnostics().count(Reason::no_active_hook,
                                              Phase::lifecycle) == before_no_hook + 1,
              "LifecycleNoTrace failed to restore CurrentHook or count event()");
    }
    require(!context_access_allowed(), "CurrentHook survived hook exit");
    require(current().mode == Mode::Tracking && current().current_hook == nullptr,
            "tracking mode did not remain after CurrentHook exit");
  }
  require(current().mode == Mode::Application && current().current_hook == nullptr,
          "tracking exit did not restore application provenance");

  Diagnostics diagnostics;
  std::uint64_t dropped_samples = 0;
  for (std::uint64_t id = 1; id <= 200; ++id)
    dropped_samples += !diagnostics.record(Severity::warning,
        Reason::export_queue_full, Phase::exporting, Action::event_dropped,
        42, id, "request.completed");
  require(diagnostics.count(Reason::export_queue_full, Phase::exporting) == 200 &&
              dropped_samples > 0,
          "bounded diagnostics lost its reserved cumulative count");
  Reports reports;
  require(diagnostics.drain_coalesced(collect, &reports) == 0,
          "diagnostic output ran outside TransportNoTrace");
  {
    Scope worker(Mode::TransportNoTrace);
    require(worker.entered() && !context_access_allowed() &&
                check_entry(Callee::ApplicationEndpoint) == EntryDecision::Bypass,
            "transport worker retained tracing callbacks");
    diagnostics.no_active_hook("transport.event");
    const auto emitted = diagnostics.drain_coalesced(collect, &reports);
    require(emitted == 3 && reports.aggregate_count == 201 &&
                reports.aggregate_unattributed && reports.sample_count == 1 &&
                reports.event_id != 0 && reports.sample_call_id == 42 &&
                reports.sample_correlated,
            "diagnostic worker did not coalesce counts and bound samples");
  }
  require(diagnostics.count(Reason::export_queue_full, Phase::exporting) == 0,
          "diagnostic worker did not consume its aggregate counter");
}
