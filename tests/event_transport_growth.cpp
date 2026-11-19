#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include "event_transport.hpp"

int main() {
  tracer::EventDelivery delivery;
  {
    tracer::EventTransport transport;
    std::vector<tracer::EventTransport::Packet*> packets;
    for (unsigned i = 0; i < 5000; ++i) {
      auto* packet = transport.try_acquire(delivery);
      assert(packet);
      packets.push_back(packet);
    }
    assert(delivery.pending.load() == packets.size());
    assert(delivery.dropped.load() == 0);
    for (auto* packet : packets) {
      transport.publish(*packet, false);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (delivery.pending.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    assert(delivery.pending.load() == 0);
  }
  assert(delivery.pending.load() == 0);
}
