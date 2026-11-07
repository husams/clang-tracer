#include "local_perf.hpp"

#include <stdexcept>

#if defined(__linux__) && defined(__x86_64__)
#include "../proc_identity.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <memory>
#include <optional>
#include <regex>
#include <system_error>
#include <vector>

#include <linux/perf_event.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace tracer_repl {
namespace {
using Clock = std::chrono::steady_clock;

// The GCC 8.5 SDK has the original perf_event_attr. The version 3 UAPI
// prefix adds the clockid needed to correlate samples with probe events.
struct PerfAttributes {
  std::uint32_t type, size;
  std::uint64_t config, period, sample_type, read_format, flags;
  std::uint32_t wakeup_events, bp_type;
  std::uint64_t config1, config2, branch_sample_type, sample_regs_user;
  std::uint32_t sample_stack_user;
  std::int32_t clockid;
};
static_assert(sizeof(PerfAttributes) == 96 && offsetof(PerfAttributes, clockid) == 92);
constexpr std::uint64_t disabled = 1ULL << 0;
constexpr std::uint64_t exclude_kernel = 1ULL << 5;
constexpr std::uint64_t exclude_hv = 1ULL << 6;
constexpr std::uint64_t use_clockid = 1ULL << 25;
constexpr unsigned long fd_cloexec = 1UL << 3;
constexpr std::uint64_t format_lost = 1ULL << 4;
constexpr unsigned record_lost_samples = 13;
constexpr std::size_t max_threads = 64;
constexpr std::size_t ring_pages = 8;

struct RingMetadata {
  std::uint64_t head, tail, offset, size;
};
constexpr auto metadata_offset = offsetof(perf_event_mmap_page, data_head);
static_assert(metadata_offset == 1024);

enum class Kind { enter, exit, cpu };
struct Event {
  Kind kind;
  std::uint32_t tid = 0;
  std::uint64_t time = 0, ip = 0;
  std::int64_t number = 0, result = 0;
};
bool earlier(const Event &left, const Event &right) {
  return left.time < right.time;
}
struct EventSource {
  Kind kind;
  std::uint64_t config;
  std::size_t number_offset = 0, result_offset = 0;
};

[[noreturn]] void perf_error(const std::string &operation, int code = errno) {
  throw std::system_error(code, std::generic_category(), operation);
}

const char *name(Kind kind) {
  switch (kind) {
  case Kind::enter: return "syscall-enter";
  case Kind::exit: return "syscall-exit";
  case Kind::cpu: return "cpu-sample";
  }
  throw std::logic_error("unknown local perf event");
}

std::vector<EventSource> sources(LocalPerfMode mode) {
  if (mode == LocalPerfMode::cpu)
    return {{Kind::cpu, PERF_COUNT_SW_TASK_CLOCK}};
  bool permission = false;
  for (const auto *root : {"/sys/kernel/tracing", "/sys/kernel/debug/tracing"}) {
    std::vector<EventSource> events;
    for (const auto kind : {Kind::enter, Kind::exit}) {
      const auto path = std::filesystem::path(root) / "events/raw_syscalls" /
                        (kind == Kind::enter ? "sys_enter" : "sys_exit");
      std::ifstream id(path / "id");
      if (!id) {
        permission |= errno == EACCES || errno == EPERM;
        break;
      }
      std::ifstream format(path / "format");
      if (!format) {
        permission |= errno == EACCES || errno == EPERM;
        break;
      }
      EventSource source{kind, 0};
      if (!(id >> source.config) || !source.config)
        throw std::runtime_error("invalid syscall tracepoint ID in tracefs");

      const std::regex field(R"(\b(id|ret);\s*offset:(\d+);\s*size:8;\s*signed:1;)");
      std::optional<std::size_t> number, result;
      std::string line;
      while (std::getline(format, line)) {
        std::smatch match;
        if (std::regex_search(line, match, field)) {
          const auto offset = std::stoul(match[2]);
          if (offset > 256)
            throw std::runtime_error("unsupported syscall tracepoint offset");
          (match[1] == "id" ? number : result) = offset;
        }
      }
      if (!number || (kind == Kind::exit && !result))
        throw std::runtime_error("unsupported syscall tracepoint format in tracefs");
      source.number_offset = *number;
      source.result_offset = result.value_or(0);
      events.push_back(source);
    }
    if (events.size() == 2)
      return events;
  }
  if (permission)
    throw std::runtime_error("permission denied reading syscall tracepoints in tracefs");
  throw std::runtime_error("raw syscall tracepoints are unavailable in tracefs");
}

struct Task {
  std::filesystem::path stat;
  std::uint32_t tid;
  std::uint64_t start;
};

std::vector<Task> snapshot_tasks(const std::filesystem::path &process) {
  std::vector<Task> tasks;
  for (const auto &entry : std::filesystem::directory_iterator(process / "task")) {
    if (tasks.size() == max_threads)
      throw std::runtime_error("local perf capture supports at most 64 existing threads");
    const auto stat = entry.path() / "stat";
    tasks.push_back({stat,
                     static_cast<std::uint32_t>(std::stoul(entry.path().filename().string())),
                     tracer::proc::start_time(stat)});
  }
  if (tasks.empty())
    throw std::runtime_error("target has no available threads");
  return tasks;
}

template <class T> T read_at(const unsigned char *bytes, std::size_t size,
                             std::size_t offset) {
  if (offset > size || size - offset < sizeof(T))
    throw std::runtime_error("truncated perf event record");
  T value;
  std::memcpy(&value, bytes + offset, sizeof(value));
  return value;
}

class Ring {
  int fd_ = -1;
  void *mapping_ = MAP_FAILED;
  std::size_t mapping_size_ = 0;
  RingMetadata *meta_ = nullptr;
  const unsigned char *data_ = nullptr;
  std::size_t size_ = 0;
  EventSource source_;
  std::uint32_t tid_;
  bool loss_counter_ = false;
  std::uint64_t lost_ = 0, throttles_ = 0;

