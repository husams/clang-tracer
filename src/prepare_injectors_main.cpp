#include "cpp_resolver.hpp"
#include "header_context.hpp"
#include "prepare_elf_signature.hpp"
#include "prepared_contract.hpp"
#include "tracer/compiler_options.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/SHA256.h>

namespace fs = std::filesystem;
namespace api = clangtracer::v1;

namespace {
std::string image_digest(
    const fs::path &path,
    const std::function<void(const llvm::object::ELF64LEObjectFile &)> &inspect = {}) {
  auto binary = llvm::object::createBinary(path.string());
  if (!binary)
    throw std::runtime_error(llvm::toString(binary.takeError()));
  const auto *object =
      llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(binary->getBinary());
  if (!object ||
      object->getELFFile().getHeader().e_machine != llvm::ELF::EM_X86_64 ||
      (object->getELFFile().getHeader().e_type != llvm::ELF::ET_EXEC &&
       object->getELFFile().getHeader().e_type != llvm::ELF::ET_DYN))
    throw std::runtime_error("module must be a linked Linux x86_64 ELF image");
  if (inspect) inspect(*object);
  const auto bytes = binary->getBinary()->getMemoryBufferRef().getBuffer();
  const auto digest = llvm::SHA256::hash(llvm::ArrayRef<std::uint8_t>(
      reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size()));
  return std::string(reinterpret_cast<const char *>(digest.data()),
                     digest.size());
}

void write(const fs::path &path, const std::string &data) {
  std::ofstream out(path, std::ios::binary);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!out)
    throw std::runtime_error("cannot write generated artifact: " +
                             path.string());
}
} // namespace

int main(int argc, char **argv) {
  try {
    std::map<std::string, std::string> options;
    std::vector<std::string> headers;
    std::vector<std::string> extra_arguments;
    bool metadata_only = false;
    bool member = false;
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        std::cout << "Native header preparation: --selector SELECTOR --include "
                     "HEADER "
                     "--compdb FILE [--translation-unit FILE] "
                     "--output NEW_DIRECTORY [--module ELF] [--metadata-only] "
                     "[--extra-arg ARG]...\n"
                     "ELF preparation: --selector SELECTOR --signature TYPE "
                     "--module ELF --output NEW_DIRECTORY [--member] [--metadata-only]\n"
                     "Uses the first include's unique compile command if recorded, "
                     "or the first command for a source beside it. "
                     "--module pins the package to an ELF image.\n";
        return 0;
      }
      if (option == "--metadata-only" || option == "--member") {
        auto &flag = option == "--metadata-only" ? metadata_only : member;
        if (flag)
          throw std::runtime_error("duplicate option: " + option);
        flag = true;
        continue;
      }
      if (option != "--include" && option != "--selector" &&
          option != "--compdb" && option != "--translation-unit" &&
          option != "--module" && option != "--output" && option != "--extra-arg" &&
          option != "--signature")
        throw std::runtime_error("unknown option: " + option);
      if (++i == argc || std::string(argv[i]).empty())
        throw std::runtime_error("missing value for " + option);
      if (option == "--include")
        headers.push_back(fs::canonical(argv[i]).string());
      else if (option == "--extra-arg")
        extra_arguments.emplace_back(argv[i]);
      else if (!options.emplace(option, argv[i]).second)
        throw std::runtime_error("duplicate option: " + option);
    }
    for (const auto *option : {"--selector", "--output"})
      if (!options.count(option))
        throw std::runtime_error(std::string("missing required option: ") +
                                 option);
    const auto signature_option = options.find("--signature");
    const bool elf_mode = signature_option != options.end();
    if (elf_mode) {
      if (!options.count("--module"))
        throw std::invalid_argument("--signature requires --module");
      if (!headers.empty() || options.count("--compdb") ||
          options.count("--translation-unit") || !extra_arguments.empty())
        throw std::invalid_argument("--signature cannot be combined with source or compiler options");
    } else {
      if (member)
        throw std::invalid_argument("--member requires --signature");
      if (!options.count("--compdb"))
        throw std::runtime_error("missing required option: --compdb");
      if (headers.empty() || headers.size() > 16)
        throw std::runtime_error("provide 1..16 headers");
    }
    const auto output = fs::absolute(options.at("--output"));
    if (fs::exists(output) || fs::is_symlink(output))
      throw std::runtime_error("output already exists; choose a new directory");
    api::ProbeSpec context;
    for (const auto &header : headers) {
      if (header.find_first_of("\"\r\n") != std::string::npos)
        throw std::runtime_error("invalid header path");
      context.add_include_headers(header);
    }
    fs::path module;
    std::string digest;
    std::optional<tracer::client::ResolvedCallable> elf_callable;
    if (const auto image = options.find("--module"); image != options.end()) {
      module = fs::canonical(image->second);
      digest = image_digest(module, [&](const auto &object) {
        if (elf_mode)
          elf_callable = tracer::client::resolve_elf_signature(
              object, options.at("--selector"), signature_option->second, member);
      });
    }
    std::string translation_unit;
    bool cl_driver = false;
    const auto resolved = [&] {
      if (elf_callable)
        return std::move(*elf_callable);

      const auto unit = options.find("--translation-unit");
      auto command = tracer::client::resolve_prepared_header_command(
          headers.front(), options.at("--compdb"),
          unit == options.end() ? std::string{} : unit->second);
      translation_unit = command.translation_unit;
      cl_driver = command.driver_mode == "cl";
      const auto supplemental = tracer::normalize_extra_compile_arguments(
          extra_arguments, command.arguments, command.directory);
      for (const auto &argument : supplemental)
        command.arguments.push_back(cl_driver ? "/clang:" + argument : argument);
      const auto resolve_adapter = [&](bool pointer_layouts) {
        auto callable = tracer::client::resolve_header_callable(
            options.at("--selector"), context, command, pointer_layouts);
        auto &adapter = *callable.spec.mutable_prepared();
        tracer::client::verify_prepared_adapter(options.at("--selector"), adapter,
                                                callable, pointer_layouts);
        return callable;
      };
      try {
        return resolve_adapter(true);
      } catch (const std::runtime_error &error) {
        const auto original = std::current_exception();
        try {
          auto callable = resolve_adapter(false);
          std::cerr << "prepare: retaining address-only pointers: " << error.what() << '\n';
          return callable;
        } catch (const std::runtime_error &) {
          std::rethrow_exception(original);
        }
      }
    }();
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
    prepared.set_selector(options.at("--selector"));
    prepared.set_symbol(resolved.spec.symbol());
    *prepared.mutable_signature() = resolved.spec.signature();
    *prepared.mutable_argument_names() = resolved.spec.argument_names();
    if (!module.empty() && image_digest(module) != digest)
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
    const auto name = options.at("--selector").substr(0, options.at("--selector").find('('));
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
        write(output / "generated/adapter.hpp", declaration);
        write(output / "generated/adapter.cpp", wrapper);
      }
      write(output / "prepared.pb", prepared.SerializeAsString());
    } catch (...) {
      fs::remove_all(output);
      throw;
    }
    std::cout << "prepared " << output << "\n"
              << resolved.declaration << "\nsymbol=" << prepared.symbol()
              << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "prepare: " << error.what() << '\n';
    return 1;
  }
}
