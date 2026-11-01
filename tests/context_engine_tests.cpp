#include <atomic>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

#include "context_engine.hpp"

namespace ce = tracer::context_engine;
void verify_engine_storage();
void verify_context_execution();
void verify_context_scheduler();
void verify_context_packages();

namespace {
void require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <class F> void rejects(ce::ErrorCode code, F &&function) {
  try {
    function();
  } catch (const ce::Error &error) {
    require(error.code() == code, error.what());
    return;
  }
  throw std::runtime_error("expected context rejection");
}

std::string lifecycle;
unsigned cleanup_boundary_calls = 0;
unsigned normal_boundary_calls = 0;

struct Data {
  int value = 10;

  Data() { lifecycle += 'A'; }

  ~Data() noexcept { lifecycle += 'a'; }
};

struct Other {
  int value = 100;

  Other() { lifecycle += 'B'; }

  ~Other() noexcept { lifecycle += 'b'; }
};

struct Failing {
  Failing() { throw 42; }
};

template <class T>
ce::StateRegistration slot(ce::Identity id,
                           ce::Policy policy = ce::Policy::shared) {
  return {id, policy,
          [](const ce::ScopeInit &, std::any &payload) noexcept {
            try {
              payload = std::make_shared<T>();
              return ce::ErrorCode::ok;
            } catch (...) {
              return ce::ErrorCode::construction_failed;
            }
          },
          [](std::any &payload) noexcept { payload.reset(); },
          [](std::any &payload) noexcept -> void * {
            auto *value = std::any_cast<std::shared_ptr<T>>(&payload);
            return value ? value->get() : nullptr;
          }};
}

auto registration(ce::Identity id, std::vector<ce::StateRegistration> slots) {
  auto result = std::make_shared<ce::ScopeRegistration>();
  result->identity = id;
  result->owner_code = std::make_shared<int>(1);
  result->states = std::move(slots);
  return std::shared_ptr<const ce::ScopeRegistration>(result);
}

void plus32(void *payload, void *) {
  static_cast<Data *>(payload)->value += 32;
}

bool invoke_test_cleanup(const void *, void (*function)(void *),
                         void *argument) noexcept {
  ++cleanup_boundary_calls;
  function(argument);
  return true;
}

bool invoke_test_normal(const void *, void (*function)(void *),
                        void *argument) noexcept {
  ++normal_boundary_calls;
  function(argument);
  return true;
}

void nested_invocations_share_and_restore() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11), slot<Other>(12)});
  const ce::SlotRequest data{r, 11};
  {
    ce::InvocationFrame a(context);
    a.create_or_attach(r);
    a.create_or_attach(r); // Two same-scope dependencies create one binding.
    require(context.binding_count() == 1,
            "same-scope dependency not deduplicated");
    require(lifecycle == "AB", "complete scope not constructed once");
    {
      ce::InvocationFrame b(context);
      b.create_or_attach(r);
      require(context.binding_count() == 2, "nested attachment not pushed");
      ce::HookExit exit(context);
      ce::HookAccess access(context, std::span(&data, 1));
      auto &injected = *static_cast<Data *>(access.ref(data));
      require(&injected == context.get(data), "injection/get alias differs");
      context.with(data, plus32, nullptr);
      require(injected.value == 42, "visitor did not reuse guard");
      rejects(ce::ErrorCode::active_access,
              [&] { ce::InvocationFrame forbidden(context); });
    }
    require(lifecycle == "AB", "borrowed cleanup destroyed parent");
    require(context.binding_count() == 1, "parent cursor not restored");
    {
      ce::HookExit exit(context);
      context.with(
          data,
          [](void *p, void *) {
            require(static_cast<Data *>(p)->value == 42,
                    "A POST missed B update");
          },
          nullptr);
    }
    a.pop_all_and_restore();
    a.pop_all_and_restore();
  }
  require(context.binding_count() == 0, "root cursor not restored");
  require(lifecycle == "ABba", "whole-record reverse finalization differs");
}

