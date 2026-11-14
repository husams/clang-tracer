#include <algorithm>
#include <mutex>
#include <set>
#include <stdexcept>
#include <utility>

#include "live_data_internal.hpp"

namespace tracer::live {
namespace {
using data::ObjectView;

enum class Scope { all, selected };

struct DiscoveryRun {
  std::string_view fallback;
  std::function<void(const ObjectView&, const std::string&)> registration;

  static void emit(void* context, const ObjectView& object) {
    auto& run = *static_cast<DiscoveryRun*>(context);
    run.registration(object, std::string(object.root.empty() ? run.fallback : object.root));
  }

  std::string run(const std::function<void(clang_tracer::ObjectSink&)>& discover) {
    clang_tracer::ObjectSink sink(this, &emit);
    try {
      discover(sink);
    } catch (const std::exception& error) {
      return *error.what() ? error.what() : "exception with no message";
    } catch (...) {
      return "unknown exception";
    }
    return {};
  }
};

struct SavedCandidate {
  data::Candidate match;
  std::weak_ptr<void> owner;
};

struct SelectionId {
  std::string id;
  const SavedCandidate* saved;
  bool handled = false;
  bool found = false;
};

bool same_owner(const std::weak_ptr<void>& left, const std::shared_ptr<void>& right) {
  return !left.owner_before(right) && !right.owner_before(left);
}

struct UpdateRun {
  const data::TargetImage& target;
  const data::UpdateProgram& program;
  const LiveDataSelection& selection;
  Scope scope;
  LiveDataReport report;
  std::vector<SelectionId> ids;
  std::map<std::pair<std::string, std::uintptr_t>, std::weak_ptr<void>> seen;

  void attempt(const ObjectView& object, const std::string& root, std::string id = {}) {
    using enum LiveObjectOutcome;
    const auto address = reinterpret_cast<std::uintptr_t>(object.object);
    LiveObjectResult result{skipped, std::move(id), std::string(object.type), root, {}, address};
    const auto callback = program.callbacks.find(object.type);
    if (object.skip_reason) {
      result.detail = object.skip_reason;
    } else if (selection.address && selection.address != address) {
      result.detail = "address filter did not match";
    } else if (callback == program.callbacks.end()) {
      result.detail = "no update overload for the discovered type";
    } else if (object.owner == nullptr && !object.scoped) {
      result.detail = "lifetime or mutation scope unverified";
    } else if (callback->second.requires_owner && object.owner == nullptr) {
      result.detail = "update requires an acquired shared owner";
    } else if (!target.verified_layout(object) || object.size != callback->second.size ||
               (object.alignment && object.alignment != callback->second.alignment) ||
               address % callback->second.alignment) {
      result.detail = "type layout differs from the callback or build-matched DWARF";
    } else {
      auto [position, inserted] =
          seen.try_emplace({result.type, address}, object.owner ? std::weak_ptr<void>(*object.owner)
                                                                : std::weak_ptr<void>{});
      if (!inserted) {
        ++report.duplicates;
        result.detail = object.owner && !same_owner(position->second, *object.owner)
                            ? "address already visited with another lifetime or scoped borrow"
                            : "duplicate discovery";
      } else {
        try {
          target.verify();
          callback->second.invoke(object.object, object.owner);
          result.outcome = updated;
        } catch (const std::exception& error) {
          result.outcome = failed;
          result.detail = error.what();
        } catch (...) {
          result.outcome = failed;
          result.detail = "callback threw an unknown exception";
        }
      }
    }
    report.objects.push_back(std::move(result));
  }

  void register_object(const ObjectView& object, const std::string& root) {
    if (std::ranges::find(report.roots, root) == report.roots.end()) {
      report.roots.push_back(root);
    }
    if (scope != Scope::selected) {
      attempt(object, root);
      return;
    }

    const auto address = reinterpret_cast<std::uintptr_t>(object.object);
    for (auto& id : ids) {
      if (!id.saved || id.handled) {
        continue;
      }
      const auto& candidate = id.saved->match;
      if (candidate.report.address != address || candidate.report.type != object.type) {
        continue;
      }
      id.found = true;
      if (!object.owner || root != candidate.report.root ||
          !same_owner(id.saved->owner, *object.owner) || !target.matches(candidate)) {
        continue;
      }
      id.handled = true;
      attempt(object, root, id.id);
    }
  }

