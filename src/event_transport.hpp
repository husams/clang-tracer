#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace tracer {

// Probe-owned, retained until process exit like its wrapper and metadata.
struct EventDelivery {
  int socket_fd = -1;
  std::atomic<std::uint64_t> pending{0}, dropped{0}, sent{0};
  std::atomic<std::uint64_t> drain_deadline_ns{0};
};

// A bounded multi-producer/single-consumer queue. Callers never wait for space
// or socket readiness; only the dedicated sender retries a congested socket.
class EventTransport {
public:
  static constexpr std::size_t kCapacity = 4096;
  static constexpr std::size_t kInlineEventBytes = 64 * 1024;
  static constexpr char kExternalEventMarker = '\0';
  // Linux UAPI values, including when building with an older SDK.
  static constexpr int kAddSeals = 1033, kGetSeals = 1034;
  static constexpr int kReadOnlySeals = 0x02 | 0x04 | 0x08;
  struct Packet {
    std::atomic<std::size_t> turn{0};
    std::size_t position = 0;
    EventDelivery *delivery = nullptr;
    bool valid = false;
    std::string bytes;
  };

  explicit EventTransport(void (*suppress_callbacks)() = nullptr);
  ~EventTransport();
  EventTransport(const EventTransport &) = delete;
  EventTransport &operator=(const EventTransport &) = delete;

  Packet *try_acquire(EventDelivery &delivery) noexcept;
  void publish(Packet &packet, bool valid) noexcept;
  void activate() noexcept;
  void deactivate() noexcept;
  static void begin_drain(EventDelivery &delivery) noexcept;

private:
  std::unique_ptr<Packet[]> slots_;
  alignas(64) std::atomic<std::size_t> enqueue_{0};
  alignas(64) std::size_t dequeue_ = 0;
  std::atomic<bool> stopping_{false}, notified_{false};
  std::atomic<unsigned> active_{0};
  int wake_fd_ = -1;
  void (*suppress_callbacks_)();
  std::thread worker_;

  void notify() noexcept;
  bool ready() const noexcept;
  void run() noexcept;
  bool send(EventDelivery &, const void *, std::size_t) noexcept;
};
} // namespace tracer
