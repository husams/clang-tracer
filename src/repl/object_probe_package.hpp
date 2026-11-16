#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "tracer.pb.h"

namespace tracer_repl {

struct ObjectProbeCompileInputs {
  std::vector<std::string> includes;
  std::string compilation_database;
  std::string translation_unit;
  std::vector<std::string> extra_arguments;
};

struct ObjectProbePackageInput {
  std::uint32_t version = 1;
  std::string qualified_type;
  std::vector<std::string> methods;
  std::vector<std::string> fields;
  std::vector<std::string> writer_selectors;
  std::string callback_source;
  bool allow_dwarf_writer_fallback = false;
  ObjectProbeCompileInputs compile;
};

// Offline, versioned input to a later object-probe binding operation. The
// package contains no process or ELF-specific state; those are checked when
// the package is bound to a live target.
class ObjectProbePackage final {
public:
  using Artifact = clangtracer::v1::PreparedInjector;
  using PrepareMethod = std::function<Artifact(
      const std::string &exact_method, const ObjectProbeCompileInputs &compile)>;

  static constexpr std::uint32_t current_version = 1;

  static ObjectProbePackage prepare(const ObjectProbePackageInput &input,
                                    const PrepareMethod &prepare_method);
  static ObjectProbePackage load(const std::filesystem::path &path);

  void save(const std::filesystem::path &path) const;

  const ObjectProbePackageInput &input() const noexcept { return input_; }
  const std::vector<Artifact> &prepared_methods() const noexcept { return prepared_methods_; }

private:
  ObjectProbePackageInput input_;
  std::vector<Artifact> prepared_methods_;

  static void validate(const ObjectProbePackageInput &input,
                       const std::vector<Artifact> *artifacts = nullptr);
};

} // namespace tracer_repl
