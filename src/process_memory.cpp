#include "process_memory.hpp"

#include "debug_resolver.hpp"
#include "linux_syscalls.hpp"
#include "native_entry.hpp"
#include "proc_identity.hpp"
#include "session.hpp"
#include "tracer/instruction_trace.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <limits>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

namespace tracer::memory {
namespace api = clangtracer::v1;
using Code = grpc::StatusCode;

namespace {
constexpr std::size_t max_maps_bytes = 1024 * 1024;
constexpr std::size_t max_read_bytes = 64 * 1024;
constexpr std::size_t max_scan_bytes = 1024 * 1024;
constexpr std::size_t max_pattern_bytes = 256;
constexpr int default_match_limit = 64;
constexpr int max_match_limit = 1024;

class MemoryError final : public std::runtime_error {
public:
  Code code;
  MemoryError(Code status, const std::string &message)
      : std::runtime_error(message), code(status) {}
};

struct Fd {
  const int value;
  explicit Fd(int fd) : value(fd) {}
  Fd(const Fd &) = delete;
  ~Fd() {
    if (value >= 0)
      ::close(value);
  }
};

[[noreturn]] void io_error(const std::string &operation, int error) {
  if (error == ENOENT || error == ESRCH)
    throw MemoryError(Code::FAILED_PRECONDITION, "target process is no longer available");
  const auto code = error == EACCES || error == EPERM ? Code::PERMISSION_DENIED
                  : error == EFAULT ? Code::OUT_OF_RANGE
                  : Code::UNAVAILABLE;
  throw MemoryError(code, operation + ": " + std::generic_category().message(error));
}

std::string hex_address(std::uint64_t address) {
  std::ostringstream out;
  out << "0x" << std::hex << address;
  return out.str();
}

class Target {
  grpc::ServerContext &context_;
  Session &session_;
  std::chrono::steady_clock::time_point deadline_;

