#pragma once

#include "live_update.hpp"
#include "tracer.pb.h"

#include <functional>

namespace tracer::live {

struct ExtensionPlan {
  clangtracer::v1::LiveAgentExtension input;
  std::function<void()> validate;
};

ExtensionPlan plan_extension(std::uint32_t pid, std::uint64_t process_start,
    const std::string &session, const std::string &executable,
    const clangtracer::v1::LiveUpdateRequest &request);

} // namespace tracer::live
