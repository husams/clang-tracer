#include "event_transport.hpp"

#include <cerrno>
#include <stdexcept>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

namespace tracer {
namespace {
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert((EventTransport::kCapacity & (EventTransport::kCapacity - 1)) ==
              0);

std::uint64_t monotonic_ns() noexcept {
  timespec now{};
  ::clock_gettime(CLOCK_MONOTONIC, &now);
  return std::uint64_t(now.tv_sec) * 1000000000ULL + now.tv_nsec;
}
} // namespace

EventTransport::EventTransport(void (*suppress_callbacks)())
    : slots_(std::make_unique<Packet[]>(kCapacity)),
      suppress_callbacks_(suppress_callbacks) {
  for (std::size_t i = 0; i < kCapacity; ++i)
    slots_[i].turn.store(i, std::memory_order_relaxed);
  wake_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (wake_fd_ < 0)
    throw std::runtime_error("cannot create event sender wakeup");
  try {
    worker_ = std::thread([this] { run(); });
  } catch (...) {
    ::close(wake_fd_);
    throw;
  }
}

EventTransport::~EventTransport() {
  stopping_.store(true, std::memory_order_release);
  notified_.store(false, std::memory_order_release);
  notify();
  worker_.join();
  ::close(wake_fd_);
}

EventTransport::Packet *
EventTransport::try_acquire(EventDelivery &delivery) noexcept {
  auto position = enqueue_.load(std::memory_order_relaxed);
  // Bound contention retries as well as capacity; no producer spins waiting
  // for a paused producer, the sender or a full queue to make progress.
  for (unsigned attempt = 0; attempt < 8; ++attempt) {
    auto &packet = slots_[position & (kCapacity - 1)];
    const auto turn = packet.turn.load(std::memory_order_acquire);
    const auto difference = static_cast<std::int64_t>(turn - position);
    if (difference < 0)
      break;
    if (difference == 0 &&
        enqueue_.compare_exchange_weak(position, position + 1,
                                       std::memory_order_relaxed)) {
      packet.position = position;
      packet.delivery = &delivery;
      delivery.pending.fetch_add(1, std::memory_order_relaxed);
      return &packet;
    }
    position = enqueue_.load(std::memory_order_relaxed);
  }
  delivery.dropped.fetch_add(1, std::memory_order_relaxed);
  return nullptr;
}

void EventTransport::notify() noexcept {
  if (!notified_.exchange(true, std::memory_order_acq_rel)) {
    const std::uint64_t value = 1;
    // Nonblocking eventfd: a saturated counter already has a pending wakeup.
    // The worker's bounded poll also recovers an interrupted notification.
    (void)::write(wake_fd_, &value, sizeof(value));
  }
}

void EventTransport::publish(Packet &packet, bool valid) noexcept {
  packet.valid = valid;
  packet.turn.store(packet.position + 1, std::memory_order_release);
  // Coalesce wakeups under a hot producer instead of preempting the traced
  // thread for every datagram. Sparse traffic is picked up within 1 ms.
  if ((packet.position & 31) == 0)
    notify();
}

void EventTransport::activate() noexcept {
  active_.fetch_add(1, std::memory_order_release);
  notified_.store(false, std::memory_order_release);
  notify();
}

void EventTransport::deactivate() noexcept {
  active_.fetch_sub(1, std::memory_order_release);
  notified_.store(false, std::memory_order_release);
  notify();
}

bool EventTransport::ready() const noexcept {
  return slots_[dequeue_ & (kCapacity - 1)].turn.load(
             std::memory_order_acquire) == dequeue_ + 1;
}

void EventTransport::begin_drain(EventDelivery &delivery) noexcept {
  std::uint64_t unset = 0;
  delivery.drain_deadline_ns.compare_exchange_strong(
      unset, monotonic_ns() + 1000000000ULL, std::memory_order_release);
}

bool EventTransport::send(EventDelivery &delivery, const void *bytes,
                          std::size_t size) noexcept {
  while (!stopping_.load(std::memory_order_acquire)) {
    const auto count =
        ::send(delivery.socket_fd, bytes, size, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (count == static_cast<ssize_t>(size))
      return true;
    if (count >= 0 ||
        (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
      return false;
    const auto deadline =
        delivery.drain_deadline_ns.load(std::memory_order_acquire);
    if (deadline && monotonic_ns() >= deadline)
      return false;
    pollfd writable{delivery.socket_fd, POLLOUT, 0};
    (void)::poll(&writable, 1, 10);
  }
  return false;
}

void EventTransport::run() noexcept {
  if (suppress_callbacks_)
    suppress_callbacks_();
  for (;;) {
    while (ready()) {
      auto &packet = slots_[dequeue_ & (kCapacity - 1)];
      auto &delivery = *packet.delivery;
      if (packet.valid) {
        const bool delivered =
            !packet.bytes.empty() && packet.bytes.size() <= kMaxEventBytes &&
            send(delivery, packet.bytes.data(), packet.bytes.size());
        if (delivered)
          delivery.sent.fetch_add(1, std::memory_order_relaxed);
        else
          delivery.dropped.fetch_add(1, std::memory_order_relaxed);
      }
      delivery.pending.fetch_sub(1, std::memory_order_release);
      packet.turn.store(dequeue_ + kCapacity, std::memory_order_release);
      ++dequeue_;
    }
    if (stopping_.load(std::memory_order_acquire))
      return;
    notified_.store(false, std::memory_order_release);
    if (ready())
      continue;
    pollfd wake{wake_fd_, POLLIN, 0};
    (void)::poll(&wake, 1, active_.load(std::memory_order_acquire) ? 1 : -1);
    std::uint64_t value;
    (void)::read(wake_fd_, &value, sizeof(value));
  }
}
} // namespace tracer