  void copy(std::uint64_t offset, void *to, std::size_t length) const {
    const auto start = static_cast<std::size_t>(offset % size_);
    const auto first = std::min(length, size_ - start);
    std::memcpy(to, data_ + start, first);
    if (first != length)
      std::memcpy(static_cast<unsigned char *>(to) + first, data_, length - first);
  }

  Event decode(const unsigned char *payload, std::size_t length, std::uint32_t pid) const {
    Event event{source_.kind};
    const bool cpu = source_.kind == Kind::cpu;
    const std::size_t base = cpu ? sizeof(std::uint64_t) : 0;
    if (cpu)
      event.ip = read_at<std::uint64_t>(payload, length, 0);
    if (read_at<std::uint32_t>(payload, length, base) != pid ||
        (event.tid = read_at<std::uint32_t>(payload, length, base + 4)) != tid_)
      throw std::runtime_error("perf sample identity differs from the selected target");
    event.time = read_at<std::uint64_t>(payload, length, base + 8);
    if (!cpu) {
      const auto raw_size = read_at<std::uint32_t>(payload, length, 16);
      constexpr std::size_t raw_offset = 20;
      if (length - raw_offset < raw_size)
        throw std::runtime_error("truncated syscall tracepoint payload");
      event.number = read_at<std::int64_t>(payload + raw_offset, raw_size,
                                           source_.number_offset);
      if (source_.kind == Kind::exit)
        event.result = read_at<std::int64_t>(payload + raw_offset, raw_size,
                                             source_.result_offset);
    }
    return event;
  }

public:
  Ring(EventSource source, std::uint32_t tid) : source_(source), tid_(tid) {}
  Ring(const Ring &) = delete;
  Ring &operator=(const Ring &) = delete;
  ~Ring() {
    if (mapping_ != MAP_FAILED)
      ::munmap(mapping_, mapping_size_);
    if (fd_ >= 0)
      ::close(fd_);
  }