  int open_process() {
    verify_live();
    const int fd = ::open(proc::directory(session_.pid()).c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
      io_error("cannot open target proc directory", errno);
    return fd;
  }

  // Open maps relative to this descriptor so both reads address the same
  // proc entry, even if Linux recycles the PID.
  Fd proc_;

public:
  Target(grpc::ServerContext &context, Session &session,
         std::chrono::steady_clock::time_point deadline)
      : context_(context), session_(session), deadline_(deadline), proc_(open_process()) {}

  void checkpoint() const {
    if (std::chrono::steady_clock::now() >= deadline_ ||
        std::chrono::system_clock::now() >= context_.deadline())
      throw MemoryError(Code::DEADLINE_EXCEEDED, "memory inspection deadline expired");
    if (context_.IsCancelled())
      throw MemoryError(Code::CANCELLED, "memory inspection cancelled");
  }

  void verify_live() const {
    checkpoint();
    if (session_.snapshot().state() != api::Session::ATTACHED)
      throw MemoryError(Code::FAILED_PRECONDITION,
                        "memory inspection requires a live attached session");
    bool same_process = false;
    try {
      same_process = Session::ProcessStartTime(session_.pid()) ==
                     session_.process_start_time();
    } catch (const std::runtime_error &) {
      // Treat an unavailable identity as a failed check.
    }
    if (!same_process) {
      session_.refresh_process_state();
      throw MemoryError(Code::FAILED_PRECONDITION,
                        "target exited or its process identity could not be verified");
    }
    checkpoint();
  }

  std::string maps() const {
    const Fd fd(::openat(proc_.value, "maps", O_RDONLY | O_CLOEXEC));
    if (fd.value < 0)
      io_error("cannot open target mappings", errno);
    std::string result;
    std::array<char, 8192> buffer;
    for (;;) {
      checkpoint();
      const auto count = ::read(fd.value, buffer.data(), buffer.size());
      if (count < 0) {
        if (errno == EINTR)
          continue;
        io_error("cannot read target mappings", errno);
      }
      if (count == 0)
        break;
      if (static_cast<std::size_t>(count) > max_maps_bytes - result.size())
        throw MemoryError(Code::RESOURCE_EXHAUSTED, "target mapping snapshot exceeds 1 MiB");
      result.append(buffer.data(), count);
    }
    if (result.empty())
      throw MemoryError(Code::FAILED_PRECONDITION, "target has no live memory mappings");
    return result;
  }

  std::string read_bytes(std::uint64_t address, std::size_t size) const {
    const auto page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
      throw MemoryError(Code::INTERNAL, "cannot determine native page size");
    std::string result(size, '\0');
    verify_live();
    for (std::size_t offset = 0; offset < size;) {
      checkpoint();
      const auto start = address + offset;
      const auto count = std::min<std::size_t>(size - offset, page_size - start % page_size);
      iovec local{result.data() + offset, count};
      iovec remote{reinterpret_cast<void *>(start), count};
      const auto n = linux_api::process_vm_readv(session_.pid(), &local, 1, &remote, 1, 0);
      if (n < 0) {
        const int error = errno;
        if (error == EINTR)
          continue;
        io_error("cannot read target memory at " + hex_address(start), error);
      }
      if (static_cast<std::size_t>(n) != count)
        throw MemoryError(Code::OUT_OF_RANGE,
                          "short target memory read at " + hex_address(start + n));
      offset += count;
    }
    return result;
  }
};

std::uint64_t parse_number(std::string_view text, int base) {
  std::uint64_t value = 0;
  const auto end = text.data() + text.size();
  const auto parsed = std::from_chars(text.data(), end, value, base);
  if (parsed.ec != std::errc{} || parsed.ptr != end)
    throw MemoryError(Code::ABORTED, "cannot parse target mapping snapshot");
  return value;
}

api::ListMemoryMappingsResponse parse_mappings(const std::string &text,
                                               const Target &target) {
  api::ListMemoryMappingsResponse snapshot;
  std::istringstream lines(text);
  std::string line;
  std::uint64_t previous_end = 0;
  while (std::getline(lines, line)) {
    target.checkpoint();
    std::istringstream row(line);
    std::string range, permissions, offset, device, inode, path;
    if (!(row >> range >> permissions >> offset >> device >> inode) ||
        permissions.size() != 4)
      throw MemoryError(Code::ABORTED, "cannot parse target mapping snapshot");
    const auto dash = range.find('-');
    if (dash == std::string::npos)
      throw MemoryError(Code::ABORTED, "cannot parse target mapping range");
    const std::string_view bounds(range);
    const auto begin = parse_number(bounds.substr(0, dash), 16);
    const auto end = parse_number(bounds.substr(dash + 1), 16);
    if (end <= begin || begin < previous_end)
      throw MemoryError(Code::ABORTED, "target mappings changed while reading snapshot");
    previous_end = end;
    std::getline(row >> std::ws, path);
    auto &mapping = *snapshot.add_mappings();
    mapping.set_begin_address(begin);
    mapping.set_end_address(end);
    mapping.set_permissions(std::move(permissions));
    mapping.set_file_offset(parse_number(offset, 16));
    mapping.set_device(std::move(device));
    mapping.set_inode(parse_number(inode, 10));
    mapping.set_path(std::move(path));
  }
  return snapshot;
}

std::string token_prefix(const Session &session) { return session.id() + ":mem:"; }

std::string mapping_token(const Session &session, const std::string &maps) {
  llvm::SHA256 hash;
  hash.update(maps);
  return token_prefix(session) + llvm::toHex(hash.final(), true);
}

void require_readable(const api::ListMemoryMappingsResponse &snapshot,
                      std::uint64_t begin, std::uint64_t end) {
  for (const auto &mapping : snapshot.mappings()) {
    if (mapping.end_address() <= begin)
      continue;
    if (mapping.begin_address() > begin)
      break;
    if (mapping.permissions().front() != 'r')
      throw MemoryError(Code::PERMISSION_DENIED,
                        "target memory is unreadable at " + hex_address(begin));
    if (mapping.end_address() >= end)
      return;
    begin = mapping.end_address();
  }
  throw MemoryError(Code::OUT_OF_RANGE, "target memory is unmapped at " + hex_address(begin));
}

template <typename Operation>
grpc::Status inspect(grpc::ServerContext &context, Session &session,
                     std::chrono::seconds timeout, Operation operation) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  std::lock_guard lock(session.operation_mutex());
  try {
    Target target(context, session, deadline);
    operation(target);
    return grpc::Status::OK;
  } catch (const MemoryError &error) {
    return {error.code, error.what()};
  } catch (const std::exception &error) {
    return {Code::INTERNAL, error.what()};
  }
}

template <typename Operation>
grpc::Status inspect_snapshot(grpc::ServerContext &context, Session &session,
                              std::chrono::seconds timeout, const std::string &token,
                              Operation operation) {
  const auto prefix = token_prefix(session);
  if (token.compare(0, prefix.size(), prefix) != 0 || token.size() != prefix.size() + 64 ||
      token.find_first_not_of("0123456789abcdef", prefix.size()) != std::string::npos)
    return {Code::INVALID_ARGUMENT, "memory mapping token does not identify this session"};
  return inspect(context, session, timeout, [&](const Target &target) {
    const auto maps = target.maps();
    if (mapping_token(session, maps) != token)
      throw MemoryError(Code::ABORTED, "memory mapping snapshot is stale; run %memory maps again");
    operation(target, parse_mappings(maps, target));
    if (maps != target.maps())
      throw MemoryError(Code::ABORTED, "target mappings changed during inspection; run %memory maps again");
    target.verify_live();
  });
}

template <typename Operation>
grpc::Status inspect_range(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout, const std::string &token,
                           std::uint64_t address, std::uint32_t size, Operation operation) {
  if (!address || size > std::numeric_limits<std::uint64_t>::max() - address)
    return {Code::INVALID_ARGUMENT, "invalid or overflowing memory address range"};
  return inspect_snapshot(context, session, timeout, token,
                          [&](const Target &target, const auto &snapshot) {
    require_readable(snapshot, address, address + size);
    operation(target, target.read_bytes(address, size));
  });
}

using Entry = api::PreviewFunctionEntryResponse;
using Object = llvm::object::ELF64LEObjectFile;
constexpr auto native_abi = "linux-x86-64-sysv";
constexpr std::uint64_t max_entry_image_bytes = 128 * 1024 * 1024;

std::pair<std::uint64_t, Entry::Boundary> function_boundary(
    const Object &object, const Target &target, std::uint64_t address) {
  std::set<std::uint64_t> sizes;
  const auto visit = [&](const llvm::object::SymbolRef &symbol) {
    target.checkpoint();
    const llvm::object::ELFSymbolRef elf(symbol);
    if (elf.getELFType() != llvm::ELF::STT_FUNC || !elf.getSize())
      return;
    auto value = symbol.getAddress();
    auto flags = symbol.getFlags();
    const bool valid_address = static_cast<bool>(value);
    const bool valid_flags = static_cast<bool>(flags);
    if (!valid_address)
      llvm::consumeError(value.takeError());
    if (!valid_flags)
      llvm::consumeError(flags.takeError());
    if (valid_address && valid_flags && *value == address &&
        !(*flags & llvm::object::SymbolRef::SF_Undefined))
      sizes.insert(elf.getSize());
  };
  for (const auto &symbol : object.symbols())
    visit(symbol);
  for (const auto &symbol : object.getDynamicSymbolIterators())
    visit(symbol);
  if (sizes.size() == 1)
    return {*sizes.begin(), Entry::ELF_SYMBOL};
  if (!sizes.empty())
    throw MemoryError(Code::FAILED_PRECONDITION, "ELF symbols disagree on the function extent");

  auto dwarf = llvm::DWARFContext::create(object);
  auto frames = dwarf->getEHFrame();
  if (!frames) {
    llvm::consumeError(frames.takeError());
  } else if (*frames) {
    for (const auto &frame : (*frames)->entries()) {
      target.checkpoint();
      const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&frame);
      if (fde && fde->getInitialLocation() == address && fde->getAddressRange())
        sizes.insert(fde->getAddressRange());
    }
  }
  if (sizes.size() != 1)
    throw MemoryError(Code::FAILED_PRECONDITION,
                      "ELF address is not a unique function entry in symbols or .eh_frame");
  return {*sizes.begin(), Entry::EH_FRAME};
}

bool mapped_build_id_matches(const Target &target,
                             const api::ListMemoryMappingsResponse &snapshot,
                             const Object &object, const api::MemoryMapping &entry,
                             std::uint64_t bias) {
  const auto id = llvm::object::getBuildID(&object);
  if (id.empty() || id.size() > 64)
    return false;
  const auto *file = reinterpret_cast<const std::uint8_t *>(
      object.getMemoryBufferRef().getBufferStart());
  const auto offset = static_cast<std::uint64_t>(id.data() - file);
  auto programs = object.getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    return false;
  }
  for (const auto &segment : *programs) {
    if (segment.p_type != llvm::ELF::PT_LOAD || offset < segment.p_offset)
      continue;
    const auto delta = offset - segment.p_offset;
    if (delta >= segment.p_filesz || id.size() > segment.p_filesz - delta ||
        segment.p_vaddr > UINT64_MAX - bias ||
        delta > UINT64_MAX - bias - segment.p_vaddr)
      continue;
    const auto address = bias + segment.p_vaddr + delta;
    if (id.size() > UINT64_MAX - address)
      continue;
    const auto note_mapping = std::find_if(snapshot.mappings().begin(), snapshot.mappings().end(),
                                           [&](const auto &mapping) {
      return mapping.path() == entry.path() && mapping.inode() == entry.inode() &&
             mapping.device() == entry.device() && mapping.permissions().front() == 'r' &&
             mapping.begin_address() <= address && address + id.size() <= mapping.end_address() &&
             offset >= mapping.file_offset() &&
             offset - mapping.file_offset() == address - mapping.begin_address();
    });
    if (note_mapping != snapshot.mappings().end() &&
        target.read_bytes(address, id.size()) ==
        std::string_view(reinterpret_cast<const char *>(id.data()), id.size()))
      return true;
  }
  return false;
}

