#include "tracer/compat.hpp"
#include "prepare_package.hpp"
#include "module_index_internal.hpp"
#include "prepared_adapter.hpp"
#include "prepared_contract.hpp"

#include <llvm/ADT/StringExtras.h>

namespace tracer::client {
namespace fs = std::filesystem;
namespace api = clangtracer::v1;

api::PreparedInjector write_prepared_package(
    const ResolvedCallable &resolved, const std::string &selector,
    const fs::path &module, const std::string &digest, const fs::path &output,
    bool metadata_only, const std::string &translation_unit, bool cl_driver) {
  const auto &signature = resolved.spec.signature();
  const auto &types = resolved.spec.debug_info();
  const auto legacy = [](api::ValueType type, const std::string &spelling) {
    switch (type) {
    case api::VOID: return spelling == "void";
    case api::INT32: return spelling == "int";
    case api::INT64: return spelling == "long long";
    case api::UINT64: return spelling == "unsigned long long";
    case api::FLOAT32: return spelling == "float";
    case api::FLOAT64: return spelling == "double";
    default: return false;
    }
  };
  bool typed = cl_driver || resolved.requires_typed_adapter || signature.kind() == api::METHOD ||
               !legacy(signature.result(), types.result_cpp_type());
  for (int i = 0; i < signature.arguments_size(); ++i)
    typed |= !legacy(signature.arguments(i), types.arguments(i).cpp_type());
  auto prepared = resolved.spec.prepared();
  const auto adapter = tracer::prepared::adapter_source(prepared);
  prepared.set_format_version(typed ? tracer::prepared::typed_version : module.empty() ? 2 : 1);
  if (typed) {
    *prepared.mutable_header_types() = types;
    prepared.set_source_translation_unit(translation_unit);
  } else {
    prepared.clear_adapter_declarations();
    prepared.clear_adapter_standard_headers();
    prepared.clear_adapter_abi_options();
    prepared.clear_adapter_toolchain_options();
    prepared.clear_adapter_uses_default_toolchain();
  }
  if (!module.empty()) {
    prepared.set_module_sha256(digest);
    prepared.set_module_name(module.filename().string());
  }
  prepared.set_selector(selector);
  prepared.set_symbol(resolved.spec.symbol());
  *prepared.mutable_signature() = resolved.spec.signature();
  *prepared.mutable_argument_names() = resolved.spec.argument_names();
  const bool arm_snapshot = !prepared.receiver_storage().empty() &&
      ::tracer::compat::starts_with(prepared.target_triple(), "aarch64-");
  if (!module.empty() && module_index::inspect_elf(module, [&](const module_index::Elf &image) {
        if (arm_snapshot != (image.getArch() == llvm::Triple::aarch64))
          throw std::runtime_error("prepared receiver ABI does not match the ELF architecture");
      }, {}, arm_snapshot) != llvm::toHex(llvm::ArrayRef<std::uint8_t>(
      reinterpret_cast<const std::uint8_t *>(digest.data()), digest.size()), true))
    throw std::runtime_error("module changed during preparation");
  if (prepared.ByteSizeLong() > 64 * 1024)
    throw std::runtime_error("prepared metadata exceeds 64 KiB");
  std::string arguments;
  for (const auto &arg : types.arguments()) {
    if (!arguments.empty())
      arguments += ", ";
    arguments += arg.cpp_type();
  }
  std::string declaration = "#pragma once\n" + adapter;
  declaration += "namespace tracer_prepared {\nusing Result = " +
                 types.result_cpp_type() + ";\nusing Signature = Result(" +
                 arguments + ");\n";
  std::string wrapper = "#include \"adapter.hpp\"\n";
  const auto name = selector.substr(0, selector.find('('));
  if (signature.kind() == api::METHOD) {
    declaration += "using Receiver = " + types.record_name() + ";\n";
  } else if (!typed) {
    declaration += "using Function = Signature*;\nFunction target() noexcept;\n";
    wrapper += "namespace tracer_prepared {\n"
               "Function target() noexcept { return static_cast<Function>(&" +
               name + "); }\n}\n";
  }
  declaration += "}\n";
  if (!fs::create_directory(output))
    throw std::runtime_error("cannot create new output directory");
  try {
    if (!metadata_only) {
      fs::create_directory(output / "generated");
      module_index::write_file(output / "generated/adapter.hpp", declaration);
      module_index::write_file(output / "generated/adapter.cpp", wrapper);
    }
    module_index::write_file(output / "prepared.pb", prepared.SerializeAsString());
  } catch (...) {
    fs::remove_all(output);
    throw;
  }
  return prepared;
}
} // namespace tracer::client
