#pragma once

#include "tracer.pb.h"

namespace tracer {
inline bool has_scalar_overrides(const clangtracer::v1::ProbeSpec &spec) {
  return spec.allow_argument_overrides() || spec.allow_result_override();
}

inline const char *scalar_override_error(const clangtracer::v1::ProbeSpec &spec) {
  if (!has_scalar_overrides(spec))
    return nullptr;
  namespace api = clangtracer::v1;
  const auto scalar = [](api::ValueType type) {
    return (type >= api::INT32 && type <= api::FLOAT64) || type == api::BOOL;
  };
  const auto &signature = spec.signature();
  const auto &debug = spec.debug_info();
  if (!spec.has_debug_info() || spec.has_prepared() || signature.kind() != api::FUNCTION)
    return "scalar overrides require a DWARF-bound free function without a prepared package";
  if (debug.result_is_reference() || debug.result_is_object_value() ||
      (signature.result() != api::VOID && !scalar(signature.result())) ||
      debug.arguments_size() != signature.arguments_size())
    return "scalar overrides require an entirely scalar signature";
  for (int i = 0; i < signature.arguments_size(); ++i)
    if (!scalar(signature.arguments(i)) || debug.arguments(i).is_reference() ||
        debug.arguments(i).is_object_value())
      return "scalar overrides require an entirely scalar signature";
  if (spec.allow_argument_overrides() && signature.arguments().empty())
    return "argument overrides require at least one argument";
  if (spec.allow_result_override() && signature.result() == api::VOID)
    return "result overrides require a nonvoid return type";
  return nullptr;
}
} // namespace tracer
