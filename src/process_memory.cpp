#include "process_memory.hpp"

#include "linux_syscalls.hpp"
#include "proc_identity.hpp"
#include "session.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/SHA256.h>

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
grpc::Status inspect_range(grpc::ServerContext &context, Session &session,
                           std::chrono::seconds timeout, const std::string &token,
                           std::uint64_t address, std::uint32_t size, Operation operation) {
  if (!address || size > std::numeric_limits<std::uint64_t>::max() - address)
    return {Code::INVALID_ARGUMENT, "invalid or overflowing memory address range"};
  const auto prefix = token_prefix(session);
  if (token.compare(0, prefix.size(), prefix) != 0 || token.size() != prefix.size() + 64 ||
      token.find_first_not_of("0123456789abcdef", prefix.size()) != std::string::npos)
    return {Code::INVALID_ARGUMENT, "memory mapping token does not identify this session"};
  return inspect(context, session, timeout, [&](const Target &target) {
    const auto maps = target.maps();
    if (mapping_token(session, maps) != token)
      throw MemoryError(Code::ABORTED, "memory mapping snapshot is stale; run %memory maps again");
    require_readable(parse_mappings(maps, target), address, address + size);
    operation(target, target.read_bytes(address, size));
    if (maps != target.maps())
      throw MemoryError(Code::ABORTED, "target mappings changed during inspection; run %memory maps again");
    target.verify_live();
  });
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
} // namespace tracer::memory
