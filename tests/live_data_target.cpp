#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <dlfcn.h>
#include <pthread.h>

#include "fixtures/live_data/model.hpp"
#include "live_data.hpp"
#include "proc_identity.hpp"

using namespace tracer::live;
using namespace example;
namespace fs = std::filesystem;

namespace {
void check(bool result, const char* message) {
  if (!result) {
    throw std::runtime_error(message);
  }
}

std::size_t count(const LiveDataReport& report, LiveObjectOutcome outcome) {
  return std::count_if(report.objects.begin(), report.objects.end(),
                       [&](const auto& row) { return row.outcome == outcome; });
}

struct Harness {
  fs::path fixtures, sdk;
  LiveDataSession session;
  LiveDataTarget target;

  JitRequest source(std::string_view name) const {
    const auto path = fixtures / name;
    std::ifstream input(path);
    check(bool(input), "fixture source could not be opened");
    JitRequest request;
    request.files.push_back({path.string(), {std::istreambuf_iterator<char>(input), {}}});
    request.compiler_arguments = {"-std=c++23", "-O0", "-I" + sdk.string()};
    // This executable supplies the role of the coordinator's exact import
    // binder. Include only symbols used by the source fixtures.
    for (const char* symbol :
         {"_ZN7example15snapshot_cachesEv",
          "_ZN7example17snapshot_accountsEv",
          "_ZN7example15expired_accountEv",
          "_ZN7example21release_primary_cacheERKSt10shared_ptrINS_5CacheEE",
          "_ZN7example22checked_borrow_accountEPFvRNS_7AccountERKSt11unique_lockISt5mutexEPvES7_",
          "_ZN7example15g_primary_cacheE",
          "_ZN7example16g_active_accountE",
          "_ZN7example11owner_mutexE",
          "_ZN7example5Cache17update_if_presentEii",
          "_ZN7example7Account21set_daily_limit_centsEi",
          "_ZTIN7example7ServiceE",
          "_ZTIN7example5CacheE",
          "__dynamic_cast",
          "pthread_mutex_lock",
          "pthread_mutex_unlock",
          "__libc_single_threaded",
          "_ZTISt9exception",
          "_ZTISt13runtime_error",
          "_ZNSt13runtime_errorC1EPKc",
          "_ZNSt13runtime_errorD1Ev",
          "_ZTVN10__cxxabiv117__class_type_infoE",
          "_ZTVN10__cxxabiv120__si_class_type_infoE"}) {
      if (const auto* address = ::dlsym(RTLD_DEFAULT, symbol)) {
        request.imports.emplace(symbol, reinterpret_cast<std::uintptr_t>(address));
      }
    }
    return request;
  }

