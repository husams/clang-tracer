#pragma once

#include "live_transaction.hpp"
#include "tracer.pb.h"

#include <functional>
#include <stdexcept>

namespace tracer::live {

struct Plan {
  JitRequest jit;
  std::vector<Entry> entries;
  std::vector<clangtracer::v1::LiveUpdateStatus::Candidate> candidates;
  std::vector<SourceFile> accepted_sources;
  SafePoint safe_point = SafePoint::independent_entry;
  std::function<void()> validate_sources;
};

class Stale : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

Plan plan_sources(std::uint32_t pid, const std::string &executable,
                  const clangtracer::v1::LiveUpdateRequest &request,
                  const Revision &active, const std::function<bool()> &cancelled);

// Extensions supply generated JIT entry symbols in the same order as entries.
struct BuildInputs {
  std::string module;
  std::string source_root;
  JitRequest baseline;
  std::function<void()> revalidate;
};

BuildInputs open_native_build(const std::string &executable,
                             const clangtracer::v1::LiveUpdateRequest &request);

Entry bind_native_entry(std::uint32_t pid, const std::string &module,
                        const std::string &linkage, const Revision &active, bool overwrite = true);

struct ExtensionSources {
  JitRequest jit;
  std::string build_id;
  std::uintptr_t entry = 0;
  std::function<void()> validate;
};

ExtensionSources extension_sources(std::uint32_t pid, const std::string &module,
    const clangtracer::v1::LiveUpdateRequest &request, const std::vector<std::string> &paths,
    const std::string &registry);

void validate_entry(std::uint32_t pid, const Entry &entry);

} // namespace tracer::live
