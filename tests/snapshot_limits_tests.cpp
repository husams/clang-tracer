// The ranges, defaults and resolution of the followed-member limits.
#include "snapshot_limits.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char *what) {
  if (!ok) throw std::runtime_error(std::string("snapshot limits check failed: ") + what);
}
std::string error_of(const tracer::LimitSpec &spec, const std::string &text) {
  try {
    (void)tracer::parse_limit(spec, text);
  } catch (const std::invalid_argument &error) {
    return error.what();
  }
  return {};
}
} // namespace

int main() {
  using namespace tracer;
  for (const auto &spec : kLimitSpecs) {
    const std::string name = spec.name;
    require(find_limit(name) == &spec, "find_limit");
    require(spec.minimum <= spec.maximum, "range");
    const auto standard = default_limit(name);
    require(standard >= spec.minimum && standard <= spec.maximum, "default inside range");
    require(parse_limit(spec, std::to_string(standard)) == standard, "default round trip");
    require(parse_limit(spec, "unlimited") == spec.maximum, "unlimited selects the maximum");
    require(parse_limit(spec, std::to_string(spec.minimum)) == spec.minimum, "minimum");
    require(parse_limit(spec, std::to_string(spec.maximum)) == spec.maximum, "maximum");
    if (spec.minimum != 0) require(!error_of(spec, "off").empty(), "off refused above zero");
    else require(parse_limit(spec, "off") == 0, "off");
    if (spec.minimum > 0) {
      const auto message = error_of(spec, std::to_string(spec.minimum - 1));
      require(message.find("--" + name + " must be between " + std::to_string(spec.minimum) +
                           " and " + std::to_string(spec.maximum)) != std::string::npos,
              "below the range names the range");
    }
    if (spec.maximum != kUnlimited)
      require(error_of(spec, std::to_string(std::uint64_t{spec.maximum} + 1)).find("must be between") !=
                  std::string::npos,
              "above the range names the range");
    require(error_of(spec, "").find("invalid --" + name) != std::string::npos, "empty");
    require(error_of(spec, "12x").find("invalid --" + name) != std::string::npos, "not a number");
    require(error_of(spec, "-1").find("invalid --" + name) != std::string::npos, "negative");
    require(!error_of(spec, "99999999999").empty(), "too long");
    require(spec.env != nullptr && std::string(spec.env).rfind("TRACER_PREPARE_", 0) == 0, "env");
    require(spec.meaning && *spec.meaning, "meaning");
  }
  require(find_limit("no-such-limit") == nullptr, "unknown option");

  // The depth, in particular, is bounded by what the event can nest.
  require(find_limit("follow-depth")->maximum == kMaxFollowDepth, "follow-depth maximum");
  require(find_limit("snapshot-slice")->maximum == kMaxSliceBytes, "slice maximum");

  // defaults, then the package, then the probe
  const SnapshotLimits defaults = resolve_limits(nullptr, nullptr);
  require(defaults.follow_depth == 3 && defaults.follow_members == 48 &&
              defaults.string_bytes == 4000 && defaults.slice_bytes == 4096 &&
              defaults.arena_bytes == 16384,
          "built-in defaults");
  clangtracer::v1::SnapshotLimits prepared, probe;
  require(set_limit(prepared, "follow-depth", "5"), "set follow-depth");
  require(set_limit(prepared, "string-bytes", "100"), "set string-bytes");
  require(set_limit(probe, "string-bytes", "off"), "set off");
  require(set_limit(probe, "follow-members", "unlimited"), "set unlimited");
  require(!set_limit(probe, "nonsense", "1"), "unknown name");
  const auto layered = resolve_limits(&prepared, &probe);
  require(layered.follow_depth == 5, "package wins over the default");
  require(layered.string_bytes == 0, "probe wins over the package");
  require(layered.follow_members == kUnlimited, "unlimited reaches the generated code");
  require(layered.checked_reads == 256, "untouched options keep the default");

  // A string must fit its payload, a payload its arena.
  clangtracer::v1::SnapshotLimits wide;
  set_limit(wide, "string-bytes", "6000");
  const auto clamped = resolve_limits(&wide, nullptr);
  require(clamped.string_bytes == clamped.slice_bytes - 96, "a string is lowered to fit the slice");
  set_limit(wide, "snapshot-slice", "8192");
  const auto raised = resolve_limits(&wide, nullptr);
  require(raised.string_bytes == 6000 && raised.arena_bytes >= 8192,
          "raising the slice lifts the string and grows the arena");
  std::cout << "snapshot limits ok\n";
}
