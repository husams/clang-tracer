#pragma once

#include <memory>
#include <string>

#include "tracer.pb.h"

namespace clang { class Interpreter; }

namespace tracer::arm_snapshot {
struct Codec {
  std::unique_ptr<clang::Interpreter> compiler;
  using Sample = bool (*)(const void *, std::string &) noexcept;
  Sample sample = nullptr;
  Codec();
  ~Codec();
  Codec(Codec &&) noexcept;
  Codec &operator=(Codec &&) noexcept;
};

Codec compile(const clangtracer::v1::ProbeSpec &spec);
}