void verify_entry_image(const Target &target,
                        const api::ListMemoryMappingsResponse &snapshot, Entry &result) {
  const auto &path = result.module();
  if (std::none_of(snapshot.mappings().begin(), snapshot.mappings().end(),
                   [&](const auto &mapping) {
    return mapping.path() == path && mapping.inode() && mapping.permissions()[2] == 'x';
  }))
    throw MemoryError(Code::FAILED_PRECONDITION, "module path has no live executable mapping");

  // Hold the file descriptor across the read and verify the object identity.
  const Fd fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK));
  if (fd.value < 0)
    throw MemoryError(Code::FAILED_PRECONDITION, "cannot open the mapped ELF file");
  struct stat before{};
  if (::fstat(fd.value, &before) != 0 || !S_ISREG(before.st_mode) ||
      before.st_size <= 0 || static_cast<std::uint64_t>(before.st_size) > max_entry_image_bytes)
    throw MemoryError(Code::FAILED_PRECONDITION, "mapped ELF must be a regular file up to 128 MiB");
  auto buffer = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(before.st_size, path);
  if (!buffer)
    throw MemoryError(Code::FAILED_PRECONDITION, "cannot read the mapped ELF file");
  // Copy from the pinned fd so a concurrent truncate cannot fault an mmap.
  const auto file_size = buffer->getBufferSize();
  for (std::size_t done = 0; done < file_size;) {
    target.checkpoint();
    const auto size = std::min<std::size_t>(file_size - done, 64 * 1024);
    const auto count = ::pread(fd.value, buffer->getBufferStart() + done, size, done);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      throw MemoryError(Code::ABORTED, "cannot read the complete mapped ELF file");
    done += count;
  }
  auto binary = llvm::object::createBinary(buffer->getMemBufferRef());
  if (!binary) {
    llvm::consumeError(binary.takeError());
    throw MemoryError(Code::FAILED_PRECONDITION, "cannot parse the mapped ELF file");
  }
  const auto *object = llvm::dyn_cast<Object>(binary->get());
  if (!object || object->getArch() != llvm::Triple::x86_64)
    throw MemoryError(Code::FAILED_PRECONDITION, "mapped module is not an x86-64 ELF executable or DSO");
  const auto elf_type = object->getELFFile().getHeader().e_type;
  if (elf_type != llvm::ELF::ET_DYN && elf_type != llvm::ELF::ET_EXEC)
    throw MemoryError(Code::FAILED_PRECONDITION, "mapped module is not an x86-64 ELF executable or DSO");
  const auto elf_address = result.elf_address();
  if (!result.function_size()) {
    const auto [size, boundary] = function_boundary(*object, target, elf_address);
    result.set_function_size(size);
    result.set_boundary(boundary);
  }

  const auto file = buffer->getBuffer();
  auto programs = object->getELFFile().program_headers();
  if (!programs) {
    llvm::consumeError(programs.takeError());
    throw MemoryError(Code::FAILED_PRECONDITION, "mapped ELF has invalid program headers");
  }
  std::set<std::pair<std::uint64_t, std::uint64_t>> locations;
  const auto window = std::min<std::uint64_t>(result.function_size(), native_entry::preview_bytes);
  for (const auto &segment : *programs) {
    if (segment.p_type != llvm::ELF::PT_LOAD || !(segment.p_flags & llvm::ELF::PF_X) ||
        elf_address < segment.p_vaddr || segment.p_offset > file.size() ||
        segment.p_filesz > file.size() - segment.p_offset)
      continue;
    const auto delta = elf_address - segment.p_vaddr;
    if (delta >= segment.p_filesz || result.function_size() > segment.p_filesz - delta)
      continue;
    const auto offset = segment.p_offset + delta;
    for (const auto &mapping : snapshot.mappings()) {
      target.checkpoint();
      if (mapping.path() != path || mapping.inode() != before.st_ino ||
          mapping.permissions()[0] != 'r' || mapping.permissions()[2] != 'x' ||
          offset < mapping.file_offset())
        continue;
      const auto mapped_offset = offset - mapping.file_offset();
      const auto span = mapping.end_address() - mapping.begin_address();
      if (mapped_offset >= span || window > span - mapped_offset)
        continue;
      const auto runtime = mapping.begin_address() + mapped_offset;
      if (runtime < elf_address ||
          (elf_type == llvm::ELF::ET_EXEC && runtime != elf_address) ||
          (result.runtime_address() && runtime != result.runtime_address()))
        continue;
      const auto colon = mapping.device().find(':');
      if (colon == std::string::npos)
        continue;
      const std::string_view device(mapping.device());
      const bool same_device = parse_number(device.substr(0, colon), 16) == major(before.st_dev) &&
          parse_number(device.substr(colon + 1), 16) == minor(before.st_dev);
      // OverlayFS can report a different device for the same inode. Require
      // independent live build-ID evidence before accepting that mapping.
      if (!same_device &&
          !mapped_build_id_matches(target, snapshot, *object, mapping, runtime - elf_address))
        continue;
      locations.emplace(runtime, offset);
    }
  }
  if (locations.size() != 1)
    throw MemoryError(Code::FAILED_PRECONDITION, "entry has no unique verified readable executable mapping");
  const auto [runtime, offset] = *locations.begin();
  auto bytes = target.read_bytes(runtime, window);
  if (std::string_view(bytes) != std::string_view(file.data() + offset, window))
    throw MemoryError(Code::ABORTED, "live entry bytes differ from the mapped ELF");
  struct stat after{};
  if (::fstat(fd.value, &after) != 0 || after.st_size != before.st_size ||
      after.st_mtim.tv_sec != before.st_mtim.tv_sec || after.st_mtim.tv_nsec != before.st_mtim.tv_nsec ||
      after.st_ctim.tv_sec != before.st_ctim.tv_sec || after.st_ctim.tv_nsec != before.st_ctim.tv_nsec)
    throw MemoryError(Code::ABORTED, "mapped ELF file changed during entry preview");
  result.set_runtime_address(runtime);
  result.set_entry_bytes(std::move(bytes));
}

