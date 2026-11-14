#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "live_migration.hpp"
#include "live_update.hpp"
#include "state.hpp"

using namespace std::chrono_literals;
namespace managed = tracer::live::managed;
namespace live = tracer::live;

namespace {
void require(bool satisfied, std::string_view error) {
  if (!satisfied) {
    throw std::runtime_error(std::string(error));
  }
}

std::atomic<int> code_version{1};
std::vector<int> events;
std::promise<void>* entered_hook = nullptr;
std::shared_future<void> release_hook;

live::SourceFile source(const std::filesystem::path& path) {
  std::ifstream input(path);
  require(input.good(), "could not read fixture source");
  return {path.string(), {std::istreambuf_iterator<char>(input), {}}};
}

struct Target {
  managed::Registry registry;
  managed::InstanceId a;
  managed::InstanceId b;
  managed::InstanceId c;
  managed::InstanceId singleton;
  managed::InstanceId tag_count;
  managed::InstanceId tag_fraction;

  explicit Target(bool managed_cache = true) {
    using namespace fixture;
    auto lease = registry.access();
    const managed::Schema cache{managed::layout<CacheV1>("cache.1", "fixture::CacheV1"),
                                managed::create_from<CacheV1, Seed, make_v1>};
    if (managed_cache) {
      lease.add_type("Cache", cache);
    }
    lease.add_type("Singleton", cache);
    lease.add_type("UninitializedSingleton", cache);
    lease.add_type("Tagged", {managed::layout<TaggedV1>("tagged.1", "fixture::TaggedV1"),
                              managed::create_default<TaggedV1>});
    if (managed_cache) {
      const Seed search{"search", 700}, catalog{"catalog", 1400}, empty{"empty", 0};
      a = lease.create("Cache", &search);
      b = lease.create("Cache", &catalog);
      c = lease.create("Cache", &empty);
      const auto removed = lease.create("Cache", &empty);
      require(lease.erase("Cache", removed), "fixture removal");
    }
    const Seed one{"singleton", 5};
    singleton = lease.create("Singleton", &one);
    tag_count = lease.create("Tagged");
    tag_fraction = lease.create("Tagged");
    lease.with<TaggedV1>("Tagged", tag_count, "tagged.1", [](auto& tag) {
      tag.kind = TaggedV1::Kind::count;
      tag.count = 51;
    });
    lease.with<TaggedV1>("Tagged", tag_fraction, "tagged.1", [](auto& tag) {
      tag.kind = TaggedV1::Kind::fraction;
      tag.fraction = 2.5;
    });
  }
};

void old_state(Target& target, std::size_t search_count = 700) {
  auto lease = target.registry.access();
  lease.with<fixture::CacheV1>("Cache", target.a, "cache.1", [&](const auto& cache) {
    require(cache.name == "search" && cache.entries.size() == search_count, "search rollback");
  });
  lease.with<fixture::CacheV1>("Cache", target.b, "cache.1", [](const auto& cache) {
    require(cache.name == "catalog" && cache.entries.size() == 1400, "catalog rollback");
  });
  lease.with<fixture::TaggedV1>("Tagged", target.tag_fraction, "tagged.1", [](const auto& tag) {
    require(tag.kind == fixture::TaggedV1::Kind::fraction && tag.fraction == 2.5, "tag rollback");
  });
}

managed::Result apply(managed::Revision& revision, const managed::Publication& code = {}) {
  return revision.apply(std::chrono::steady_clock::now() + 3s, code);
}
} // namespace

extern "C" void fixture_hook_observe(int entries) {
  require(code_version.load() == 27, "hook ran before related code was installed");
  events.push_back(entries);
  if (entered_hook && entries == 700) {
    entered_hook->set_value();
    release_hook.wait();
  }
}

