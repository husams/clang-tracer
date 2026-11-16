#pragma once

#include "live_plan.hpp"

namespace tracer::live {

Plan plan_live_mock(std::uint32_t pid, const std::string& executable,
                    const clangtracer::v1::LiveUpdateRequest& request, const Revision& active,
                    const std::function<bool()>& cancelled);

} // namespace tracer::live