void find_matches(const Target &target, std::string_view bytes, std::uint64_t address,
                  std::string_view pattern, std::string_view mask, int limit,
                  api::ScanMemoryResponse &result) {
  if (pattern.size() > bytes.size())
    return;
  struct Check {
    std::size_t offset;
    unsigned char mask;
    unsigned char expected;
  };
  std::vector<Check> checks;
  checks.reserve(pattern.size());
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    const auto bits = static_cast<unsigned char>(mask[i]);
    const auto value = static_cast<unsigned char>(pattern[i]);
    if (bits)
      checks.push_back({i, bits, static_cast<unsigned char>(value & bits)});
  }
  // Test exact bytes first, even when the pattern starts with wildcards.
  std::stable_partition(checks.begin(), checks.end(), [](const Check &check) {
    return check.mask == 0xff;
  });
  const auto last = bytes.size() - pattern.size();
  for (std::size_t start = 0; start <= last; ++start) {
    if (start % 4096 == 0)
      target.checkpoint();
    if (!std::all_of(checks.begin(), checks.end(), [&](const Check &check) {
          return (static_cast<unsigned char>(bytes[start + check.offset]) & check.mask)
                 == check.expected;
        }))
      continue;
    if (result.addresses_size() == limit) {
      result.set_truncated(true);
      break;
    }
    result.add_addresses(address + start);
  }
}
} // namespace