void constructor_rollback_is_transactional() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto parent = registration(1, {slot<Data>(11)});
  auto first = registration(2, {slot<Other>(21)});
  auto failed = registration(3, {slot<Data>(31), slot<Failing>(32)});
  ce::InvocationFrame a(context);
  a.create_or_attach(parent);
  {
    ce::InvocationFrame b(context);
    b.create_or_attach(first);
    rejects(ce::ErrorCode::construction_failed,
            [&] { b.create_or_attach(failed); });
    require(context.binding_count() == 1,
            "constructor rollback leaked a completed push");
    require(lifecycle == "ABAab", "constructor rollback order differs");
  }
  require(lifecycle == "ABAab", "rollback repeated cleanup");
}

void generations_do_not_reinterpret_live_records() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto old = registration(1, {slot<Data>(11)});
  auto newer = registration(1, {slot<Data>(11), slot<Other>(12)});
  ce::InvocationFrame a(context);
  a.create_or_attach(old);
  {
    ce::InvocationFrame b(context);
    rejects(ce::ErrorCode::incompatible_registration,
            [&] { b.create_or_attach(newer); });
  }
  require(context.binding_count() == 1 && lifecycle == "A",
          "generation mismatch mutated active record");
}

void context_only_consumers_and_complete_plan() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11), slot<Other>(12)});
  const ce::SlotRequest data{r, 11}, other{r, 12};
  ce::InvocationFrame a(context);
  a.create_or_attach(r);
  {
    ce::InvocationFrame b(context); // No constructor dependency.
    ce::HookExit exit(context);
    ce::HookAccess access(context, std::span(&data, 1));
    rejects(ce::ErrorCode::unplanned_access, [&] { context.get(other); });
    rejects(ce::ErrorCode::active_access, [&] { context.init_scope(r); });
    require(context.get(data) == access.ref(data),
            "inherited lookup copied payload");
  }
  {
    ce::HookExit exit(context);
    ce::HookAccess snapshot_plan(context, {}, true);
    require(static_cast<Other *>(context.get(other))->value == 100,
            "visible slot not included in entry plan");
  }
}

void independent_intervals_reject_nested_acquisition() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest request{r, 11};
  ce::InvocationFrame frame(context);
  frame.create_or_attach(r);
  ce::HookExit exit(context);

  struct Args {
    ce::Context &context;
    const ce::SlotRequest &request;
  } args{context, request};

  context.with(
      request,
      [](void *, void *raw) {
        auto &a = *static_cast<Args *>(raw);
        rejects(ce::ErrorCode::active_access,
                [&] { a.context.with(a.request, plus32, nullptr); });
      },
      &args);
  // An exception releases the short interval and allows the next one.
  try {
    context.with(request, [](void *, void *) { throw 42; }, nullptr);
  } catch (int) {
  }
  context.with(request, plus32, nullptr);
}

void end_on_failure_restores_parent_before_publication() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest data{r, 11};
  ce::InvocationFrame a(context);
  a.create_or_attach(r);
  {
    ce::InvocationFrame b(context);
    b.create_or_attach(r);
    {
      ce::HookExit exit(context);
      rejects(ce::ErrorCode::borrowed_end, [&] { context.end_scope(1); });
      context.init_scope(r);
      context.with(data, plus32, nullptr);
    }
    bool published = false;

    struct Publication {
      bool &published;
    } publication{published};

    {
      ce::HookExit exit(
          context,
          [](void *p, bool failed) noexcept {
            auto &publication = *static_cast<Publication *>(p);
            publication.published = failed && lifecycle == "AAa";
          },
          &publication);
      try {
        ce::HookAccess access(context, std::span(&data, 1));
        context.end_scope(1);
        rejects(ce::ErrorCode::repeated_end, [&] { context.end_scope(1); });
        require(static_cast<Data *>(context.get(data))->value == 42,
                "end changed visibility before hook exit");
        throw 42;
      } catch (int) {
        exit.capture_failure();
      }
    }
    require(published,
            "publish did not follow guard release, end and finalization");
    {
      ce::HookExit exit(context);
      rejects(ce::ErrorCode::repeated_end, [&] { context.end_scope(1); });
      context.with(
          data,
          [](void *p, void *) {
            require(static_cast<Data *>(p)->value == 10,
                    "parent not restored after end");
          },
          nullptr);
    }
  }
  require(lifecycle == "AAa", "ended child was destroyed twice");
}

