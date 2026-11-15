#include "native_stack.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFUnwindTable.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "linux_syscalls.hpp"
#include "tracer/compat.hpp"

namespace {
using namespace llvm::ELF;
#if defined(__aarch64__)
constexpr unsigned native_sp = 31, native_pc = 32, native_return = 30;
constexpr auto native_machine = EM_AARCH64;
constexpr auto call_size = 4;
bool preserved_register(unsigned reg) { return reg >= 19 && reg <= 30; }
#else
constexpr unsigned native_sp = 7, native_pc = 16, native_return = native_pc;
constexpr auto native_machine = EM_X86_64;
constexpr auto call_size = 1;
bool preserved_register(unsigned reg) { return reg == 3 || reg == 6 || (reg >= 12 && reg <= 15); }
#endif
constexpr unsigned native_registers = std::tuple_size_v<tracer::NativeCodeRegisters>;
using RegisterMask = std::uint64_t;
constexpr RegisterMask register_bit(unsigned reg) { return RegisterMask{1} << reg; }

#if defined(__x86_64__)
thread_local tracer::TraceContext *current_context = nullptr;
#endif

[[noreturn]] void stop(const char *detail) { throw std::runtime_error(detail); }

struct Image;
struct Mapping {
  std::uint64_t begin{}, end{}, offset{};
  std::uint64_t device_major{}, device_minor{}, inode{};
  bool readable{}, executable{};
  std::string path;
  // Verified only for this fresh /proc snapshot, never retained across
  // captures.
  mutable std::shared_ptr<Image> verified_image;
};

// Trivial per-thread scratch stays valid for late callbacks without allocating
// a large buffer on the already intercepted application stack.
thread_local std::array<char, 16 * 1024> mapping_bytes;

// Retain at most eight procfs streams, never parsed mappings. Exclusive try-
// ownership avoids sharing a seq_file cursor or waiting for another capture.
// Rewinding to zero discards seq_file output and restarts the current VMA walk.
struct MappingStream {
  std::atomic_flag busy = ATOMIC_FLAG_INIT;
  int fd = -1;
  pid_t pid = 0;
  dev_t device = 0;
  ino_t inode = 0;
};
std::array<MappingStream, 8> mapping_streams;
void reset_mapping_streams() {
  const int saved_errno = errno;
  for (auto &stream : mapping_streams) {
    struct stat identity{};
    if (stream.fd >= 0 && !::fstat(stream.fd, &identity) &&
        identity.st_dev == stream.device && identity.st_ino == stream.inode)
      ::close(stream.fd);
    stream.fd = -1;
    stream.pid = 0;
    stream.busy.clear(std::memory_order_relaxed);
  }
  errno = saved_errno;
}
// A child must not read its parent's mm through an inherited procfs stream.
// If child-hook registration fails, keep using private fresh opens instead.
const bool mapping_pool_enabled =
    ::pthread_atfork(nullptr, nullptr, reset_mapping_streams) == 0;

struct MappingInput {
  MappingStream *stream = nullptr;
  int fd = -1;
  explicit MappingInput(pid_t pid = ::getpid()) {
    if (mapping_pool_enabled)
      for (auto &candidate : mapping_streams)
        if (!candidate.busy.test_and_set(std::memory_order_acquire)) {
          stream = &candidate;
          break;
        }
    if (stream) {
      struct stat identity{};
      if (stream->fd >= 0) {
        if (::fstat(stream->fd, &identity) ||
            identity.st_dev != stream->device ||
            identity.st_ino != stream->inode) {
          // The application closed/reused this number: do not close its FD.
          stream->fd = -1;
        } else if (stream->pid != pid ||
                   ::lseek(stream->fd, 0, SEEK_SET) != 0) {
          ::close(stream->fd);
          stream->fd = -1;
        }
      }
      if (stream->fd < 0) {
        stream->fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (stream->fd >= 0 && ::fstat(stream->fd, &identity)) {
          ::close(stream->fd);
          stream->fd = -1;
        }
        stream->pid = pid;
        stream->device = identity.st_dev;
        stream->inode = identity.st_ino;
      }
      fd = stream->fd;
    } else {
      // All slots busy: take the original private path immediately.
      fd = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    }
  }
  ~MappingInput() {
    if (stream)
      stream->busy.clear(std::memory_order_release);
    else if (fd >= 0)
      ::close(fd);
  }
  MappingInput(const MappingInput &) = delete;
  MappingInput &operator=(const MappingInput &) = delete;
};

// Small contiguous vectors let legacy seq_file readv implementations continue
// past their page-sized short reads in one syscall. Every byte still comes
// from this capture's freshly rewound stream; a short read is never EOF proof.
ssize_t read_mapping_chunk(int fd, char *data, std::size_t size) {
  std::array<iovec, 5> vectors;
  unsigned count = 0;
  std::size_t offset = 0;
  while (offset < size && count < vectors.size()) {
    const auto amount = std::min<std::size_t>(3840, size - offset);
    vectors[count++] = {data + offset, amount};
    offset += amount;
  }
  const auto amount = ::readv(fd, vectors.data(), count);
  if (amount < 0 && (errno == ENOSYS || errno == EOPNOTSUPP ||
                     errno == EINVAL || errno == EPERM || errno == EACCES))
    return ::read(fd, data, size);
  return amount;
}

std::vector<Mapping> mappings(std::uint64_t stack_pointer = 0,
                              pid_t process = ::getpid()) {
  MappingInput input(process);
  const int fd = input.fd;
  if (fd < 0)
    stop("cannot read native mappings");
  std::vector<Mapping> result;
  result.reserve(32);
  const auto parse = [&](std::string_view line) {
    Mapping m;
    const char *cursor = line.data(), *end = cursor + line.size();
    const auto spaces = [&] {
      while (cursor != end && (*cursor == ' ' || *cursor == '\t'))
        ++cursor;
    };
    const auto number = [&](auto &value, int base) {
      const auto parsed = std::from_chars(cursor, end, value, base);
      if (parsed.ec != std::errc{})
        stop("invalid native mapping");
      cursor = parsed.ptr;
    };
    const auto separator = [&](char value) {
      if (cursor == end || *cursor++ != value)
        stop("invalid native mapping");
    };
    number(m.begin, 16);
    separator('-');
    number(m.end, 16);
    separator(' ');
    spaces();
    if (end - cursor < 5 || cursor[4] != ' ')
      stop("invalid native mapping");
    m.readable = cursor[0] == 'r';
    m.executable = cursor[2] == 'x';
    // Unwinding needs executable owners and this invocation's stack VMA only.
    // Refresh the input on every capture so the ranges match the live process.
    if (!m.executable &&
        !(m.readable && m.begin <= stack_pointer && stack_pointer < m.end))
      return;
    cursor += 5;
    spaces();
    number(m.offset, 16);
    separator(' ');
    spaces();
    number(m.device_major, 16);
    separator(':');
    number(m.device_minor, 16);
    separator(' ');
    spaces();
    number(m.inode, 10);
    if (cursor != end && *cursor != ' ' && *cursor != '\t')
      stop("invalid native mapping");
    spaces();
    m.path.assign(cursor, end);
    result.push_back(std::move(m));
  };
  std::size_t carry = 0;
  std::string long_line;
  for (;;) {
    const auto amount = read_mapping_chunk(fd, mapping_bytes.data() + carry,
                                           mapping_bytes.size() - carry);
    if (amount < 0) {
      if (errno == EINTR)
        continue;
      stop("cannot read native mappings");
    }
    const auto total = carry + static_cast<std::size_t>(amount);
    std::size_t begin = 0;
    while (const auto *newline = static_cast<const char *>(std::memchr(
               mapping_bytes.data() + begin, '\n', total - begin))) {
      const auto end = static_cast<std::size_t>(newline - mapping_bytes.data());
      const std::string_view line(mapping_bytes.data() + begin, end - begin);
      if (long_line.empty()) {
        parse(line);
      } else {
        long_line.append(line);
        parse(long_line);
        long_line.clear();
      }
      begin = end + 1;
    }
    carry = total - begin;
    if (!amount) {
      if (carry || !long_line.empty()) {
        long_line.append(mapping_bytes.data() + begin, carry);
        parse(long_line);
      }
      break;
    }
    if (carry == mapping_bytes.size()) {
      // Preserve unusually long mapping names and chunk-spanning records.
      long_line.append(mapping_bytes.data(), carry);
      carry = 0;
    } else if (carry) {
      std::memmove(mapping_bytes.data(), mapping_bytes.data() + begin, carry);
    }
  }
  return result;
}

#if defined(__x86_64__)
bool runtime_module(std::string_view path) {
  std::string name(path.substr(path.find_last_of('/') + 1));
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  // Same runtime families excluded by native admission, plus loader/transport
  // libraries. SONAME is checked too, so renaming a runtime does not hide it.
  constexpr std::string_view prefixes[] = {
      "libc-",       "libpthread-",   "libdl-",    "ld-",          "libm-",       "librt-",
      "libc.so",     "libpthread.so", "libdl.so",  "libstdc++.so", "libgcc_s.so", "libgcc.so",
      "libc++.so",   "libc++abi.so",  "libllvm",   "libclang",     "libunwind",   "libasan",
      "libtsan",     "libubsan",      "libomp",    "libgomp",      "libatomic",   "libm.so",
      "librt.so",    "libresolv.so",  "libnss_",   "libutil.so",   "libcrypt.so", "libanl.so",
      "libgfortran", "clang-tracer",  "libtracer", "ld-linux",     "ld-musl",     "libprotobuf",
      "libgrpc",     "libabsl"};
  for (auto prefix : prefixes)
    if (::tracer::compat::starts_with(name, prefix)) {
      return true;
    }
  return false;
}

bool standard_namespace(std::string_view context) {
  for (std::string_view root : {"std", "__gnu_cxx", "__cxxabiv1"})
    if (context == root || (::tracer::compat::starts_with(context, root) &&
                            ::tracer::compat::starts_with(context.substr(root.size()), "::"))) {
      return true;
    }
  return false;
}

bool standard_symbol(const char *linkage_name) {
  if (!linkage_name)
    return false;
  // CRT startup/termination stubs can live in the executable rather than a
  // runtime DSO; they are not application callers either.
  for (std::string_view name :
       {"_start", "_init", "_fini", "__libc_csu_init", "__libc_csu_fini",
        "__do_global_dtors_aux", "frame_dummy", "register_tm_clones",
        "deregister_tm_clones", "__clang_call_terminate"})
    if (linkage_name == name)
      return true;
  llvm::ItaniumPartialDemangler demangler;
  if (demangler.partialDemangle(linkage_name) || !demangler.isFunction())
    return false;
  std::size_t size = 0;
  std::unique_ptr<char, decltype(&std::free)> context(
      demangler.getFunctionDeclContextName(nullptr, &size), &std::free);
  // Inspect only the declaration context. An application function may take or
  // return std:: types without becoming a standard-library frame.
  return context && standard_namespace(context.get());
}

bool standard_die(llvm::DWARFDie die) {
  const auto declaration = [](llvm::DWARFDie current) {
    if (!current)
      return current;
    const auto specification = current.getAttributeValueAsReferencedDie(
        llvm::dwarf::DW_AT_specification);
    return specification ? specification : current.getAttributeValueAsReferencedDie(
        llvm::dwarf::DW_AT_abstract_origin);
  };
  auto fast = die;
  while (auto origin = declaration(die)) {
    die = origin;
    fast = declaration(declaration(fast));
    if (die == fast)
      break; // Malformed DWARF can contain a reference cycle.
  }
  for (; die; die = die.getParent())
    if (die.getTag() == llvm::dwarf::DW_TAG_namespace)
      if (const char *name = die.getName(llvm::DINameKind::ShortName))
        if (standard_namespace(name))
          return true;
  return false;
}

struct FrameSymbols {
  std::string function, module, file;
  std::uint32_t line = 0;
  std::uint64_t module_offset = 0;
  bool application = false;
};
#endif

// Immutable instructions for one object PC. Compile LLVM's row/rule objects
// once, but keep live register availability, bounds and memory checks per call.
struct UnwindPlan {
  enum Action : std::uint8_t { CFAValue, CFARead, Register };
  struct Rule {
    Action action = CFAValue;
    std::uint8_t destination = 0, source = 0;
    std::int32_t offset = 0;
  };
  unsigned cfa_register = 0;
  std::int32_t cfa_offset = 0;
  RegisterMask preserved = 0;
  bool root = false;
#if defined(__aarch64__)
  bool signed_return = false;
#endif
  unsigned count = 0;
  std::array<Rule, native_registers> rules;
};

struct Image {
  std::unique_ptr<llvm::MemoryBuffer> bytes;
  std::unique_ptr<llvm::object::ObjectFile> object;
  std::unique_ptr<llvm::DWARFContext> dwarf;
  std::vector<Elf64_Phdr> loads;
  std::optional<bool> application;
  bool retained = false;
  std::unordered_map<std::uint64_t, std::vector<llvm::dwarf::UnwindRow>> rows;
  std::unordered_map<std::uint64_t, UnwindPlan> plans;
#if defined(__x86_64__)
  std::unordered_map<std::uint64_t, std::shared_ptr<const FrameSymbols>> symbols;
#endif
};

// Shared by probes; all LLVM contexts and row caches are protected together.
// Cache storage is bounded and remains valid while late wrapper entrants drain.
struct Cache {
  std::mutex mutex;
  std::unordered_map<std::string, std::shared_ptr<Image>> images;
  std::string session_id;
  std::size_t bytes = 0;
  std::size_t row_count = 0;
  std::size_t plan_count = 0;
  std::size_t symbolized_frames = 0;