grpc::Status list_mappings(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout,
                           api::ListMemoryMappingsResponse &response) {
  return inspect(context, session, timeout, [&](const Target &target) {
    const auto maps = target.maps();
    auto result = parse_mappings(maps, target);
    if (maps != target.maps())
      throw MemoryError(Code::ABORTED, "target mappings changed during listing; retry");
    target.verify_live();
    result.set_mapping_token(mapping_token(session, maps));
    response.Swap(&result);
  });
}

grpc::Status start_instruction_trace(
    grpc::ServerContext &context, Session &session, std::chrono::seconds timeout,
    const api::StartInstructionTraceRequest &request, api::InstructionTrace &response) {
  const auto begin = request.address();
  const auto size = request.size();
  if (!request.tid() || request.tid() > INT32_MAX || !begin || !size ||
      size > TRACER_INSTRUCTION_MAX_BYTES || size > UINT64_MAX - begin ||
      !request.max_records() || request.max_records() > TRACER_INSTRUCTION_MAX_RECORDS ||
      !request.timeout_seconds() || request.timeout_seconds() > TRACER_INSTRUCTION_MAX_SECONDS)
    return {Code::INVALID_ARGUMENT, "invalid instruction trace thread, range or limits"};
  return inspect(context, session, timeout, [&](const Target &target) {
    const auto maps = target.maps();
    if (mapping_token(session, maps) != request.mapping_token())
      throw MemoryError(Code::ABORTED, "instruction mapping snapshot is stale; run %memory maps again");
    const auto snapshot = parse_mappings(maps, target);
    const auto executable = std::any_of(snapshot.mappings().begin(), snapshot.mappings().end(),
                                       [&](const auto &mapping) {
      return mapping.begin_address() <= begin && begin + size <= mapping.end_address() &&
             mapping.inode() != 0 && mapping.permissions().at(0) == 'r' &&
             mapping.permissions().at(1) == '-' && mapping.permissions().at(2) == 'x';
    });
    if (!executable)
      throw MemoryError(Code::FAILED_PRECONDITION,
                        "instruction range must fit one readable file-backed executable mapping");
    const auto bytes = target.read_bytes(begin, size);
    if (maps != target.maps())
      throw MemoryError(Code::ABORTED, "target mappings changed during setup; run %memory maps again");
    target.verify_live();
    try {
      response = session.start_instruction_trace(request, bytes);
    } catch (const std::invalid_argument &error) {
      throw MemoryError(Code::FAILED_PRECONDITION, error.what());
    }
  });
}