int main(int argc, char** argv) try {
  require(argc == 3, "expected fixture and SDK paths");
  const auto fixtures = std::filesystem::absolute(argv[1]);
  auto request = live::JitRequest{};
  request.files = {source(fixtures / "state.hpp"),
                   source(fixtures / "update.hpp"),
                   source(fixtures / "after.cpp"),
                   source(fixtures / "helper.cpp"),
                   {(fixtures / "header_only.cpp").string(), "#include \"update.hpp\"\n"}};
  request.compiler_arguments = {"-std=c++23", "-O1", "-I" + std::string(argv[2])};
  request.translation_units = {
      {(fixtures / "header_only.cpp").string(), fixtures.string(), request.compiler_arguments}};
  request.imports = {
      {"fixture_hook_observe", reinterpret_cast<std::uintptr_t>(&fixture_hook_observe)}};
  const auto options = managed::parse_update(std::vector<std::string>{
      (fixtures / "update.hpp").string(), "--after-file", (fixtures / "after.cpp").string(),
      "--after-function", "fixture::after", "--for-each", "Cache"});
  require(options.paths.size() == 1, "header-only update options");

  // Code and hooks are materialized by the production ORC service in this
  // process. These callbacks stand in for the coordinator's native entry
  // writer. They publish one JIT entry through an application dispatch slot.
  const std::atomic<int (*)()> original{+[] { return 1; }};
  std::atomic<int (*)()> dispatch{original.load()};
  int (*next)() = nullptr;
  auto select_code = [&](const managed::Revision& revision) {
    next = reinterpret_cast<int (*)()>(revision.image()->lookup("fixture_updated_value"));
  };
  auto install = [&] {
    dispatch = next;
    code_version = dispatch.load()();
    return managed::CodeResult{managed::CodeState::installed, {}};
  };
  auto restore = [&] {
    dispatch = original.load();
    code_version = dispatch.load()();
    return managed::CodeResult{managed::CodeState::installed, {}};
  };
  const managed::Publication code{install, restore};

  auto multi_request = request;
  multi_request.translation_units.push_back(
      {(fixtures / "helper.cpp").string(), fixtures.string(), request.compiler_arguments});
  multi_request.entry_symbols = {"fixture_updated_value"};
  Target target;
  auto stale = managed::stage(target.registry, request);
  auto revision = managed::stage(target.registry, multi_request, options.hook);
  select_code(*revision);
  require(apply(*revision).status == managed::Status::blocked,
          "a related native entry requires coordinated publication");
  {
    std::promise<void> borrowed, finish_borrow;
    auto finish = finish_borrow.get_future();
    auto task = std::async(std::launch::async, [&] {
      auto lease = target.registry.access();
      lease.with<fixture::CacheV1>("Cache", target.a, "cache.1", [&](const auto&) {
        borrowed.set_value();
        finish.wait();
      });
    });
    borrowed.get_future().wait();
    const auto timed_out = revision->apply(std::chrono::steady_clock::now() + 30ms, code);
    finish_borrow.set_value();
    task.get();
    require(timed_out.status == managed::Status::blocked && code_version == 1, "borrow must drain");
  }

  std::promise<void> hook, finish_hook;
  auto observed_hook = hook.get_future();
  entered_hook = &hook;
  release_hook = finish_hook.get_future().share();
  auto update = std::async(std::launch::async, [&] { return apply(*revision, code); });
  if (observed_hook.wait_for(3s) != std::future_status::ready) {
    finish_hook.set_value();
    throw std::runtime_error("first hook never ran: " + update.get().detail);
  }
  auto allocation = std::async(std::launch::async, [&] {
    auto lease = target.registry.access();
    const fixture::Seed seed{"new", 90};
    require(dispatch.load()() == 27, "reader observed old code with new state");
    return lease.create("Cache", &seed);
  });
  const auto allocation_wait = allocation.wait_for(30ms);
  finish_hook.set_value();
  const auto applied = update.get();
  entered_hook = nullptr;
  const auto future = allocation.get();
  require(allocation_wait == std::future_status::timeout, "allocation escaped safe point");
  require(applied.status == managed::Status::applied && applied.instances == 6, applied.detail);
  require(events == std::vector<int>({700, 1400, 0}), "hook order/count/selectivity");
  require(apply(*stale).status == managed::Status::stale,
          "staged update must reject stale revision");
  {
    auto lease = target.registry.access();
    auto check = [&](auto id, std::string_view type, std::size_t size, std::size_t max, int hooks) {
      lease.with<fixture::CacheV2>(type, id, "cache.2", [&](const auto& cache) {
        require(cache.entries.size() == size && cache.max_entries == max &&
                    cache.hook_calls == hooks,
                "unexpected migrated cache or factory defaults");
        require(
            std::all_of(cache.entries.begin(), cache.entries.end(), [](int n) { return n == 42; }),
            "cache values changed");
      });
    };
    check(target.a, "Cache", 700, 4796, 1);
    check(target.b, "Cache", 1400, 1656, 1);
    check(target.c, "Cache", 0, 1024, 1);
    check(target.singleton, "Singleton", 5, 1024, 0);
    check(future, "Cache", 90, 1024, 0);
    const fixture::Seed seed{"lazy", 3};
    check(lease.create("UninitializedSingleton", &seed), "UninitializedSingleton", 3, 1024, 0);
    lease.with<fixture::TaggedV2>("Tagged", target.tag_count, "tagged.2", [](const auto& tag) {
      require(tag.revision == 2 && tag.kind == fixture::TaggedV1::Kind::count && tag.count == 51,
              "tagged count migration");
    });
    lease.with<fixture::TaggedV2>("Tagged", target.tag_fraction, "tagged.2", [](const auto& tag) {
      require(tag.revision == 2 && tag.kind == fixture::TaggedV1::Kind::fraction &&
                  tag.fraction == 2.5,
              "tagged fraction migration");
    });
    lease.with<fixture::CacheV2>("Cache", target.a, "cache.2",
                                 [](auto& cache) { cache.entries.push_back(7); });
  }
  require(revision->revert(std::chrono::steady_clock::now() + 3s).status ==
              managed::Status::blocked,
          "a related native entry requires coordinated revert");
  auto reverted = revision->revert(std::chrono::steady_clock::now() + 3s, {restore, install});
  revision.reset();
  require(reverted.status == managed::Status::applied && reverted.instances == 8, reverted.detail);
  require(dispatch.load()() == 1, "revert did not restore related code");
  old_state(target, 701);
  {
    auto lease = target.registry.access();
    lease.with<fixture::CacheV1>("Cache", future, "cache.1", [](const auto& cache) {
      require(cache.entries.size() == 90, "reverse migration lost post-apply allocation");
    });
    require(lease.erase("Cache", future), "reverse destruction must use live image");
    lease.erase("Cache", target.a);
    const fixture::Seed replacement{"search", 700};
    const auto new_id = lease.create("Cache", &replacement);
    require(new_id > target.a, "managed identities cannot be reused");
    target.a = new_id;
  }

  for (const auto& failure : {"-DFAIL_HOOK", "-DFAIL_INVALID_STATE"}) {
    auto multi = multi_request;
    multi.compiler_arguments.push_back(failure);
    auto failing = managed::stage(target.registry, multi, options.hook);
    select_code(*failing);
    events.clear();
    const auto result = apply(*failing, code);
    require(result.status == managed::Status::blocked && target.registry.generation() == 2,
            "bad hook or validation committed");
    require(events == std::vector<int>({1400}), "failing hook must stop before later instances");
    require(code_version == 1, "hook failure must restore code");
    old_state(target);
  }

  Target second;
  auto multi = multi_request;
  multi.compiler_arguments.push_back("-DFAIL_HOOK");
  auto failing = managed::stage(second.registry, multi, options.hook);
  select_code(*failing);
  events.clear();
  const auto failure = apply(*failing, code);
  require((failure.detail.find("second hook failed") != std::string::npos ||
           failure.detail.find("managed hook rejected state") != std::string::npos) &&
              events == std::vector<int>({700, 1400}),
          "fail on the second hook");
  old_state(second);
  const auto uncertain =
      apply(*failing, {install, [] {
                         return managed::CodeResult{managed::CodeState::restoration_required,
                                                    "test uncertain restore"};
                       }});
  require(uncertain.status == managed::Status::restoration_required,
          "uncertain rollback was hidden");
  auto reader = std::async(std::launch::async, [&] { old_state(second); });
  const auto reader_wait = reader.wait_for(30ms);
  const auto recovered = failing->recover(restore);
  reader.get();
  require(reader_wait == std::future_status::timeout &&
              recovered.status == managed::Status::applied,
          "uncertain rollback must retain the safe point until recovery");

  Target unmanaged(false);
  auto unmanaged_revision = managed::stage(unmanaged.registry, request);
  const auto unsupported = apply(*unmanaged_revision);
  require(unsupported.status == managed::Status::rebuild_required, "unmanaged type was admitted");
  auto header_only = request;
  header_only.translation_units.clear();
  std::erase_if(header_only.files,
                [&](const auto& file) { return file.path != (fixtures / "update.hpp").string(); });
  auto no_hook = managed::stage(second.registry, header_only);
  require(apply(*no_hook).status == managed::Status::applied, "declared init without a hook");
  {
    auto lease = second.registry.access();
    lease.with<fixture::CacheV2>("Cache", second.a, "cache.2", [](const auto& cache) {
      require(cache.max_entries == 1024 && cache.hook_calls == 0, "declared defaults not used");
    });
  }
  std::cout << "managed migration integration passed\n";
  return 0;
} catch (const std::exception& error) {

  std::cerr << error.what() << '\n';
  return 1;
}