void borrowed_end_status_is_noexcept_under_payload_guard() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest data{r, 11};
  ce::InvocationFrame owner(context);
  owner.create_or_attach(r);
  {
    ce::InvocationFrame child(context);
    child.create_or_attach(r);
    ce::HookExit exit(context);
    ce::HookAccess access(context, std::span(&data, 1));
    const auto status = context.try_end_scope(1);
    require(status == ce::ErrorCode::borrowed_end,
            "borrowed end status did not reject an attached scope");
  }
  require(lifecycle == "A",
          "borrowed end status changed scope lifetime under payload guard");
}

void target_exception_preserves_cleanup() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  int target_calls = 0;
  bool post = false;
  try {
    ce::InvocationFrame frame(context);
    frame.create_or_attach(r);
    {
      ce::HookExit exit(context);
    }
    ++target_calls;
    throw 73;
    post = true;
  } catch (int value) {
    require(value == 73, "target exception changed");
  }
  require(target_calls == 1 && !post && lifecycle == "Aa" &&
              !context.binding_count(),
          "native unwind lifecycle differs");
}

void missing_state_does_not_construct() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest request{r, 11};
  ce::InvocationFrame frame(context);
  ce::HookExit exit(context);
  rejects(ce::ErrorCode::missing_state,
          [&] { context.with(request, plus32, nullptr); });
  require(lifecycle.empty() && !context.binding_count(),
          "context access created state");
}

void payload_destruction_uses_cleanup_boundary_once() {
  lifecycle.clear();
  cleanup_boundary_calls = 0;
  normal_boundary_calls = 0;
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto mutable_registration = std::make_shared<ce::ScopeRegistration>();
  mutable_registration->identity = 71;
  mutable_registration->owner_code = std::make_shared<int>(1);
  mutable_registration->states = {slot<Data>(711)};
  mutable_registration->invoke = &invoke_test_normal;
  mutable_registration->invoke_cleanup = &invoke_test_cleanup;
  std::shared_ptr<const ce::ScopeRegistration> r = mutable_registration;
  {
    ce::InvocationFrame frame(context);
    frame.create_or_attach(r);
  }
  ce::drain_lifecycle();
  require(cleanup_boundary_calls == 1 && normal_boundary_calls == 1 &&
              lifecycle == "Aa",
          "payload destruction did not use exactly one cleanup boundary");
}

void accepted_tasks_finish_after_owner_closes() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context parent(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest data{r, 11};
  auto ledger = std::make_shared<ce::PropagationLedger>(
      ce::PropagationLimits{2, 4, 128 * 1024, 64});
  std::shared_ptr<ce::TaskCapture> task, cancelled;
  {
    ce::InvocationFrame owner(parent);
    owner.create_or_attach(r);
    task = ce::TaskCapture::capture(parent, ledger);
    cancelled = ce::TaskCapture::capture(parent, ledger);
    require(task && cancelled && ledger->outstanding() == 2,
            "task reservation failed");
    require(!ce::TaskCapture::capture(parent, ledger),
            "per-scope capacity was bypassed");
    require(cancelled->cancel_before_start() &&
                !cancelled->cancel_before_start(),
            "duplicate cancellation consumed ownership");
  }
  require(lifecycle == "A", "owner destroyed state retained by accepted task");
  ce::Context worker(diagnostics);
  {
    ce::TaskAttachment attachment(worker, task);
    require(!task->cancel_before_start(),
            "running cancellation released live ownership");
    require(!ce::TaskCapture::capture(worker, ledger),
            "closed scope admitted new work");
    ce::InvocationFrame invocation(worker);
    invocation.create_or_attach(
        r); // An accepted capability permits this after closure.
    ce::HookExit exit(worker);
    worker.with(data, plus32, nullptr);
  }
  require(ledger->outstanding() == 0 && lifecycle == "Aa",
          "terminal task did not finalize exactly once");
  rejects(ce::ErrorCode::invalid_frame,
          [&] { ce::TaskAttachment duplicate(worker, task); });
}