  void open(std::size_t page_size, bool &try_loss_counter) {
    const bool cpu = source_.kind == Kind::cpu;
    PerfAttributes attr{};
    attr.type = cpu ? PERF_TYPE_SOFTWARE : PERF_TYPE_TRACEPOINT;
    attr.size = sizeof(attr);
    attr.config = source_.config;
    attr.period = cpu ? 10'000'000 : 1;
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_TIME |
                       (cpu ? PERF_SAMPLE_IP : PERF_SAMPLE_RAW);
    attr.flags = disabled | use_clockid | (cpu ? exclude_kernel | exclude_hv : 0);
    attr.clockid = CLOCK_MONOTONIC;
    attr.wakeup_events = 1;
    const auto open_event = [&] {
      return static_cast<int>(::syscall(SYS_perf_event_open, &attr, tid_, -1, -1,
                                        fd_cloexec));
    };
    attr.read_format = try_loss_counter ? format_lost : 0;
    fd_ = open_event();
    if (fd_ < 0 && errno == EINVAL && try_loss_counter) {
      attr.read_format = 0;
      fd_ = open_event();
      if (fd_ >= 0)
        try_loss_counter = false;
    }
    if (fd_ < 0) {
      const int error = errno;
      auto operation = std::string("cannot open local perf ") + name(source_.kind) +
                       " for TID " + std::to_string(tid_);
      if (error == EACCES || error == EPERM)
        operation += " (check perf_event_paranoid and perf permissions)";
      perf_error(operation, error);
    }
    loss_counter_ = attr.read_format == format_lost;
    mapping_size_ = (1 + ring_pages) * page_size;
    mapping_ = ::mmap(nullptr, mapping_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED)
      perf_error("cannot map local perf ring");
    auto *base = static_cast<unsigned char *>(mapping_);
    meta_ = reinterpret_cast<RingMetadata *>(base + metadata_offset);
    if (meta_->offset != page_size || meta_->size != ring_pages * page_size)
      throw std::runtime_error("unsupported perf ring layout");
    data_ = base + meta_->offset;
    size_ = meta_->size;
  }

  int fd() const { return fd_; }

  void enable() const {
    if (::ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0) < 0)
      perf_error("cannot enable local perf event");
  }

  void disable() const {
    int status;
    do {
      status = ::ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
    } while (status < 0 && errno == EINTR);
    if (status < 0 && errno != ESRCH && errno != ENOENT)
      perf_error("cannot disable local perf event");
  }

  void drain(std::uint32_t pid, std::vector<Event> &events,
             unsigned limit, std::uint64_t &discarded) {
    auto tail = __atomic_load_n(&meta_->tail, __ATOMIC_RELAXED);
    const auto head = __atomic_load_n(&meta_->head, __ATOMIC_ACQUIRE);
    if (head - tail > size_)
      throw std::runtime_error("local perf ring exceeded its readable capacity");
    while (tail != head) {
      perf_event_header header{};
      if (head - tail < sizeof(header))
        throw std::runtime_error("truncated local perf ring header");
      copy(tail, &header, sizeof(header));
      if (header.size < sizeof(header) || header.size > head - tail)
        throw std::runtime_error("invalid local perf record size");
      const auto payload_size = header.size - sizeof(header);
      std::array<unsigned char, 512> payload;
      switch (header.type) {
      case PERF_RECORD_SAMPLE:
      case PERF_RECORD_LOST:
      case record_lost_samples:
        if (payload_size > payload.size())
          throw std::runtime_error("local perf record exceeds the pilot record size");
        copy(tail + sizeof(header), payload.data(), payload_size);
        if (header.type == PERF_RECORD_SAMPLE) {
          const auto event = decode(payload.data(), payload_size, pid);
          if (events.size() < limit) {
            events.push_back(event);
            std::push_heap(events.begin(), events.end(), earlier);
          } else {
            ++discarded;
            // Different event sources have separate rings. Keep the earliest
            // samples across them when one busy ring fills the output budget.
            if (earlier(event, events.front())) {
              std::pop_heap(events.begin(), events.end(), earlier);
              events.back() = event;
              std::push_heap(events.begin(), events.end(), earlier);
            }
          }
        } else {
          lost_ += read_at<std::uint64_t>(payload.data(), payload_size,
                                          header.type == PERF_RECORD_LOST ? 8 : 0);
        }
        break;
      case PERF_RECORD_THROTTLE:
        ++throttles_;
        break;
      }
      tail += header.size;
    }
    __atomic_store_n(&meta_->tail, tail, __ATOMIC_RELEASE);
  }

  std::uint64_t lost() const {
    if (!loss_counter_)
      return lost_;
    std::array<std::uint64_t, 2> values{};
    ssize_t size;
    do {
      size = ::read(fd_, values.data(), sizeof(values));
    } while (size < 0 && errno == EINTR);
    if (size != static_cast<ssize_t>(sizeof(values)))
      throw std::runtime_error("cannot read local perf loss counter");
    return std::max(lost_, values[1]);
  }

  std::uint64_t throttles() const { return throttles_; }
};
} // namespace

