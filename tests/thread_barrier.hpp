#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

// Only the rendezvous operation used by the concurrent-attach regression;
// does not require a C++20 standard-library implementation on the build host.
class ThreadBarrier final {
public:
  explicit ThreadBarrier(std::size_t participants)
      : participants_(participants)
      , remaining_(participants) {}

  void arrive_and_wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto generation = generation_;
    if (--remaining_ == 0) {
      remaining_ = participants_;
      ++generation_;
      lock.unlock();
      changed_.notify_all();
    } else {
      changed_.wait(lock, [&] { return generation_ != generation; });
    }
  }

private:
  const std::size_t participants_;
  std::size_t remaining_, generation_ = 0;
  std::mutex mutex_;
  std::condition_variable changed_;
};
