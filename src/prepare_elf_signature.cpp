#include "prepare_elf_signature.hpp"

#include "prepared_contract.hpp"
#include "prepared_types.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"
#include "tracer/protocol.hpp"
#include "tracer_runtime_config.hpp"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Error.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace tracer::client {
namespace {
namespace api = clangtracer::v1;

std::string compact(std::string_view value) {
  std::string result;
  for (unsigned char c : value)
    if (!std::isspace(c))
      result += c;
  return result;
}

struct ElfCallable {
  std::string symbol;
  std::string parameters;
  std::string result;
  std::string receiver;
  std::uint64_t address;
  std::uint64_t size;
};

ElfCallable select_symbol(const llvm::object::ELF64LEObjectFile &image,
                          const std::string &selector, bool member) {
  auto wanted = compact(selector);
  if (::tracer::compat::starts_with(wanted, "::")) wanted.erase(0, 2);
  std::optional<ElfCallable> match;
  const auto visit = [&](const llvm::object::SymbolRef &symbol) {
    const llvm::object::ELFSymbolRef elf(symbol);
    if (elf.getELFType() != llvm::ELF::STT_FUNC || !elf.getSize())
      return;
    auto flags = symbol.getFlags();
    if (!flags) {
      llvm::consumeError(flags.takeError());
      return;
    }
    if (*flags & llvm::object::SymbolRef::SF_Undefined)
      return;
    auto name = symbol.getName();
    if (!name) {
      llvm::consumeError(name.takeError());
      return;
    }
    const auto linkage = name->str();
    if (linkage.size() > 511 || linkage.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != linkage.npos)
      return;
    llvm::ItaniumPartialDemangler demangler;
    if (demangler.partialDemangle(linkage.c_str()) || !demangler.isFunction() ||
        demangler.isSpecialName() || demangler.isCtorOrDtor())
      return;
    using Text = std::unique_ptr<char, decltype(&std::free)>;
    std::size_t capacity = 0;
    const Text qualified(demangler.getFunctionName(nullptr, &capacity), &std::free);
    if (!qualified || !::tracer::compat::starts_with(wanted, compact(qualified.get()) + "("))
      return;
    const Text params(demangler.getFunctionParameters(nullptr, &capacity), &std::free);
    const Text full(demangler.finishDemangle(nullptr, &capacity), &std::free);
    if (!params || !full)
      return;
    const std::string base = std::string(qualified.get()) + params.get();
    const std::string_view display(full.get());
    const auto start = display.find(base);
    if (start == display.npos || compact(display.substr(start)) != wanted)
      return;
    auto address = symbol.getAddress();
    if (!address) {
      llvm::consumeError(address.takeError());
      return;
    }
    if (match) {
      if (match->symbol != linkage || match->address != *address ||
          match->size != elf.getSize())
        throw std::invalid_argument("selector matches multiple ELF function symbols");
      return;
    }
    const Text result(demangler.getFunctionReturnType(nullptr, &capacity), &std::free);
    std::string receiver;
    if (member || demangler.hasFunctionQualifiers()) {
      const auto suffix = display.substr(start + base.size());
      if (suffix.find("const") != suffix.npos) receiver += "const ";
      if (suffix.find("volatile") != suffix.npos) receiver += "volatile ";
      receiver += "void*";
    }
    match = ElfCallable{linkage, params.get(), result ? result.get() : "",
                        std::move(receiver), *address, elf.getSize()};
  };
  for (const auto &symbol : image.symbols())
    visit(symbol);
  for (const auto &symbol : image.getDynamicSymbolIterators())
    visit(symbol);
  if (!match)
    throw std::invalid_argument("no defined C++ ELF function matches the full selector");
  return std::move(*match);
}

bool type_text(std::string_view text) {
  return !text.empty() && text.size() <= 4096 && text.find_first_not_of(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_()*,& \t") == text.npos;
}

clang::QualType alias(clang::ASTContext &ast, llvm::StringRef name) {
  for (const auto *decl : ast.getTranslationUnitDecl()->lookup(&ast.Idents.get(name)))
    if (const auto *type = llvm::dyn_cast<clang::TypeAliasDecl>(decl))
      return type->getUnderlyingType();
  throw std::invalid_argument("invalid C++ signature");
}

void require_scalar(clang::QualType type) {
  type = type.getNonReferenceType();
  while (type->isPointerType())
    type = type->getPointeeType();
  if (!type->isBuiltinType())
    throw std::invalid_argument("ELF signature mode requires builtin types, pointers, or references");
}
} // namespace

ResolvedCallable resolve_elf_signature(
    const llvm::object::ELF64LEObjectFile &image, const std::string &selector,
    const std::string &signature, bool member) {
  if (!type_text(signature))
    throw std::invalid_argument("--signature requires a C++ function type such as 'const int&()'");
  const auto target = select_symbol(image, selector, member);
  if (!type_text(target.parameters) ||
      (!target.result.empty() && !type_text(target.result)))
    throw std::invalid_argument("ELF signature mode requires builtin types, pointers, or references");

  ResolvedCallable resolved;
  resolved.requires_typed_adapter = true;
  auto &package = *resolved.spec.mutable_prepared();
  package.set_format_version(tracer::prepared::typed_version);
  package.set_adapter_uses_default_toolchain(true);
  package.add_adapter_toolchain_options("--no-default-config");
  auto arguments = tracer::prepared::toolchain_arguments(
      package, {runtime::jit_flags.begin(), runtime::jit_flags.end()});
  arguments.insert(arguments.end(), {"-std=c++23", "-x", "c++",
                                     "-resource-dir=" TRACER_CLANG_RESOURCE_DIR});
  const auto parse = [&](const std::string &source) {
    auto unit = clang::tooling::buildASTFromCodeWithArgs(
        source, tracer::clang_driver_arguments(arguments), "elf-signature.cc");
    if (!unit || unit->getDiagnostics().hasErrorOccurred())
      throw std::invalid_argument("Clang rejected the C++ signature");
    return unit;
  };

  auto types = parse("using Supplied = " + signature + ";\nusing Encoded = void" +
                     target.parameters + ";\n" +
                     (target.result.empty() ? "" : "using EncodedResult = " + target.result + ";\n"));
  auto &ast = types->getASTContext();
  const auto *supplied = alias(ast, "Supplied")->getAs<clang::FunctionProtoType>();
  const auto *encoded = alias(ast, "Encoded")->getAs<clang::FunctionProtoType>();
  if (!supplied || !encoded || supplied->isVariadic() || encoded->isVariadic() ||
      supplied->getCallConv() != clang::CC_C || !supplied->getMethodQuals().empty() ||
      supplied->getRefQualifier() != clang::RQ_None || supplied->hasExtParameterInfos() ||
      supplied->getExtInfo() != clang::FunctionType::ExtInfo{})
    throw std::invalid_argument("--signature requires a fixed-arity default-ABI function type");
  if (supplied->getNumParams() != encoded->getNumParams())
    throw std::invalid_argument("signature parameters differ from the ELF symbol");
  if (!target.result.empty() &&
      !ast.hasSameType(supplied->getReturnType(), alias(ast, "EncodedResult")))
    throw std::invalid_argument("signature result differs from the ELF symbol");

  const auto result = supplied->getReturnType();
  require_scalar(result);
  llvm::SHA256 identity;
  identity.update(target.symbol + ":" + target.receiver + ":" + compact(signature));
  const auto digest = identity.final();
  const auto scope = "tracer_elf_adapter_" +
      llvm::toHex(llvm::ArrayRef<std::uint8_t>(digest).take_front(16), true);
  std::string declarations = "namespace " + scope + " {\nusing Result = " +
      tracer::prepared::type_name(ast, result) + ";\n";
  std::string parameters = target.receiver.empty() ? "" : target.receiver + " receiver";
  for (unsigned i = 0; i < supplied->getNumParams(); ++i) {
    const auto type = supplied->getParamType(i);
    if (!ast.hasSameType(type, encoded->getParamType(i)))
      throw std::invalid_argument("signature parameters differ from the ELF symbol");
    require_scalar(type);
    const auto argument = "Arg" + std::to_string(i);
    declarations += "using " + argument + " = " + tracer::prepared::type_name(ast, type) + ";\n";
    if (!parameters.empty()) parameters += ", ";
    parameters += argument + " arg" + std::to_string(i);
  }
  declarations += "Result target(" + parameters + ") __asm__(\"" + target.symbol + "\");\n}\n";
  if (supplied->getNumParams() + !target.receiver.empty() > kMaxArguments)
    throw std::invalid_argument("ELF signature has too many ABI arguments");

  auto adapter = parse(declarations);
  auto &adapter_ast = adapter->getASTContext();
  const clang::FunctionDecl *function = nullptr;
  for (const auto *decl : adapter_ast.getTranslationUnitDecl()->lookup(
           &adapter_ast.Idents.get(scope)))
    if (const auto *space = llvm::dyn_cast<clang::NamespaceDecl>(decl))
      for (const auto *item : space->lookup(&adapter_ast.Idents.get("target")))
        function = llvm::dyn_cast<clang::FunctionDecl>(item);
  if (!function)
    throw std::runtime_error("could not find the generated ELF adapter function");
  resolved.spec.mutable_signature()->set_kind(api::FUNCTION);
  tracer::prepared::describe_prepared_callable(adapter_ast, *function, nullptr,
                                               resolved.spec);
  for (const auto *parameter : function->parameters())
    resolved.spec.add_argument_names(parameter->getNameAsString());
  resolved.spec.set_symbol(target.symbol);
  package.set_target_triple(adapter_ast.getTargetInfo().getTriple().str());
  package.set_adapter_declarations(std::move(declarations));
  resolved.declaration = tracer::prepared::type_name(ast, result) + " " + selector;
  return resolved;
}
} // namespace tracer::client
