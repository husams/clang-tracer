#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "tracer/protocol.hpp"

namespace tracer::injector {

// Owns all ptrace stops while a trap probe is armed. The agent owns the
// executable wrapper and retains it across removal for late entrants.
class EntryTrap final {
  struct Worker;
  std::unique_ptr<Worker> worker_;

public:
  EntryTrap(std::uint32_t pid, std::uint64_t process_start, const Request &patch,
            std::uintptr_t remote_request, std::chrono::seconds timeout);
  ~EntryTrap();
  // An empty error means text is restored (or its image no longer exists).
  std::string stop();
  std::string diagnostic() const;
  bool armed() const;
  bool image_replaced() const;
};
} // namespace tracer::injector
