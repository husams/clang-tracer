#include "session.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "linux_syscalls.hpp"
#include <fstream>
#include <iomanip>
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
struct FileDescriptor {
  int fd;
  ~FileDescriptor() { if (fd >= 0) ::close(fd); }
};

std::size_t page_extent(std::uint32_t size) {
  const auto page = static_cast<std::size_t>(::getpagesize());
  return (size + page - 1) / page * page;
}

std::string device_string(dev_t device) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(2) << major(device)
      << ':' << std::setw(2) << minor(device);
  return out.str();
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
api::MemoryAllocation Session::allocate_memory(std::uint32_t size) {
  if (!size || size > 1024 * 1024)
    throw std::invalid_argument("allocation size must be in 1..1048576 bytes");
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_ || instructions_ || page_monitor_)
    throw std::runtime_error("allocation requires an attached session with no active instruction or page capture");
  if (allocations_.size() >= 64)
    throw std::runtime_error("session allocation history reached its limit of 64");
  constexpr long memfd_create = 319; // Linux x86-64 syscall number.
  const FileDescriptor file{static_cast<int>(::syscall(memfd_create, "clang-tracer-memory", 1u /* MFD_CLOEXEC */))};
  if (file.fd < 0 || ::ftruncate(file.fd, page_extent(size)) < 0)
    throw std::system_error(errno, std::generic_category(), "create target allocation backing");
  struct stat info {};
  if (::fstat(file.fd, &info) < 0)
    throw std::system_error(errno, std::generic_category(), "identify target allocation backing");
  const auto path = proc::directory(::getpid()) / "fd" / std::to_string(file.fd);
  AllocationRecord record;
  record.value.set_id(allocations_.size() + 1);
  record.value.set_size(size);
  record.device = device_string(info.st_dev);
  record.inode = info.st_ino;
  allocations_.push_back(std::move(record));
  auto &pending = allocations_.back();
  injector::Process process;
  try {
    attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
    pending.value.set_address(process.remote_map_file(path.string(), page_extent(size)));
    process.detach();
  } catch (...) {
    if (!pending.value.address())
      allocations_.pop_back();
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("target allocation recovery is uncertain; target remains stopped");
    }
    throw;
  }
  return pending.value;
}

void Session::free_memory_locked(AllocationRecord &record,
                                 std::chrono::steady_clock::time_point deadline) {
  if (record.value.freed())
    return;
  if (instructions_ || page_monitor_)
    throw std::runtime_error("stop instruction tracing and page monitoring before freeing memory");
  const auto size = page_extent(record.value.size());
  const auto address = record.value.address();
  for (const auto &patch : memory_patches_)
    if (!patch.value.restored() && overlaps(address, size, patch.value.address(), patch.value.size()))
      throw std::runtime_error("restore patches in the allocation before freeing memory");

  api::MemoryMapping mapping;
  mapping.set_begin_address(address);
  mapping.set_end_address(address + size);
  mapping.set_permissions("rw-p");
  mapping.set_device(record.device);
  mapping.set_inode(record.inode);
  mapping.set_path("/memfd:clang-tracer-memory (deleted)");
  injector::Process process;
  try {
    attach_process(process, deadline);
    process.remote_free(address, size, [&] { verify_patch_mappings(pid_, {mapping}); });
    record.value.set_freed(true);
    process.detach();
  } catch (...) {
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("target free recovery is uncertain; target remains stopped");
    }
    throw;
  }
}

api::MemoryAllocation Session::free_memory(std::uint64_t id) {
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_)
    throw std::runtime_error("free requires a live attached session");
  if (!id || id > allocations_.size())
    throw std::invalid_argument("memory allocation ID is unknown");
  auto &record = allocations_[id - 1];
  free_memory_locked(record, std::chrono::steady_clock::now() + operation_timeout_);
  return record.value;
}

api::TargetLibrary Session::load_target_library(std::string path) {
  if (path.empty() || path.size() > 4096 || path[0] != '/' || path.find('\0') != std::string::npos)
    throw std::invalid_argument("target library requires an absolute local path");
  path = std::filesystem::canonical(path).string();
  const FileDescriptor file{::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)};
  struct stat library {};
  Elf64_Ehdr header {};
  if (file.fd < 0 || ::fstat(file.fd, &library) < 0 || !S_ISREG(library.st_mode) ||
      ::pread(file.fd, &header, sizeof(header), 0) != sizeof(header) ||
      std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
      header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
      header.e_machine != EM_X86_64 || header.e_type != ET_DYN)
    throw std::invalid_argument("target library must be a readable Linux x86-64 ELF shared object");
  std::lock_guard lock(mutex_);
  check_identity_locked();
  if (state_ != api::Session::ATTACHED || unsafe_target_ || instructions_ || page_monitor_)
    throw std::runtime_error("library injection requires an attached session with no active instruction or page capture");
  const auto target_path = proc::directory(pid_) / "root" / path.substr(1);
  struct stat target {};
  if (::stat(target_path.c_str(), &target) < 0 ||
      target.st_ino != library.st_ino || target.st_dev != library.st_dev)
    throw std::invalid_argument("library is not the same file in the target filesystem");
  api::TargetLibrary result;
  result.set_path(path);
  if (resident_libraries_.count(path))
    return result;
  if (resident_libraries_.size() >= 32)
    throw std::runtime_error("session library limit is 32");

  auto &handle = resident_libraries_[path];
  injector::Process process;
  std::uintptr_t scratch = 0;
  const auto scratch_size = path.size() + 1;
  try {
    attach_process(process, std::chrono::steady_clock::now() + operation_timeout_);
    scratch = process.remote_allocate(scratch_size);
    process.remote_write(scratch, path.c_str(), scratch_size);
    handle = process.remote_call(process.resolve_remote_symbol("dlopen"), {scratch, RTLD_NOW | RTLD_LOCAL}, 2);
    process.remote_free(scratch, scratch_size);
    scratch = 0;
    process.detach();
    if (!handle)
      throw std::runtime_error("target dlopen rejected the library or its dependencies");
  } catch (...) {
    if (scratch && !process.unsafe()) {
      try { process.remote_free(scratch, scratch_size); } catch (...) {}
    }
    if (!handle)
      resident_libraries_.erase(path);
    if (process.unsafe()) {
      unsafe_target_ = true;
      set_failed_locked("library loading recovery is uncertain; target remains stopped");
    }
    throw;
  }
  return result;
}
} // namespace tracer