  LiveDataReport update(std::string_view file, std::string discovery = "discover",
                        const LiveDataSelection& selection = {}) {
    auto report = session.update_data(
        target, source(file),
        discovery.empty()
            ? std::nullopt
            : std::optional<LiveDataDiscovery>{{source("discover.cpp"), std::move(discovery)}},
        selection);
    for (const auto& row : report.objects) {
      std::cout << static_cast<int>(row.outcome) << " " << row.id << " " << row.type << " "
                << row.root << " " << row.detail << "\n";
    }
    return report;
  }
};

void run_forms(Harness& harness) {
  for (const char* file : {"shared.cpp", "raw.cpp", "refs.cpp"}) {
    const auto report = harness.update(file);
    check(count(report, LiveObjectOutcome::updated) == 5 && report.duplicates == 1 &&
              count(report, LiveObjectOutcome::skipped) == 3 &&
              count(report, LiveObjectOutcome::failed) == 0,
          "unexpected typed update outcomes");
    for (const auto& cache : snapshot_caches()) {
      check(std::dynamic_pointer_cast<Cache>(cache)->price() == 999, "cache write did not persist");
    }
    for (const auto& account : snapshot_accounts()) {
      check(account->limit() == 5000, "account write did not persist");
    }
  }
  const auto account = snapshot_accounts().front();
  const auto filtered = harness.update("refs.cpp", "discover",
                                       {fs::path(harness.target.module).filename(),
                                        reinterpret_cast<std::uintptr_t>(account.get()),
                                        {}});
  check(count(filtered, LiveObjectOutcome::updated) == 1 && account->calls() == 4,
        "module or address filter failed");
  const auto partial = harness.update("partial.cpp");
  check(count(partial, LiveObjectOutcome::updated) == 4 &&
            count(partial, LiveObjectOutcome::failed) == 1,
        "callback error did not preserve the partial result");
  check(std::dynamic_pointer_cast<Cache>(g_primary_cache)->calls() == 4,
        "earlier write missing after callback threw");
  check(count(harness.update("refs.cpp", "borrow_account"), LiveObjectOutcome::updated) == 1,
        "guarded borrow failed");
  check(count(harness.update("shared.cpp", "borrow_account"), LiveObjectOutcome::skipped) == 1,
        "guarded borrow was admitted as shared ownership");
  check(count(harness.update("refs.cpp", "unlocked_account"), LiveObjectOutcome::skipped) == 1,
        "unlocked borrow was admitted");
}

void run_scan(Harness& harness, Account* unowned, const void* spoof, const void* fake,
              const void* freed, const void* plain) {
  const auto scan = harness.session.scan_heap(
      harness.target, {"example::Cache", "example::Account", "example::Plain"},
      LiveDataDiscovery{harness.source("discover.cpp"), "scan_owners"});
  LiveDataSelection select;
  std::string stale, cache_id;
  const auto address_of = [](const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
  };
  for (const auto& row : scan.candidates) {
    std::cout << row.id << " " << row.type << " " << row.address << " " << row.evidence << "\n";
    check(row.address != address_of(fake) && row.address != address_of(plain),
          "invalid type evidence admitted");
    if (row.address == address_of(g_primary_cache.get())) {
      check(row.root == "g_primary_cache", "cache owner missing");
      cache_id = row.id;
    }
    if (row.address == address_of(g_active_account.get())) {
      check(row.root == "g_active_account", "account owner missing");
      stale = row.id;
    }
    if (row.address == address_of(unowned) || row.address == address_of(spoof) ||
        row.address == address_of(freed)) {
      check(row.root.empty(), "unowned heap evidence wrongly acquired an owner");
    }
    select.scan_ids.push_back(row.id);
  }
  check(!cache_id.empty() && !stale.empty(), "known heap owners missing from scan");
  const auto report = harness.update("shared.cpp", "", select);
  check(count(report, LiveObjectOutcome::updated) == 2 && unowned->calls() == 0 &&
            count(report, LiveObjectOutcome::skipped) > 0,
        "scan acted on advisory candidates");

  // New control block on the identical storage. Keep the allocation valid
  // while ending the prior C++ object lifetime and replacing its owner.
  auto* same_address = g_active_account.get();
  g_active_account.reset();
  auto* replacement = std::construct_at(same_address, 9);
  g_active_account = std::shared_ptr<Account>(replacement, [](Account* p) { std::destroy_at(p); });
  check(count(harness.update("shared.cpp", "", {{}, {}, {stale}}), LiveObjectOutcome::skipped) ==
                1 &&
            replacement->calls() == 0,
        "stale scan was redirected to reused storage");

  std::weak_ptr<Service> old = g_primary_cache;
  check(count(harness.update("release.cpp", "", {{}, {}, {cache_id}}),
              LiveObjectOutcome::updated) == 1 &&
            old.expired(),
        "real owner did not keep the cache alive during concurrent removal");
}
} // namespace

int main(int argc, char** argv) try {
  check(argc == 4, "expected fixture directory, SDK include root and target build ID");
  // Force fixture allocation before constructing the in-target service.
  g_primary_cache = std::make_shared<Cache>();
  secondary_cache = std::make_shared<Cache>();
  auto slot = std::unique_ptr<void, decltype(&std::free)>(std::malloc(sizeof(Account)), &std::free);
  check(bool(slot), "cannot allocate account fixture");

  struct ResetAccount {
    ~ResetAccount() { g_active_account.reset(); }
  } reset_account;

  g_active_account =
      std::shared_ptr<Account>(std::construct_at(static_cast<Account*>(slot.get()), 1),
                               [](Account* p) { std::destroy_at(p); });
  accounts = {std::make_shared<Account>(2), std::make_shared<Account>(3)};
  old_owner = std::make_shared<Account>(4);
  const auto unowned = std::make_unique<Account>(5);
  const auto plain = std::make_unique<Plain>();
  auto bytes = std::make_unique<std::byte[]>(sizeof(Account));
  std::memcpy(bytes.get(), static_cast<const void*>(unowned.get()), sizeof(void*));
  auto fake = std::make_unique<std::byte[]>(sizeof(Account));
  std::memset(fake.get(), 0xa5, sizeof(Account));
  void* freed = std::malloc(sizeof(Account));
  check(freed != nullptr, "cannot allocate freed fixture");
  std::memcpy(freed, static_cast<const void*>(unowned.get()), sizeof(void*));
  const auto freed_address = reinterpret_cast<std::uintptr_t>(freed);
  std::free(freed);

  Harness harness{
      fs::canonical(argv[1]),
      fs::canonical(argv[2]),
      {},
      {tracer::proc::start_time("/proc/self/stat"), fs::canonical("/proc/self/exe"), argv[3]}};
  run_forms(harness);
  run_scan(harness, unowned.get(), bytes.get(), fake.get(), reinterpret_cast<void*>(freed_address),
           plain.get());
  std::cout << "live data target passed\n";
  return 0;
} catch (const std::exception& error) {

  std::cerr << error.what() << "\n";
  return 1;
}
