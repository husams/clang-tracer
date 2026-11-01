#include "engine_storage.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ce = tracer::context_engine;
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

void verify_engine_storage() {
  ce::EngineStorage storage(256 * 1024);
  require(storage.mapped_bytes() <= 256 * 1024 &&
              storage.payload_capacity() < storage.mapped_bytes(),
          "storage metadata escaped its budget");
  auto first = storage.allocate(1, 31);
  auto aligned = storage.allocate(1, 1025, 4096);
  require(first.pointer && aligned.pointer &&
              reinterpret_cast<std::uintptr_t>(aligned.pointer) % 4096 == 0,
          "private allocation/alignment failed");
  std::memset(first.pointer, 0x57, 31);
  require(storage.release(2, first.pointer) ==
                  ce::StorageStatus::wrong_domain &&
              storage.owns_live(1, first.pointer),
          "foreign domain released private storage");
  require(storage.release(1, static_cast<std::byte *>(first.pointer) + 1) ==
              ce::StorageStatus::foreign_pointer,
          "interior pointer was accepted");
  require(storage.release(1, first.pointer, 32) ==
              ce::StorageStatus::invalid_release,
          "mismatched sized release was accepted");
  require(static_cast<unsigned char *>(first.pointer)[0] == 0x57,
          "rejected release changed live bytes");
  int application_value = 0;
  require(storage.release(1, &application_value) ==
              ce::StorageStatus::foreign_pointer,
          "application allocation entered the private release domain");
  require(storage.release(1, first.pointer, 31) == ce::StorageStatus::ok &&
              storage.release(1, first.pointer) ==
                  ce::StorageStatus::invalid_release,
          "private storage did not reject duplicate release");
  require(storage.release(1, aligned.pointer, 1025, 4096) ==
              ce::StorageStatus::ok,
          "aligned private release failed");
  auto complete = storage.allocate(1, storage.payload_capacity(), 1);
  require(complete.pointer && !storage.allocate(1, 1).pointer,
          "complete capacity could not be reused or exceeded its ceiling");
  require(storage.release(1, complete.pointer) == ce::StorageStatus::ok &&
              storage.occupied_bytes() == 0,
          "complete release leaked capacity");
  require(!storage.allocate(1, 1, 3).pointer &&
              !storage.allocate(1, ce::EngineStorage::kDefaultBudget).pointer,
          "invalid alignment/oversized request escaped bounded admission");

  std::atomic<unsigned> errors{0};
  std::array<std::thread, 4> threads;
  for (unsigned t = 0; t < threads.size(); ++t) {
    threads[t] = std::thread([&, t] {
      for (unsigned i = 0; i < 2000; ++i) {
        ce::EngineStorage::Allocation allocation;
        do {
          allocation = storage.allocate(t + 1, 300 + i % 500, 64);
          if (!allocation.pointer)
            std::this_thread::yield();
        } while (!allocation.pointer);
        const auto size = 300 + i % 500;
        std::memset(allocation.pointer, static_cast<int>(t + 1), size);
        if (!storage.owns_live(t + 1, allocation.pointer) ||
            static_cast<unsigned char *>(allocation.pointer)[size - 1] != t + 1)
          ++errors;
        if (storage.release(t + 1, allocation.pointer, size, 64) !=
            ce::StorageStatus::ok)
          ++errors;
      }
    });
  }
  for (auto &thread : threads)
    thread.join();
  require(!errors && storage.occupied_bytes() == 0,
          "concurrent private allocation ownership corrupted");

  auto one = ce::StorageDomain::create();
  auto two = ce::StorageDomain::create();
  auto large = one->allocate(40 * 1024 * 1024, 64);
  require(large.pointer && !two->allocate(40 * 1024 * 1024, 64).pointer,
          "production allocation domains did not share the global ceiling");
  require(two->release(large.pointer) == ce::StorageStatus::wrong_domain &&
              one->owns_live(large.pointer) && one->outstanding() == 1 &&
              two->outstanding() == 0,
          "production allocation domain ownership was confused");
  require(one->release(large.pointer) == ce::StorageStatus::ok &&
              !one->outstanding(),
          "global allocation domain leaked");
  large = two->allocate(40 * 1024 * 1024, 64);
  require(large.pointer &&
              two->release(large.pointer) == ce::StorageStatus::ok &&
              !two->outstanding(),
          "global capacity was not reused across generations");
}