void shared_access_serializes_across_workers() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context parent(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  const ce::SlotRequest data{r, 11};
  auto ledger = std::make_shared<ce::PropagationLedger>();
  ce::InvocationFrame owner(parent);
  owner.create_or_attach(r);
  auto left = ce::TaskCapture::capture(parent, ledger);
  auto right = ce::TaskCapture::capture(parent, ledger);
  std::atomic<unsigned> ready{0};
  auto work = [&](std::shared_ptr<ce::TaskCapture> task) {
    ce::Context worker(diagnostics);
    ce::TaskAttachment attachment(worker, std::move(task));
    ce::InvocationFrame invocation(worker);
    ++ready;
    while (ready.load() != 2) {
      std::this_thread::yield();
    }
    for (unsigned i = 0; i < 5000; ++i) {
      ce::HookExit exit(worker);
      worker.with(
          data, [](void *p, void *) { ++static_cast<Data *>(p)->value; },
          nullptr);
    }
  };
  std::thread a(work, left), b(work, right);
  a.join();
  b.join();
  require(ledger->outstanding() == 0, "completed workers retained task quota");
  ce::HookExit exit(parent);
  parent.with(
      data,
      [](void *p, void *) {
        require(static_cast<Data *>(p)->value == 10010,
                "shared updates were lost");
      },
      nullptr);
}

void confined_chain_rejects_cross_domain_without_partial_attach() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context parent(diagnostics);
  auto r =
      registration(1, {slot<Data>(11), slot<Other>(12, ce::Policy::confined)});
  auto ledger = std::make_shared<ce::PropagationLedger>();
  ce::InvocationFrame owner(parent);
  owner.create_or_attach(r);
  auto task = ce::TaskCapture::capture(parent, ledger);
  std::atomic<bool> rejected{false};
  std::thread worker([&] {
    ce::Context context(diagnostics);
    rejects(ce::ErrorCode::wrong_domain,
            [&] { ce::TaskAttachment attachment(context, task); });
    rejected = context.binding_count() == 0;
    rejects(ce::ErrorCode::wrong_domain,
            [&] { ce::InvocationFrame invalid(parent); });
  });
  worker.join();
  require(rejected && ledger->outstanding() == 0 && lifecycle == "AB",
          "confinement partially attached or destroyed the record");
}

void task_isolation_restores_unrelated_worker_context() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context parent(diagnostics), worker(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  auto ledger = std::make_shared<ce::PropagationLedger>();
  ce::InvocationFrame owner(parent);
  owner.create_or_attach(r);
  auto task = ce::TaskCapture::capture(parent, ledger);
  const ce::SlotRequest data{r, 11};
  ce::InvocationFrame worker_owner(worker);
  worker_owner.create_or_attach(r); // Same registration, distinct record.
  {
    ce::TaskAttachment attachment(worker, task);
    ce::InvocationFrame invocation(worker);
    ce::HookExit exit(worker);
    worker.with(data, plus32, nullptr);
  }
  require(worker.binding_count() == 1, "worker cursor not restored after task");
  {
    ce::HookExit exit(worker);
    worker.with(
        data,
        [](void *p, void *) {
          require(static_cast<Data *>(p)->value == 10,
                  "task used unrelated worker state");
        },
        nullptr);
  }
  {
    ce::HookExit exit(parent);
    parent.with(
        data,
        [](void *p, void *) {
          require(static_cast<Data *>(p)->value == 42,
                  "task did not use its inherited record");
        },
        nullptr);
  }
}

void cancellation_races_execution_once() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context parent(diagnostics);
  auto r = registration(1, {slot<Data>(11)});
  auto ledger = std::make_shared<ce::PropagationLedger>();
  ce::InvocationFrame owner(parent);
  owner.create_or_attach(r);
  for (unsigned round = 0; round < 100; ++round) {
    auto task = ce::TaskCapture::capture(parent, ledger);
    std::atomic<unsigned> wins{0};
    std::thread cancel([&] {
      if (task->cancel_before_start()) {
        ++wins;
      }
    });
    std::thread execute([&] {
      ce::Context worker(diagnostics);
      try {
        ce::TaskAttachment attachment(worker, task);
        ++wins;
      } catch (const ce::Error &e) {
        require(e.code() == ce::ErrorCode::invalid_frame,
                "unexpected start failure");
      }
    });
    cancel.join();
    execute.join();
    require(wins == 1 && ledger->outstanding() == 0,
            "cancellation/start race released ownership twice");
  }
}