  std::shared_ptr<Image> image(const Mapping &m) {
    if (m.path.empty() || m.path.front() != '/')
      stop("native ELF mapped path is unavailable");
    auto key = m.path;
    key.push_back('\0');
    key += std::to_string(m.device_major) + ':' + std::to_string(m.device_minor) + ':' +
           std::to_string(m.inode);
    const auto existing = images.find(key);
    if (existing != images.end())
      return existing->second;

    // A trace session treats the mapped binary as immutable. Procfs device
    // and inode values can differ from stat() for OverlayFS mappings, so the
    // mapped identity selects the session cache and ELF load offsets/ranges
    // below validate each PC against the live mapping.
    const int fd = ::open(m.path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
      stop("cannot open native ELF");
    struct Fd {
      int fd;
      ~Fd() { ::close(fd); }
    } owner{fd};
    struct stat opened{};
    if (::fstat(fd, &opened) || !S_ISREG(opened.st_mode) ||
        opened.st_size < static_cast<off_t>(sizeof(Elf64_Ehdr)) ||
        opened.st_size > 256 * 1024 * 1024 || images.size() >= 64 ||
        static_cast<std::uint64_t>(opened.st_size) >
            512 * 1024 * 1024 - bytes)
      stop("native ELF cache bound exceeded");
    auto result = std::make_shared<Image>();
    // Never retain file-backed ELF mmaps in the target: an extra offset-zero
    // mapping would make the admission resolver correctly reject ambiguous
    // load biases. A private byte copy leaves the loaded-image topology alone.
    // LLVM uses a contiguous buffer. Copy the ELF so a file truncated after
    // attach cannot fault the target through a file mapping.
    auto buffer = llvm::WritableMemoryBuffer::getNewUninitMemBuffer(
        opened.st_size, m.path);
    if (!buffer)
      stop("cannot allocate native ELF byte copy");
    std::size_t got = 0;
    while (got < buffer->getBufferSize()) {
      const auto amount = ::pread(fd, buffer->getBufferStart() + got,
                                  buffer->getBufferSize() - got, got);
      if (amount < 0 && errno == EINTR)
        continue;
      if (amount <= 0)
        stop("cannot read native ELF");
      got += static_cast<std::size_t>(amount);
    }
    result->bytes = std::move(buffer);
    const auto contents = result->bytes->getBuffer();
    Elf64_Ehdr header{};
    std::memcpy(&header, contents.data(), sizeof(header));
    if (std::memcmp(header.e_ident, "\177ELF", 4) ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_machine != native_machine ||
        (header.e_type != ET_DYN && header.e_type != ET_EXEC) ||
        header.e_phentsize != sizeof(Elf64_Phdr) ||
        header.e_phoff > contents.size() ||
        header.e_phnum >
            (contents.size() - header.e_phoff) / sizeof(Elf64_Phdr))
      stop("unsupported native ELF layout");
    for (unsigned i = 0; i < header.e_phnum; ++i) {
      Elf64_Phdr load{};
      std::memcpy(&load, contents.data() + header.e_phoff + i * sizeof(load),
                  sizeof(load));
      if (load.p_type == PT_LOAD && (load.p_flags & PF_X)) {
        if (load.p_offset > contents.size() ||
            load.p_filesz > contents.size() - load.p_offset ||
            load.p_filesz >
                std::numeric_limits<std::uint64_t>::max() - load.p_vaddr)
          stop("invalid native ELF executable segment");
        result->loads.push_back(load);
      }
    }
    auto object = llvm::object::ObjectFile::createObjectFile(
        result->bytes->getMemBufferRef());
    if (!object) {
      llvm::consumeError(object.takeError());
      stop("LLVM rejected native ELF");
    }
    result->object = std::move(*object);
    result->dwarf = llvm::DWARFContext::create(*result->object);
    // Retention is optional. Captures keep oversized or uncached images alive
    // in their mapping snapshot and apply the same unwind and lookup rules.
    if (images.size() < 64 && contents.size() <= 512 * 1024 * 1024 - bytes) {
      images.emplace(std::move(key), result);
      result->retained = true;
      bytes += contents.size();
    }
    return result;
  }
};
// Retained like the probe registry: a late entrant or a target atexit callback
// must not observe a destroyed mutex/context while the agent remains loaded.
Cache &cache = *std::make_unique<Cache>().release();

struct Location {
  const Mapping *mapping;
  Image *image;
  std::uint64_t pc;
};
Location locate(const std::vector<Mapping> &maps, std::uint64_t pc) {
  for (const auto &m : maps) {
    if (!m.executable || pc < m.begin || pc >= m.end)
      continue;
    if (!m.verified_image)
      m.verified_image = cache.image(m);
    auto &image = *m.verified_image;
    const auto file_offset = m.offset + (pc - m.begin);
    for (const auto &load : image.loads)
      if (file_offset >= load.p_offset &&
          file_offset - load.p_offset < load.p_filesz)
        return {&m, &image, load.p_vaddr + file_offset - load.p_offset};
    stop("native PC does not match an ELF executable segment");
  }
  stop("native PC is outside mapped executable ELF code");
}

const std::vector<llvm::dwarf::UnwindRow> &
rows_for(Location loc, std::vector<llvm::dwarf::UnwindRow> &scratch) {
  auto frame = loc.image->dwarf->getEHFrame();
  if (!frame) {
    llvm::consumeError(frame.takeError());
    stop("LLVM rejected native .eh_frame");
  }
  if (!*frame)
    stop("native .eh_frame is unavailable");
  for (const auto &entry : (*frame)->entries()) {
    const auto *fde = llvm::dyn_cast<llvm::dwarf::FDE>(&entry);
    if (!fde || loc.pc < fde->getInitialLocation() ||
        loc.pc - fde->getInitialLocation() >= fde->getAddressRange())
      continue;
    auto *cie = fde->getLinkedCIE();
    if (!cie || cie->getReturnAddressRegister() != native_return ||
        ::tracer::compat::contains(cie->getAugmentationString(), 'S')) {
      stop("signal or incomplete native CFI frame");
    }
    auto &cached = loc.image->rows;
    if (const auto found = cached.find(fde->getInitialLocation());
        found != cached.end())
      return found->second;
    auto table = llvm::dwarf::createUnwindTable(fde);
    if (!table) {
      llvm::consumeError(table.takeError());
      stop("unsupported native CFI program");
    }
    scratch.assign(table->begin(), table->end());
    if (loc.image->retained &&
        (cached.size() >= 4096 || scratch.size() > 65536 - cache.row_count))
      return scratch;
    const auto result = cached.emplace(fde->getInitialLocation(), std::move(scratch));
    if (loc.image->retained)
      cache.row_count += result.first->second.size();
    return result.first->second;
  }
  stop("no native CFI covers this caller");
}

std::uint64_t add_offset(std::uint64_t value, std::int32_t offset) {
  if ((offset < 0 && value < static_cast<std::uint64_t>(
                                 -static_cast<std::int64_t>(offset))) ||
      (offset > 0 &&
       value > std::numeric_limits<std::uint64_t>::max() - offset))
    stop("native CFI address overflow");
  return value + static_cast<std::int64_t>(offset);
}

struct Registers {
  tracer::NativeCodeRegisters values{};
  RegisterMask known = 0;
  bool has(unsigned reg) const { return (known & register_bit(reg)) != 0; }
};
#if defined(__x86_64__)
Registers seed(const tracer::TraceContext &ctx) {
  Registers r;
  r.values[6] = ctx.caller_bp;
  r.values[7] = ctx.caller_sp;
  r.values[16] = ctx.caller_pc;
  r.known = (1u << 6) | (1u << 7) | (1u << 16);
  return r;
}
#endif

thread_local std::array<std::uint8_t, 4096> stack_page_bytes;
const long native_page_bytes = ::sysconf(_SC_PAGESIZE);

struct StackMemory {
  std::uint64_t entry_sp;
  const Mapping &stack;
  pid_t process;
  std::uint64_t first = 0;
  std::size_t loaded = 0;

