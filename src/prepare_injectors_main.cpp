#include "cpp_resolver.hpp"
#include "prepare_package.hpp"
#include "module_index_internal.hpp"
#include "tracer/module_index.hpp"
#include "header_context.hpp"
#include "prepare_elf_signature.hpp"
#include "prepared_contract.hpp"
#include "tracer/compiler_options.hpp"

#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <llvm/ADT/StringExtras.h>

namespace fs = std::filesystem;
namespace api = clangtracer::v1;

namespace {
std::string image_digest(
    const fs::path &path,
    const std::function<void(const llvm::object::ELF64LEObjectFile &)> &inspect = {}) {
  return llvm::fromHex(tracer::module_index::inspect_elf(path, inspect));
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
                     "--output NEW_DIRECTORY [--module ELF] [--snapshot-return-type TYPE] [--metadata-only] "
                     "[--extra-arg ARG]...\n"
                     "ELF preparation: --selector SELECTOR --signature TYPE "
                     "--module ELF --output NEW_DIRECTORY [--member] [--metadata-only]\n"
                     "Indexed preparation: --selector SELECTOR --module-index INDEX "
                     "--output NEW_DIRECTORY [--module ELF] [--id ID] [--generated DIR]\n"
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
      if (option != "--module-index" && option != "--generated" && option != "--id" &&
          option != "--include" && option != "--selector" &&
          option != "--compdb" && option != "--translation-unit" &&
          option != "--module" && option != "--output" && option != "--extra-arg" &&
          option != "--signature" && option != "--snapshot-return-type")
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
    if (!options.count("--module-index") && headers.empty() && !options.count("--signature") &&
        !options.count("--compdb") && !options.count("--translation-unit")) {
      if (const auto image = options.find("--module"); image != options.end()) {
        const auto sidecar = image->second + ".tracer.sqlite";
        if (fs::is_regular_file(sidecar)) options.emplace("--module-index", sidecar);
      }
      if (!options.count("--module-index") && fs::is_regular_file("module-index.sqlite"))
        options.emplace("--module-index", "module-index.sqlite");
      if (!options.count("--module-index"))
        throw std::invalid_argument("no default module index; pass --module-index INDEX");
    }
    if (const auto indexed = options.find("--module-index"); indexed != options.end()) {
      if (!headers.empty() || !extra_arguments.empty() || member ||
          options.count("--compdb") || options.count("--translation-unit") ||
          options.count("--signature") || options.count("--snapshot-return-type"))
        throw std::invalid_argument("--module-index is incompatible with source or signature options");
      const auto index = tracer_module::ModuleIndex::open(indexed->second);
      const auto &selector = options.at("--selector");
      const auto symbol = options.count("--id") ? index.get_symbol(options.at("--id"))
                                               : index.find_symbol(selector);
      if (tracer::module_index::normalize_selector(selector) !=
              tracer::module_index::normalize_selector(symbol.selector) &&
          selector != symbol.name && selector != symbol.linkage &&
          options.count("--id"))
        throw std::invalid_argument("--id does not match the requested selector");
      const auto output = tracer::module_index::prepare_indexed(index, symbol.id,
          options.count("--module") ? options.at("--module") : "",
          options.at("--output"), options.count("--generated") ? options.at("--generated") : "",
          metadata_only);
      std::cout << "prepared " << output << "\nsymbol=" << symbol.linkage << '\n';
      return 0;
    }
    if (options.count("--generated") || options.count("--id"))
      throw std::invalid_argument("--generated and --id require indexed preparation");
    const auto signature_option = options.find("--signature");
    const bool elf_mode = signature_option != options.end();
    const auto snapshot_type_option = options.find("--snapshot-return-type");
    if (snapshot_type_option != options.end() && snapshot_type_option->second.empty())
      throw std::invalid_argument("--snapshot-return-type requires a type name");
    if (elf_mode) {
      if (!options.count("--module"))
        throw std::invalid_argument("--signature requires --module");
      if (!headers.empty() || options.count("--compdb") ||
          options.count("--translation-unit") || !extra_arguments.empty() ||
          snapshot_type_option != options.end())
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
            options.at("--selector"), context, command, pointer_layouts,
            snapshot_type_option == options.end()
                ? std::string{}
                : snapshot_type_option->second);
        auto &adapter = *callable.spec.mutable_prepared();
        tracer::client::verify_prepared_adapter(options.at("--selector"), adapter,
                                                callable, pointer_layouts);
        return callable;
      };
      try {
        return resolve_adapter(true);
      } catch (const std::runtime_error &error) {
        if (snapshot_type_option != options.end())
          throw;
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
    const auto prepared = tracer::client::write_prepared_package(
        resolved, options.at("--selector"), module, digest, output, metadata_only,
        translation_unit, cl_driver);
    std::cout << "prepared " << output << "\n"
              << resolved.declaration << "\nsymbol=" << prepared.symbol()
              << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "prepare: " << error.what() << '\n';
    return 1;
  }
}
