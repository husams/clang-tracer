#include "session.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "injector.hpp"
#include "proc_identity.hpp"

namespace tracer {
namespace api = clangtracer::v1;
namespace {
bool overlaps(std::uint64_t a, std::size_t size_a, std::uint64_t b, std::size_t size_b) {
  return a < b + size_b && b < a + size_a;
}

void verify_patch_mappings(std::uint32_t pid, const std::vector<api::MemoryMapping> &mappings) {
  std::ifstream input(proc::directory(pid) / "maps");
  std::size_t matched = 0;
  for (std::string line; std::getline(input, line);) {
    std::istringstream row(line);
    std::uint64_t begin = 0, end = 0, offset = 0, inode = 0;
    char dash = 0;
    std::string permissions, device, path;
    if (!(row >> std::hex >> begin >> dash >> end >> permissions >> offset >>
          device >> std::dec >> inode) || dash != '-')
      throw std::runtime_error("cannot verify the target mapping during patch");
    std::getline(row >> std::ws, path);
    for (const auto &mapping : mappings) {
      if (mapping.begin_address() == begin && mapping.end_address() == end &&
          mapping.file_offset() == offset && mapping.inode() == inode &&
          mapping.permissions() == permissions && mapping.device() == device &&
          mapping.path() == path)
        ++matched;
    }
  }
  if (matched != mappings.size())
    throw std::runtime_error("a patched mapping changed before the operation");
}
} // namespace

void Session::write_memory_patch_locked(MemoryPatchRecord &record, bool restore,
                                        std::chrono::steady_clock::time_point deadline) {
  if (instructions_)
    throw std::runtime_error("stop instruction tracing before changing patched memory");
  const auto &before = restore ? record.replacement : record.expected;
  const auto &after = restore ? record.expected : record.replacement;
  injector::Process process;
  try {
    attach_process(process, deadline);
    const injector::CodePatch patch{record.value.address(), before.data(), after.data(), before.size()};
    if (!process.patch_code_batch({&patch, 1}, [&] { verify_patch_mappings(pid_, record.mappings); }))
      throw std::runtime_error("a target thread is executing in the patch range; retry");
    record.value.set_restored(restore);
    process.detach();
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("memory patch recovery is uncertain; target remains stopped");
    }
    throw;
  }
}

api::MemoryPatch Session::apply_memory_patch(std::uint64_t address, std::string expected,
                                            std::string replacement,
                                            std::vector<api::MemoryMapping> mappings) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_)
    throw std::runtime_error("memory patch requires a live attached session");
  if (memory_patches_.size() >= 256)
    throw std::runtime_error("session memory patch history reached its limit of 256");
  for (const auto &[id, probe] : probes_) {
    (void)id;
    if (probe.patch_committed &&
        overlaps(address, expected.size(), probe.value.entry_address(), kMaxPatchBytes))
      throw std::runtime_error("memory patch overlaps a native probe");
  }
  if (pthread_create_patch_committed_ && pthread_create_patch_ &&
      overlaps(address, expected.size(), pthread_create_patch_->entry, pthread_create_patch_->size))
    throw std::runtime_error("memory patch overlaps a tracer-owned hook");
  for (const auto &patch : memory_patches_)
    if (!patch.value.restored() && overlaps(address, expected.size(), patch.value.address(), patch.value.size()))
      throw std::runtime_error("memory patch overlaps an active session patch");
  api::MemoryPatch patch;
  patch.set_id(memory_patches_.size() + 1);
  patch.set_address(address);
  patch.set_size(expected.size());
  patch.set_restored(true);
  auto &record = memory_patches_.emplace_back(
      MemoryPatchRecord{std::move(patch), std::move(expected), std::move(replacement), std::move(mappings)});
  try {
    write_memory_patch_locked(record, false, std::chrono::steady_clock::now() + operation_timeout_);
  } catch (...) {
    if (record.value.restored() && !unsafe_target_)
      memory_patches_.pop_back();
    throw;
  }
  return record.value;
}

api::MemoryPatch Session::restore_memory_patch(std::uint64_t id) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_)
    throw std::runtime_error("memory patch restoration requires a live attached session");
  if (!id || id > memory_patches_.size())
    throw std::invalid_argument("memory patch ID is unknown");
  auto &record = memory_patches_[id - 1];
  if (!record.value.restored())
    write_memory_patch_locked(record, true, std::chrono::steady_clock::now() + operation_timeout_);
  return record.value;
}
} // namespace tracer