  unsigned copy_cached(iovec *local, iovec *remote, unsigned count) const {
    unsigned missing = 0;
    for (unsigned i = 0; i < count; ++i) {
      const auto address = reinterpret_cast<std::uintptr_t>(remote[i].iov_base);
      if (loaded >= 8 && address >= first && address - first <= loaded - 8) {
        std::memcpy(local[i].iov_base,
                    stack_page_bytes.data() + address - first, 8);
      } else {
        local[missing] = local[i];
        remote[missing++] = remote[i];
      }
    }
    return missing;
  }

  void read(iovec *local, iovec *remote, unsigned count) {
    count = copy_cached(local, remote, count);
    if (!count)
      return;
    auto low = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t high = 0;
    for (unsigned i = 0; i < count; ++i) {
      const auto address = reinterpret_cast<std::uintptr_t>(remote[i].iov_base);
      low = std::min(low, static_cast<std::uint64_t>(address));
      high = std::max(high, static_cast<std::uint64_t>(address));
    }
    if (native_page_bytes > 0 && low >= entry_sp) {
      const auto page_remaining = native_page_bytes - low % native_page_bytes;
      const auto size = std::min<std::uint64_t>(
          {stack_page_bytes.size(), page_remaining, stack.end - low});
      if (size >= 8 && high - low <= size - 8) {
        // Only read a page that already contains a required saved word. Do not
        // fault in unrelated stack pages or cache this thread's live tracer
        // frames below entry_sp. State is reset for every actual capture.
        first = low;
        loaded = 0;
        iovec destination{stack_page_bytes.data(),
                          static_cast<std::size_t>(size)};
        iovec source{reinterpret_cast<void *>(low),
                     static_cast<std::size_t>(size)};
        const auto got =
            ::tracer::linux_api::process_vm_readv(process, &destination, 1, &source, 1, 0);
        if (got > 0)
          loaded = static_cast<std::size_t>(got);
        count = copy_cached(local, remote, count);
      }
    }
    // A failed/partial window read never invalidates an otherwise recoverable
    // stack. Read only the still-needed words through the original safe path.
    if (count && ::tracer::linux_api::process_vm_readv(process, local, count, remote, count, 0) !=
                     static_cast<ssize_t>(count * sizeof(std::uint64_t))) {
      stop("native stack memory is unreadable");
    }
  }
};

const UnwindPlan &plan_for(Location loc, UnwindPlan &fallback) {
  if (const auto found = loc.image->plans.find(loc.pc);
      found != loc.image->plans.end())
    return found->second;
  const llvm::dwarf::UnwindRow *row = nullptr;
  std::vector<llvm::dwarf::UnwindRow> scratch;
  for (const auto &candidate : rows_for(loc, scratch))
    if (candidate.hasAddress() && candidate.getAddress() <= loc.pc &&
        (!row || candidate.getAddress() > row->getAddress()))
      row = &candidate;
  if (!row)
    stop("no native CFI row applies");
  using Rule = llvm::dwarf::UnwindLocation;
  const auto &cfa = row->getCFAValue();
  if (cfa.getLocation() != Rule::RegPlusOffset || cfa.hasAddressSpace() ||
      cfa.getRegister() >= native_registers)
    stop("native CFA requires an unavailable register or expression");
  fallback.cfa_register = cfa.getRegister();
  fallback.cfa_offset = cfa.getOffset();
#if defined(__aarch64__)
  constexpr unsigned ra_sign_state = 34; // DWARF AArch64 RA_SIGN_STATE.
  if (const auto rule = row->getRegisterLocations().getRegisterLocation(ra_sign_state)) {
    if (rule->getLocation() != Rule::Constant)
      stop("native CFI return signing state is unavailable");
    fallback.signed_return = (rule->getConstant() & 1) != 0;
  }
#endif
  for (unsigned i = 0; i < native_registers; ++i) {
    const auto rule = row->getRegisterLocations().getRegisterLocation(i);
    if (!rule || rule->getLocation() == Rule::Unspecified) {
      if (preserved_register(i))
        fallback.preserved |= register_bit(i);
    } else if (rule->getLocation() == Rule::Undefined) {
      fallback.root |= i == native_return;
    } else if (rule->getLocation() == Rule::Same) {
      fallback.preserved |= register_bit(i);
    } else if (rule->getLocation() == Rule::CFAPlusOffset &&
               !rule->hasAddressSpace()) {
      fallback.rules[fallback.count++] = {
          rule->getDereference() ? UnwindPlan::CFARead : UnwindPlan::CFAValue,
          static_cast<std::uint8_t>(i), 0, rule->getOffset()};
    } else if (rule->getLocation() == Rule::RegPlusOffset &&
               !rule->hasAddressSpace() && !rule->getDereference() &&
               rule->getRegister() < native_registers) {
      fallback.rules[fallback.count++] = {
          UnwindPlan::Register, static_cast<std::uint8_t>(i),
          static_cast<std::uint8_t>(rule->getRegister()), rule->getOffset()};
    }
    // Unsupported registers have no instruction or preservation bit.
  }
  // Node references survive rehash; plans belong to path-keyed session images.
  // At the bound, execute this capture's temporary plan without retaining it.
  if (!loc.image->retained || cache.plan_count < 16384) {
    const auto inserted = loc.image->plans.emplace(loc.pc, fallback);
    if (inserted.second && loc.image->retained)
      ++cache.plan_count;
    return inserted.first->second;
  }
  return fallback;
}

Registers unwind(Registers r, Location loc, const Mapping &stack,
                 StackMemory &memory) {
  UnwindPlan fallback;
  const auto &plan = plan_for(loc, fallback);
  if (!r.has(plan.cfa_register))
    stop("native CFA requires an unavailable register or expression");
  const auto sp = add_offset(r.values[plan.cfa_register], plan.cfa_offset);
  if (sp < r.values[native_sp] || (native_pc == native_return && sp == r.values[native_sp]) ||
      sp < stack.begin || sp > stack.end)
    stop("native CFA does not advance within the original stack");
  Registers next = r;
  next.known = r.known & plan.preserved;
  if (plan.root) {
    next.values[native_return] = 0;
    next.known |= register_bit(native_return);
  }
  std::array<iovec, native_registers> local_reads, remote_reads;
  unsigned read_count = 0;
  for (unsigned i = 0; i < plan.count; ++i) {
    const auto &rule = plan.rules[i];
    const auto destination = rule.destination;
    if (rule.action == UnwindPlan::Register) {
      if (r.has(rule.source)) {
        next.values[destination] =
            add_offset(r.values[rule.source], rule.offset);
        next.known |= register_bit(destination);
      }
      continue;
    }
    const auto value = add_offset(sp, rule.offset);
    if (rule.action == UnwindPlan::CFARead) {
      if (value < stack.begin || value > stack.end || stack.end - value < 8)
        stop("native CFI read leaves the original stack mapping");
      local_reads[read_count] = {&next.values[destination], sizeof(value)};
      remote_reads[read_count++] = {reinterpret_cast<void *>(value),
                                    sizeof(value)};
    } else {
      next.values[destination] = value;
    }
    next.known |= register_bit(destination);
  }
  memory.read(local_reads.data(), remote_reads.data(), read_count);
  next.values[native_sp] = sp;
  next.known |= register_bit(native_sp);
  if (!next.has(native_return))
    stop("native CFI cannot recover the caller PC");
  next.values[native_pc] = next.values[native_return];
#if defined(__aarch64__)
  if (plan.signed_return)
    next.values[native_pc] = tracer::strip_return_signature(next.values[native_pc]);
#endif
  next.known |= register_bit(native_pc);
  if (sp == r.values[native_sp] && next.values[native_pc] == r.values[native_pc])
    stop("native CFI did not advance beyond a leaf");
  return next;
}

#if defined(__x86_64__)
std::string readable_text(std::string value) {
  value.resize(std::min<std::size_t>(value.size(), 256));
  for (char &c : value)
    if (static_cast<unsigned char>(c) < 32 || c == 127)
      c = '?';
  return value;
}

bool application_image(Image &image, std::string_view path) {
  if (image.application)
    return *image.application;
  image.application = false;
  if (runtime_module(path))
    return false;
  const auto &elf =
      llvm::cast<llvm::object::ELF64LEObjectFile>(*image.object).getELFFile();
  auto dynamic = elf.dynamicEntries();
  if (!dynamic) {
    llvm::consumeError(dynamic.takeError());
    return false;
  }
  std::uint64_t strings = 0, string_size = 0;
  std::optional<std::uint64_t> soname_offset;
  for (const auto &entry : *dynamic) {
    if (entry.d_tag == DT_NULL)
      break;
    if (entry.d_tag == DT_STRTAB)
      strings = entry.d_un.d_ptr;
    if (entry.d_tag == DT_STRSZ)
      string_size = entry.d_un.d_val;
    if (entry.d_tag == DT_SONAME)
      soname_offset = entry.d_un.d_val;
  }
  if (soname_offset) {
    auto table = elf.toMappedAddr(strings);
    if (!table) {
      llvm::consumeError(table.takeError());
      return false;
    }
    const auto contents = image.bytes->getBuffer();
    const auto *begin = reinterpret_cast<const char *>(*table);
    if (begin < contents.begin() || begin > contents.end() ||
        string_size > static_cast<std::uint64_t>(contents.end() - begin) ||
        *soname_offset >= string_size)
      return false;
    const auto *name = begin + *soname_offset;
    const auto available = string_size - *soname_offset;
    const auto length = ::strnlen(name, available);
    if (length == available || runtime_module(std::string_view(name, length)))
      return false;
  }
  image.application = true;
  return true;
}

// ELF symbol tables need not be ordered by address. Consult them only when
// DWARF lacks a name, and read and demangle only a symbol covering this PC.
template <typename Symbols>
bool find_symbol(Symbols symbols, std::uint64_t pc, FrameSymbols &metadata) {
  for (const auto &symbol : symbols) {
    auto address = symbol.getAddress();
    if (!address) {
      llvm::consumeError(address.takeError());
      continue;
    }
    if (pc < *address || pc - *address >= llvm::object::ELFSymbolRef(symbol).getSize())
      continue;
    auto type = symbol.getType();
    if (!type) {
      llvm::consumeError(type.takeError());
      continue;
    }
    if (*type != llvm::object::SymbolRef::ST_Function)
      continue;
    auto name = symbol.getName();
    if (!name) {
      llvm::consumeError(name.takeError());
      continue;
    }
    if (name->empty())
      continue;
    metadata.function = readable_text(llvm::demangle(*name));
    metadata.application &= !standard_symbol(name->str().c_str());
    return true;
  }
  return false;
}

std::shared_ptr<const FrameSymbols> symbols_for(Location loc) {
  auto &symbols = loc.image->symbols;
  if (const auto found = symbols.find(loc.pc); found != symbols.end())
    return found->second;

  auto metadata = std::make_shared<FrameSymbols>();
  metadata->module = readable_text(loc.mapping->path);
  metadata->module_offset = loc.pc;
  metadata->application = application_image(*loc.image, loc.mapping->path);
  try {
    const llvm::DILineInfoSpecifier spec(
        llvm::DILineInfoSpecifier::FileLineInfoKind::AbsoluteFilePath,
        llvm::DINameKind::LinkageName);
    const auto info = loc.image->dwarf->getLineInfoForAddress(
        {loc.pc, llvm::object::SectionedAddress::UndefSection}, spec);
    if (info) {
      metadata->application &= !standard_symbol(info->FunctionName.c_str());
      if (info->FunctionName != llvm::DILineInfo::BadString)
        metadata->function = readable_text(llvm::demangle(info->FunctionName));
      if (info->FileName != llvm::DILineInfo::BadString)
        metadata->file = readable_text(info->FileName);
      metadata->line = info->Line;
    }
    metadata->application &= !standard_die(
        loc.image->dwarf->getDIEsForAddress(loc.pc).FunctionDIE);
  } catch (...) {
    // Keep module identity even if debug-name lookup fails.
  }
  if (metadata->function.empty() &&
      !find_symbol(loc.image->object->symbols(), loc.pc, *metadata)) {
    const auto &elf =
        llvm::cast<llvm::object::ELF64LEObjectFile>(*loc.image->object);
    find_symbol(elf.getDynamicSymbolIterators(), loc.pc, *metadata);
  }
  if (loc.image->retained && cache.symbolized_frames < 16384) {
    symbols.emplace(loc.pc, metadata);
    ++cache.symbolized_frames;
  }
  return metadata;
}
#endif
} // namespace

#if defined(__x86_64__)
struct tracer::NativeStackStorage {
  std::vector<StackFrame> frames;
  std::unordered_map<std::uint64_t, std::shared_ptr<const FrameSymbols>> symbols;
};

namespace {
void symbolize_stack(tracer::NativeStackStorage &storage,
                     std::vector<Mapping> &maps, bool application_only) noexcept {
  try {
    std::lock_guard lock(cache.mutex);
    if (maps.empty())
      maps = mappings();
    for (auto &frame : storage.frames) {
      if (frame.kind == tracer::FrameKind::JitBridge) {
        frame.function = "clangtracer_wrapper (verified bridge)";
        continue;
      }
      try {
        const auto pc = frame.kind == tracer::FrameKind::TargetEntry
                            ? frame.pc : frame.pc - 1;
        auto found = storage.symbols.find(pc);
        if (found == storage.symbols.end()) {
          const auto loc = locate(maps, pc);
          if (application_only && !application_image(*loc.image, loc.mapping->path))
            continue;
          auto metadata = symbols_for(loc);
          found = storage.symbols.emplace(pc, std::move(metadata)).first;
        }
        const auto &metadata = found->second;
        frame.module_offset = metadata->module_offset;
        frame.function = metadata->function.c_str();
        frame.module = metadata->module.c_str();
        frame.file = metadata->file.c_str();
        frame.application_frame = metadata->application;
      } catch (...) {
        // Unknown names do not invalidate recovered native frames.
      }
    }
  } catch (...) {
    // Raw frames and unwind status remain available if symbolization fails.
  }
}
} // namespace

extern "C" void tracer_stack_enter(tracer::TraceContext *ctx,
                                   void *frame) noexcept {
  const int saved_errno = errno;
  ctx->wrapper_frame = reinterpret_cast<std::uintptr_t>(frame);
  // This address is supplied by __builtin_frame_address(0) in our live wrapper,
  // not by an arbitrary target register. Other unwind reads remain fault-safe.
  std::uint64_t words[2];
  std::memcpy(words, frame, sizeof(words));
  ctx->caller_bp = words[0];
  ctx->caller_pc = words[1];
  ctx->caller_sp = ctx->wrapper_frame + sizeof(words);
  ctx->stack_previous = current_context;
  current_context = ctx;
  errno = saved_errno;
}
extern "C" void tracer_stack_leave(tracer::TraceContext *ctx) noexcept {
  if (current_context == ctx)
    current_context = const_cast<tracer::TraceContext *>(ctx->stack_previous);
  const int saved_errno = errno;
  tracer::release_native_stack(*ctx);
  errno = saved_errno;
}
#endif

namespace tracer {
NativeCodeStack capture_native_code_stack(const NativeCodeRegisters &registers,
    std::span<std::uint64_t> pcs, std::span<std::uint64_t> stack_pointers) noexcept {
  NativeCodeStack result;
  const auto depth = std::min(pcs.size(), stack_pointers.size());
  if (!depth || !registers[native_pc])
    return result;
  pcs.front() = registers[native_pc];
  stack_pointers.front() = registers[native_sp];
  result.count = 1;
  try {
    const auto process = ::getpid();
    const auto maps = mappings(registers[native_sp], process);
    const auto stack = std::ranges::find_if(maps, [&](const auto &mapping) {
      return mapping.readable && registers[native_sp] >= mapping.begin && registers[native_sp] < mapping.end;
    });
    if (stack == maps.end())
      return result;
    const std::lock_guard lock(cache.mutex);
    StackMemory memory{registers[native_sp], *stack, process};
    Registers current{registers, register_bit(native_registers) - 1};
    current = unwind(current, locate(maps, current.values[native_pc]), *stack, memory);
    while (current.values[native_pc]) {
      if (result.count == depth) {
        result.status = StackStatus::Truncated;
        return result;
      }
      if (current.values[native_pc] < call_size)
        break;
      const auto location = locate(maps, current.values[native_pc] - call_size);
      pcs[result.count] = current.values[native_pc];
      stack_pointers[result.count++] = current.values[native_sp];
      current = unwind(current, location, *stack, memory);
    }
    if (!current.values[native_pc])
      result.status = StackStatus::Complete;
  } catch (...) {
    // Keep all original frames verified before the missing or invalid row.
  }
  return result;
}

#if defined(__x86_64__)
void begin_native_stack_session(std::string_view session_id) {
  std::lock_guard lock(cache.mutex);
  if (cache.session_id == session_id)
    return;
  cache.images.clear();
  cache.bytes = 0;
  cache.row_count = 0;
  cache.plan_count = 0;
  cache.symbolized_frames = 0;
  cache.session_id.assign(session_id);
}

void release_native_stack(const TraceContext &ctx) noexcept {
  delete ctx.stack_storage;
  ctx.stack_storage = nullptr;
}
void capture_native_stack(const TraceContext &ctx, unsigned depth,
                          bool application_only) noexcept {
  const int saved_errno = errno;
  auto &out = ctx.stack;
  out = {};
  if (ctx.stack_storage)
    ctx.stack_storage->frames.clear();
  out.requested_depth = depth;
  out.application_only = application_only;
  out.status = StackStatus::Partial;
  std::vector<Mapping> maps;
  try {
    if (!depth)
      stop("stack depth must be positive");
    if (!ctx.stack_storage)
      ctx.stack_storage = std::make_unique<NativeStackStorage>().release();
    auto &frames = ctx.stack_storage->frames;
    frames.push_back({ctx.target_entry,
                      ctx.caller_sp >= 8 ? ctx.caller_sp - 8 : 0,
                      FrameKind::TargetEntry});
    if (!ctx.caller_pc || !ctx.caller_sp)
      stop("Clang wrapper caller state is unavailable");
    // Procfs I/O and parsing use private scratch; do not serialize captures
    // behind that work. LLVM/image/cache access remains protected below.
    const auto process = ::getpid();
    maps = mappings(ctx.caller_sp, process);
    std::lock_guard lock(cache.mutex);
    const Mapping *stack = nullptr;
    for (const auto &m : maps)
      if (m.readable && m.begin <= ctx.caller_sp && ctx.caller_sp < m.end) {
        stack = &m;
        break;
      }
    if (!stack)
      stop("entry stack is outside readable mappings");
    StackMemory memory{ctx.caller_sp, *stack, process};
    auto regs = seed(ctx);
    auto *outer = ctx.stack_previous;
    while (regs.values[16]) {
      if (frames.size() == depth) {
        out.status = StackStatus::Truncated;
        std::snprintf(out.detail, sizeof(out.detail),
                      "entry stack exceeds requested depth");
        break;
      }
      const auto pc = regs.values[16];
      // A live outer invocation supplies exact labels around its trampoline
      // call and the wrapper frame. We never guess CFI for anonymous JIT code.
      if (outer && regs.has(6) && regs.values[6] == outer->wrapper_frame &&
          pc >= outer->bridge_begin && pc <= outer->bridge_end &&
          outer->bridge_end > outer->bridge_begin) {
        frames.push_back({pc, regs.values[7], FrameKind::JitBridge});
        if (outer->caller_sp <= regs.values[7] || outer->caller_sp > stack->end)
          stop("JIT bridge does not advance within the original stack");
        regs = seed(*outer);
        outer = outer->stack_previous;
        continue;
      }
      frames.push_back({pc, regs.values[7], FrameKind::Native});
      regs = unwind(regs, locate(maps, pc - 1), *stack, memory);
    }
    if (!regs.values[16]) {
      out.status = StackStatus::Complete;
      std::snprintf(out.detail, sizeof(out.detail),
                    "native CFI reached the thread root");
    }
  } catch (const std::exception &error) {
    std::snprintf(out.detail, sizeof(out.detail), "%s", error.what());
  } catch (...) {
    std::snprintf(out.detail, sizeof(out.detail),
                  "native stack capture failed");
  }
  // Resolve before the C++ hook runs, for complete, partial and truncated
  // unwinds alike. This runs after the unwind lock has been released.
  if (ctx.stack_storage) {
    auto &storage = *ctx.stack_storage;
    symbolize_stack(storage, maps, application_only);
    auto &frames = storage.frames;
    out.captured_depth = static_cast<unsigned>(frames.size());
    if (application_only) {
      frames.erase(std::remove_if(frames.begin(), frames.end(),
                                  [](const auto &f) { return !f.application_frame; }),
                   frames.end());
      out.hidden_frames = out.captured_depth - frames.size();
    }
    out.count = static_cast<unsigned>(frames.size());
    out.frames = frames.data();
  }
  errno = saved_errno;
}

void serialize_native_stack(const StackTrace &stack,
                            clangtracer::v1::StackTrace &output) {
  output.set_status(
      static_cast<clangtracer::v1::StackTrace::Status>(stack.status));
  output.set_requested_depth(stack.requested_depth);
  output.set_application_only(stack.application_only);
  output.set_hidden_frames(stack.hidden_frames);
  output.set_captured_depth(stack.captured_depth);
  output.set_detail(stack.detail,
                    ::strnlen(stack.detail, sizeof(stack.detail)));
  for (unsigned i = 0; i < stack.count; ++i) {
    const auto &frame = stack.frames[i];
    auto *out = output.add_frames();
    out->set_pc(frame.pc);
    out->set_stack_pointer(frame.stack_pointer);
    out->set_kind(static_cast<clangtracer::v1::StackFrame::Kind>(frame.kind));
    // Serialize the exact metadata the callback observed; no later live lookup
    // may change the snapshot after the hook or after a library unload.
    out->set_function(frame.function, ::strnlen(frame.function, 256));
    out->set_module(frame.module, ::strnlen(frame.module, 256));
    out->set_file(frame.file, ::strnlen(frame.file, 256));
    out->set_line(frame.line);
    out->set_module_offset(frame.module_offset);
    out->set_application_frame(frame.application_frame);
  }
}
#endif
} // namespace tracer
