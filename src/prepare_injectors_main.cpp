#include "cpp_resolver.hpp"
#include "header_context.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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
std::string image_digest(const fs::path &path) {
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
    for (int i = 1; i < argc; ++i) {
      const std::string option = argv[i];
      if (option == "--help") {
        std::cout << "Native header preparation: --selector SELECTOR --include "
                     "HEADER "
                     "--compdb FILE [--translation-unit FILE] "
                     "--output NEW_DIRECTORY [--module ELF]\n"
                     "By default, uses the first compile command for a source "
                     "beside the first header. --module pins the package to an ELF image.\n";
        return 0;
      }
      if (option != "--include" && option != "--selector" &&
          option != "--compdb" && option != "--translation-unit" &&
          option != "--module" && option != "--output")
        throw std::runtime_error("unknown option: " + option);
      if (++i == argc || std::string(argv[i]).empty())
        throw std::runtime_error("missing value for " + option);
      if (option == "--include")
        headers.push_back(fs::canonical(argv[i]).string());
      else if (!options.emplace(option, argv[i]).second)
        throw std::runtime_error("duplicate option: " + option);
    }
    for (const auto *option : {"--selector", "--compdb", "--output"})
      if (!options.count(option))
        throw std::runtime_error(std::string("missing required option: ") +
                                 option);
    if (headers.empty() || headers.size() > 16)
      throw std::runtime_error("provide 1..16 headers");
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
    if (const auto image = options.find("--module"); image != options.end()) {
      module = fs::canonical(image->second);
      digest = image_digest(module);
    }
    const auto unit = options.find("--translation-unit");
    tracer::client::resolve_header_context(
        context, options.at("--compdb"),
        unit == options.end() ? std::string{} : unit->second,
        tracer::client::HeaderCommandSelection::first_in_header_directory);
    const auto resolved = tracer::client::resolve_header_callable(
        options.at("--selector"), context);
    if (resolved.spec.signature().result() == api::POINTER)
      throw std::runtime_error("native prepared draft supports primitive value types only");
    for (const auto type : resolved.spec.signature().arguments())
      if (type == api::POINTER)
        throw std::runtime_error("native prepared draft supports primitive value types only");
    api::PreparedInjector prepared;
    prepared.set_format_version(module.empty() ? 2 : 1);
    if (!module.empty()) {
      prepared.set_module_sha256(digest);
      prepared.set_module_name(module.filename().string());
    }
    prepared.set_selector(options.at("--selector"));
    prepared.set_symbol(resolved.spec.symbol());
    *prepared.mutable_signature() = resolved.spec.signature();
    *prepared.mutable_argument_names() = resolved.spec.argument_names();
    prepared.set_target_triple("x86_64-unknown-linux-gnu");
    if (!module.empty() && image_digest(module) != digest)
      throw std::runtime_error("module changed during preparation");
    const auto paren = options.at("--selector").find('(');
    if (paren == std::string::npos)
      throw std::runtime_error(
          "use an explicit parameter list in the selector");
    const auto name = options.at("--selector").substr(0, paren);
    const auto marker = resolved.declaration.find(" " + name + "(");
    if (marker == std::string::npos || resolved.declaration.back() != ')')
      throw std::runtime_error("unsupported prepared declaration");
    const auto result = resolved.declaration.substr(0, marker);
    const auto arguments =
        resolved.declaration.substr(marker + name.size() + 2);
    std::string declaration = "#pragma once\n";
    for (const auto &header : headers)
      declaration += "#include \"" + header + "\"\n";
    declaration += "namespace tracer_prepared {\nusing Function = " + result +
                   " (*)(" + arguments + ";\nFunction target() noexcept;\n}\n";
    const auto wrapper =
        "#include \"adapter.hpp\"\nnamespace tracer_prepared {\n"
        "Function target() noexcept { return static_cast<Function>(&" +
        name + "); }\n}\n";
    if (!fs::create_directory(output))
      throw std::runtime_error("cannot create new output directory");
    try {
      fs::create_directory(output / "generated");
      write(output / "generated/adapter.hpp", declaration);
      write(output / "generated/adapter.cpp", wrapper);
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