void capture_local_perf(LocalPerfMode mode, std::uint32_t pid,
                        std::uint64_t start_time, unsigned seconds, unsigned limit,
                        const std::atomic<bool> &interrupted, std::ostream &out) {
  if (!seconds || seconds > max_local_perf_seconds ||
      !limit || limit > max_local_perf_events)
    throw std::invalid_argument("local perf capture requires 1 to 60 seconds and 1 to 8192 events");
  const auto process = tracer::proc::directory(pid);
  const auto verify_process = [&] {
    if (!start_time || tracer::proc::start_time(process / "stat") != start_time)
      throw std::runtime_error("target identity changed before local perf capture");
  };
  verify_process();
  const auto event_sources = sources(mode);
  const auto tasks = snapshot_tasks(process);
  const auto page = ::sysconf(_SC_PAGESIZE);
  if (page <= 0)
    throw std::runtime_error("cannot determine page size for local perf capture");

  bool loss_counter = true;
  std::vector<std::unique_ptr<Ring>> rings;
  std::vector<pollfd> fds;
  for (const auto &task : tasks) {
    for (const auto &source : event_sources) {
      auto ring = std::make_unique<Ring>(source, task.tid);
      ring->open(page, loss_counter);
      fds.push_back({ring->fd(), POLLIN, 0});
      rings.push_back(std::move(ring));
    }
  }
  for (const auto &task : tasks)
    if (tracer::proc::start_time(task.stat) != task.start)
      throw std::runtime_error("thread identity changed before local perf capture");
  verify_process();
  if (interrupted.load())
    throw std::runtime_error("local perf capture interrupted before enabling events");

  const auto label = mode == LocalPerfMode::cpu ? "cpu" : "syscalls";
  std::vector<Event> events;
  events.reserve(limit);
  std::uint64_t discarded = 0;
  for (const auto &ring : rings)
    ring->enable();
  const auto deadline = Clock::now() + std::chrono::seconds(seconds);
  out << "local capture ready kind=" << label << " pid=" << pid
      << " threads=" << tasks.size() << '\n' << std::flush;
  const char *stop = "timeout";
  for (;;) {
    for (const auto &ring : rings)
      ring->drain(pid, events, limit, discarded);
    if (interrupted.load()) {
      stop = "interrupted";
      break;
    }
    if (events.size() == limit) {
      stop = "limit";
      break;
    }
    if (std::none_of(fds.begin(), fds.end(), [](const auto &fd) { return fd.fd >= 0; })) {
      stop = "target-exit";
      break;
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (remaining <= 0)
      break;
    if (::poll(fds.data(), fds.size(), std::min<std::int64_t>(remaining, 50)) < 0) {
      if (errno == EINTR)
        continue;
      perf_error("cannot poll local perf events");
    }
    for (auto &fd : fds) {
      if (fd.revents & POLLHUP)
        fd.fd = -1;
      else if (fd.revents & (POLLERR | POLLNVAL))
        throw std::runtime_error("local perf event reported a polling error");
    }
  }

  for (const auto &ring : rings)
    ring->disable();
  std::uint64_t lost = 0, throttles = 0;
  for (const auto &ring : rings) {
    ring->drain(pid, events, limit, discarded);
    lost += ring->lost();
    throttles += ring->throttles();
  }
  rings.clear();
  std::sort_heap(events.begin(), events.end(), earlier);
  for (const auto &event : events) {
    out << "kind=" << name(event.kind) << " pid=" << pid << " tid=" << event.tid
        << " time_ns=" << event.time;
    if (event.kind == Kind::cpu)
      out << " ip=0x" << std::hex << event.ip << std::dec;
    else
      out << " nr=" << event.number;
    if (event.kind == Kind::exit)
      out << " result=" << event.result;
    out << '\n';
  }
  out << "local capture complete kind=" << label << " events=" << events.size()
      << " lost=" << lost << " loss_accounting=" << (loss_counter ? "counter" : "records")
      << " throttles=" << throttles << " discarded=" << discarded
      << " stopped=" << stop << '\n';
}
} // namespace tracer_repl

#else
namespace tracer_repl {
void capture_local_perf(LocalPerfMode, std::uint32_t, std::uint64_t, unsigned,
                        unsigned, const std::atomic<bool> &, std::ostream &) {
  throw std::runtime_error("local perf capture requires Linux x86-64");
}
} // namespace tracer_repl
#endif