grpc::Status read(grpc::ServerContext &context, Session &session,
                  std::chrono::seconds timeout, const api::ReadMemoryRequest &request,
                  api::ReadMemoryResponse &response) {
  const auto address = request.address();
  const auto size = request.size();
  if (!size || size > max_read_bytes)
    return {Code::INVALID_ARGUMENT, "memory read size must be between 1 and 65536 bytes"};
  api::ReadMemoryResponse result;
  const auto status = inspect_range(context, session, timeout, request.mapping_token(),
                                    address, size, [&](const Target &, std::string bytes) {
    result.set_address(address);
    result.set_data(std::move(bytes));
  });
  if (status.ok())
    response.Swap(&result);
  return status;
}

grpc::Status scan(grpc::ServerContext &context, Session &session,
                  std::chrono::seconds timeout, const api::ScanMemoryRequest &request,
                  api::ScanMemoryResponse &response) {
  if (!request.size() || request.size() > max_scan_bytes)
    return {Code::INVALID_ARGUMENT, "memory scan size must be between 1 and 1048576 bytes"};
  const auto &pattern = request.pattern();
  const auto &mask = request.mask();
  if (pattern.empty() || pattern.size() > max_pattern_bytes || mask.size() != pattern.size())
    return {Code::INVALID_ARGUMENT, "memory scan pattern and mask must have the same length of 1 to 256 bytes"};
  if (request.max_matches() > max_match_limit)
    return {Code::INVALID_ARGUMENT, "memory scan match limit must be at most 1024"};
  const int limit = request.max_matches() ? static_cast<int>(request.max_matches())
                                          : default_match_limit;
  api::ScanMemoryResponse result;
  const auto status = inspect_range(context, session, timeout, request.mapping_token(),
                                    request.address(), request.size(),
                                    [&](const Target &target, const std::string &bytes) {
    find_matches(target, bytes, request.address(), pattern, mask, limit, result);
  });
  if (status.ok())
    response.Swap(&result);
  return status;
}

