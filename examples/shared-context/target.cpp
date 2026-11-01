// Ordinary application code: this program has no dependency on the tracer.
#include <csignal>
#include <cstdio>
#include <thread>

#include <unistd.h>

namespace {
volatile std::sig_atomic_t running = 1;

void stop(int) {
  running = 0;
}
} // namespace

// Probe 2 runs here, on the worker thread. Keep a concrete, patchable function
// body by building with debug information and without optimization.
int calculate_total(int request_id) {
  volatile int saved = request_id;
  volatile int total = saved * 2;
  return total;
}

// Probe 1 spans the whole request, including the worker's lifetime.
int process_request(int request_id) {
  int total = 0;
  std::thread worker([&] { total = calculate_total(request_id); });
  worker.join(); // The worker finishes before the outer POST hook runs.
  return total + 1;
}

int main() {
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);
  std::printf("Target PID: %ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);

  int request_id = 1;
  while (running) {
    const int result = process_request(request_id);
    std::printf("request=%d result=%d\n", request_id, result);
    std::fflush(stdout);
    request_id = request_id == 3 ? 1 : request_id + 1;

    // Run slowly enough to read the output and attach probes manually.
    unsigned remaining = 1;
    while (running && remaining) {
      remaining = ::sleep(remaining);
    }
  }
}