void finalization_waits_until_all_thread_guards_release() {
  lifecycle.clear();
  ce::Diagnostics diagnostics;
  ce::Context guarded(diagnostics), retiring(diagnostics);
  auto a = registration(1, {slot<Data>(11)});
  auto b = registration(2, {slot<Other>(21)});
  const ce::SlotRequest data{a, 11};
  ce::InvocationFrame parent(guarded);
  parent.create_or_attach(a);
  std::optional<ce::InvocationFrame> victim;
  victim.emplace(retiring);
  victim->create_or_attach(b);
  {
    ce::HookExit exit(guarded);
    ce::HookAccess access(guarded, std::span(&data, 1));
    victim.reset();
    require(lifecycle == "AB",
            "unrelated payload finalized under a thread guard");
  }
  require(lifecycle == "ABb",
          "retired payload did not finalize at the safe point");
}

void closure_and_admission_race_cannot_resurrect_records() {
  for (unsigned round = 0; round < 20; ++round) {
    lifecycle.clear();
    ce::Diagnostics diagnostics;
    ce::Context context(diagnostics);
    auto r = registration(1, {slot<Data>(11)});
    auto ledger = std::make_shared<ce::PropagationLedger>();
    ce::InvocationFrame frame(context);
    frame.create_or_attach(r);
    auto accepted = ce::TaskCapture::capture(context, ledger);
    std::atomic<bool> ready{false}, race{false}, closed{false},
        resurrected{false};
    std::thread worker([&] {
      ce::Context task_context(diagnostics);
      ce::TaskAttachment attachment(task_context, accepted);
      ready = true;
      while (!race)
        std::this_thread::yield();
      auto contender = ce::TaskCapture::capture(task_context, ledger);
      if (contender)
        contender->cancel_before_start();
      while (!closed)
        std::this_thread::yield();
      resurrected =
          static_cast<bool>(ce::TaskCapture::capture(task_context, ledger));
    });
    while (!ready)
      std::this_thread::yield();
    race = true;
    {
      ce::HookExit exit(context);
      context.end_scope(1);
    }
    closed = true;
    worker.join();
    require(!resurrected && ledger->outstanding() == 0 && lifecycle == "Aa",
            "closure/admission race resurrected state or leaked ownership");
  }
}

void metadata_limit_reserves_complete_task_restoration() {
  ce::Diagnostics diagnostics;
  ce::Context context(diagnostics);
  auto ledger = std::make_shared<ce::PropagationLedger>(
      ce::PropagationLimits{64, 4096, 1, 64});
  require(!ce::TaskCapture::capture(context, ledger) &&
              ledger->outstanding() == 0,
          "task without complete metadata capacity was admitted");
}
} // namespace

int main() {
  static_assert(!std::is_copy_constructible_v<ce::InvocationFrame> &&
                !std::is_move_constructible_v<ce::InvocationFrame>);
  static_assert(!std::is_copy_constructible_v<ce::HookAccess> &&
                !std::is_move_constructible_v<ce::HookAccess>);
  static_assert(!std::is_copy_constructible_v<ce::HookExit> &&
                !std::is_move_constructible_v<ce::HookExit>);
  try {
    verify_context_execution();
    verify_context_scheduler();
    verify_context_packages();
    verify_engine_storage();
    nested_invocations_share_and_restore();
    constructor_rollback_is_transactional();
    generations_do_not_reinterpret_live_records();
    context_only_consumers_and_complete_plan();
    independent_intervals_reject_nested_acquisition();
    end_on_failure_restores_parent_before_publication();
  borrowed_end_status_is_noexcept_under_payload_guard();
    target_exception_preserves_cleanup();
    missing_state_does_not_construct();
    payload_destruction_uses_cleanup_boundary_once();
    accepted_tasks_finish_after_owner_closes();
    shared_access_serializes_across_workers();
    confined_chain_rejects_cross_domain_without_partial_attach();
    task_isolation_restores_unrelated_worker_context();
    cancellation_races_execution_once();
    finalization_waits_until_all_thread_guards_release();
    closure_and_admission_race_cannot_resurrect_records();
    metadata_limit_reserves_complete_task_restoration();
    std::cout << "17 context lifecycle/access/propagation checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