grpc::Status preview_entry(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout,
                           const api::PreviewFunctionEntryRequest &request, Entry &response) {
  const bool named = request.has_selector();
  const auto valid_text = [](const std::string &text) {
    return text.size() <= 4096 && text.find('\0') == std::string::npos;
  };
  if (!valid_text(request.module()) || !valid_text(request.selector()) ||
      (named && (request.selector().empty() || !request.abi().empty())) ||
      (!named && (!request.has_elf_address() || !request.elf_address() ||
                  request.module().empty() || request.module().front() != '/' ||
                  request.abi() != native_abi)))
    return {Code::INVALID_ARGUMENT,
            "provide a C++ selector or an exact absolute module path, ELF address and linux-x86-64-sysv ABI"};

  Entry result;
  result.set_abi(native_abi);
  const auto status = inspect_snapshot(context, session, timeout, request.mapping_token(),
                                       [&](const Target &target, const auto &snapshot) {
    if (named) {
      client::DebugLookupOptions lookup;
      lookup.module = request.module();
      lookup.max_entries = 1000000;
      lookup.max_depth = 256;
      lookup.is_cancelled = [&] {
        target.checkpoint();
        return false;
      };
      try {
        const auto resolved = client::resolve_debug_callable(session.pid(), request.selector(), lookup);
        const auto &info = resolved.spec.debug_info();
        result.set_module(info.module());
        result.set_selector(info.selector());
        result.set_elf_address(info.entry_virtual_address());
        result.set_runtime_address(info.runtime_entry());
        result.set_function_size(info.entry_size());
        result.set_boundary(Entry::DWARF);
      } catch (const MemoryError &) {
        throw;
      } catch (const client::DebugLookupLimit &error) {
        throw MemoryError(Code::RESOURCE_EXHAUSTED, error.what());
      } catch (const std::exception &error) {
        throw MemoryError(Code::FAILED_PRECONDITION, error.what());
      }
    } else {
      result.set_module(request.module());
      result.set_elf_address(request.elf_address());
    }
    verify_entry_image(target, snapshot, result);
    const auto address = result.runtime_address();
    const auto &bytes = result.entry_bytes();
    const auto analysis = native_entry::preview(address, bytes);
    result.set_patch_size(static_cast<std::uint32_t>(analysis.patch_size));
    result.set_patch_diagnostic(analysis.diagnostic);
    result.set_decode_failed(analysis.decode_failed);
    for (const auto &instruction : analysis.instructions) {
      auto &entry = *result.add_instructions();
      entry.set_address(instruction.address);
      entry.set_encoding(bytes.substr(instruction.address - address, instruction.size));
      entry.set_assembly(instruction.text);
      entry.set_flow(static_cast<api::EntryInstruction::Flow>(instruction.flow));
      if (instruction.target)
        entry.set_target_address(*instruction.target);
      entry.set_falls_through(instruction.falls_through);
    }
    if (target.read_bytes(address, bytes.size()) != bytes)
      throw MemoryError(Code::ABORTED, "live entry bytes changed during preview");
  });
  if (status.ok())
    response.Swap(&result);
  return status;
}
} // namespace tracer::memory