  void finish() {
    for (const auto& id : ids) {
      if (id.handled) {
        continue;
      }
      LiveObjectResult result{LiveObjectOutcome::skipped, id.id, {}, {}, {}, 0};
      if (!id.saved) {
        result.detail = "unknown or superseded scan ID";
      } else {
        const auto& candidate = id.saved->match.report;
        result.type = candidate.type;
        result.root = candidate.root;
        result.address = candidate.address;
        if (candidate.root.empty()) {
          result.detail = "no owner identity was verified at scan time; rescan with discovery";
        } else if (id.saved->owner.expired()) {
          result.detail = "scan owner expired; rescan for a fresh identity";
        } else if (id.found) {
          result.detail = "scan evidence or owner identity changed";
        } else {
          result.detail = "scan owner no longer returned by the discovery root";
        }
      }
      report.objects.push_back(std::move(result));
    }
  }
};
} // namespace

struct LiveDataSession::State {
  std::mutex mutex;
  std::map<std::string, SavedCandidate> candidates;
  std::shared_ptr<data::DiscoveryProgram> scan_discovery;
  LiveDataTarget scan_target;
  std::uint64_t sequence = 0;
};

LiveDataSession::LiveDataSession()
    : state_(std::make_unique<State>()) {}

LiveDataSession::~LiveDataSession() = default;

LiveDataReport LiveDataSession::update_data(const LiveDataTarget& selection_target,
                                            const JitRequest& source,
                                            const std::optional<LiveDataDiscovery>& discovery,
                                            const LiveDataSelection& selection) {
  std::unique_lock lock(state_->mutex, std::try_to_lock);
  if (!lock) {
    throw std::runtime_error("another live data operation is active");
  }
  const data::TargetImage target(selection_target);
  if (!target.module_matches(selection.module)) {
    throw std::invalid_argument("module filter differs from the verified target ELF");
  }
  const auto scope = selection.scan_ids.empty() ? Scope::all : Scope::selected;
  const auto program = data::compile_update(source);
  UpdateRun run{target, program, selection, scope, {}, {}, {}};
  run.report.limitations = {
      "Coverage is limited to the named discovery roots.",
      "Object-watch receiver observations are read-only outside their proven writer scope."};

  const bool same_scan_target =
      state_->scan_target.process_start == selection_target.process_start &&
      state_->scan_target.module == selection_target.module &&
      state_->scan_target.build_id == selection_target.build_id;
  std::set<std::string> unique_ids;
  for (const auto& id : selection.scan_ids) {
    if (!unique_ids.insert(id).second) {
      ++run.report.duplicates;
      continue;
    }
    const auto saved = state_->candidates.find(id);
    const SavedCandidate* candidate = nullptr;
    if (same_scan_target && saved != state_->candidates.end()) {
      candidate = &saved->second;
    }
    run.ids.push_back({id, candidate});
  }

  const auto discover =
      discovery ? std::make_shared<data::DiscoveryProgram>(data::compile_discovery(*discovery))
                : (scope == Scope::selected && same_scan_target ? state_->scan_discovery : nullptr);
  data::retain(program.image);
  if (discovery)
    data::retain(discover->image);
  const std::string name = discover ? discover->root : "DWARF atomic shared owners";
  target.verify();
  std::vector<std::string> types;
  for (const auto& [type, callback] : program.callbacks) {
    (void)callback;
    types.push_back(type);
  }
  DiscoveryRun execute{name, [&](const auto& object, const auto& root) {
                         run.register_object(object, root);
                       }};
  auto failure = execute.run([&](clang_tracer::ObjectSink& sink) {
    if (discover)
      discover->invoke(sink);
    else
      target.discover_owners(types, sink);
  });
  if (!failure.empty())
    run.report.objects.push_back(
        {LiveObjectOutcome::failed, {}, {}, name, "discovery failed: " + failure, 0});
  if (run.report.roots.empty())
    run.report.limitations.push_back("No verified discovery root is available for this request.");
  run.finish();
  return std::move(run.report);
}

LiveHeapReport LiveDataSession::scan_heap(const LiveDataTarget& selection_target,
                                          const std::vector<std::string>& types,
                                          const std::optional<LiveDataDiscovery>& discovery) {
  std::unique_lock lock(state_->mutex, std::try_to_lock);
  if (!lock) {
    throw std::runtime_error("another live data operation is active");
  }
  if (types.empty()) {
    throw std::invalid_argument("scan-heap requires at least one type");
  }
  const data::TargetImage target(selection_target);
  auto scan = target.scan(types);
  auto program = discovery
                     ? std::make_shared<data::DiscoveryProgram>(data::compile_discovery(*discovery))
                     : nullptr;
  std::map<std::pair<std::string, std::uintptr_t>, std::weak_ptr<void>> owners;
  std::map<std::pair<std::string, std::uintptr_t>, std::string> roots;
  std::set<std::pair<std::string, std::uintptr_t>> ambiguous;
  {
    if (program)
      data::retain(program->image);
    target.verify();
    DiscoveryRun execute{program ? std::string_view(program->root) : "DWARF atomic shared owners",
                         [&](const auto& object, const auto& root) {
                           if (!object.owner || !target.verified_layout(object)) {
                             return;
                           }
                           const auto key =
                               std::make_pair(std::string(object.type),
                                              reinterpret_cast<std::uintptr_t>(object.object));
                           auto [prior, inserted] = owners.try_emplace(key, *object.owner);
                           if (inserted) {
                             roots.emplace(key, root);
                           } else if (!same_owner(prior->second, *object.owner)) {
                             ambiguous.insert(key);
                           }
                         }};
    auto failure = execute.run([&](clang_tracer::ObjectSink& sink) {
      if (program)
        program->invoke(sink);
      else
        target.discover_owners(types, sink);
    });
    if (!failure.empty()) {
      scan.limitations.push_back("owner discovery failed: " + failure);
    }
  }

  LiveHeapReport result{{}, std::move(scan.limitations)};
  std::map<std::string, SavedCandidate> next;
  for (auto& candidate : scan.candidates) {
    auto& report = candidate.report;
    if (state_->sequence == UINT64_MAX) {
      throw std::runtime_error("live heap scan ID space exhausted");
    }
    report.id = "H" + std::to_string(++state_->sequence);
    const auto key = std::make_pair(report.type, report.address);
    std::weak_ptr<void> owner;
    const auto found = owners.find(key);
    if (found != owners.end() && !ambiguous.contains(key)) {
      // Re-read candidate memory while the acquired owner keeps the view alive.
      if (const auto hold = found->second.lock(); hold && target.matches(candidate)) {
        owner = found->second;
        report.root = roots.at(key);
        report.evidence =
            "matching DWARF size, ELF primary vtable and RTTI; shared owner observed under " +
            report.root;
      }
    }
    result.candidates.push_back(report);
    const auto id = report.id;
    next.emplace(id, SavedCandidate{std::move(candidate), std::move(owner)});
  }
  target.verify();
  state_->candidates = std::move(next);
  state_->scan_discovery = std::move(program);
  state_->scan_target = selection_target;
  return result;
}
} // namespace tracer::live
