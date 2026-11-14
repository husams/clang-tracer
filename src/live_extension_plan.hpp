#pragma once

#include "live_plan.hpp"
#include "tracer.pb.h"

#include <functional>

namespace tracer::live {

struct ExtensionPlan {
  clangtracer::v1::LiveAgentExtension input;
  std::function<void()> validate;
  Plan native;
};

ExtensionPlan plan_extension(std::uint32_t pid, std::uint64_t process_start,
    const std::string &session, const std::string &executable,
    const clangtracer::v1::LiveUpdateRequest &request, const Revision &active,
    const std::function<bool()> &cancelled);

} // namespace tracer::live
