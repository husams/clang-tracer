#include "prepare_elf_signature.hpp"

#include "abi_tags.hpp"
#include "type_text.hpp"

#include "prepared_contract.hpp"
#include "prepared_types.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"
#include "tracer/protocol.hpp"
#include "tracer_runtime_config.hpp"

#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/Error.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <deque>
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
  std::string display;  // the demangled spelling, ABI tags included
};

// An exact selector keeps precedence. A selector written without ABI tags may name the one
// callable that carries them: the second pass compares spellings that differ only in tag
// annotations, and callables with different tagged spellings stay ambiguous even at one address.
ElfCallable select_symbol(const llvm::object::ELF64LEObjectFile &image,
                          const std::string &selector, bool member,
                          const IndexedElfSignature *indexed, bool ignore_tags = false) {
  auto wanted = compact(selector);
  if (::tracer::compat::starts_with(wanted, "::")) wanted.erase(0, 2);
  // Tags are removed from the demangled text and the request alike; the first pass is exact.
  const auto key = [&](std::string_view text) {
    return ignore_tags ? ::tracer::strip_abi_tags(compact(text)) : compact(text);
  };
  if (ignore_tags) wanted = ::tracer::strip_abi_tags(wanted);
  std::optional<ElfCallable> match;
  bool ambiguous_indexed_linkage = false;
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
    // Candidates are judged by their characters, not their length: a long mangled name (nested
    // templates, long namespaces) is a candidate like any other and must not be skipped.
    if (linkage.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != linkage.npos)
      return;
    auto address = symbol.getAddress();
    if (!address) {
      llvm::consumeError(address.takeError());
      return;
    }
    if (indexed) {
      if (linkage != indexed->linkage) return;
      if (*address != indexed->address || elf.getSize() != indexed->size) {
        ambiguous_indexed_linkage = true;
        return;
      }
    }
    llvm::ItaniumPartialDemangler demangler;
    if (demangler.partialDemangle(linkage.c_str())) {
      if (indexed && !member && linkage.rfind("_Z", 0) != 0)
        match = ElfCallable{linkage, "", "", "", *address, elf.getSize(), linkage};
      return;
    }
    // Constructors and destructors are ordinary callables here. Their entry points (C1/C2,
    // D0/D1/D2) are told apart by linkage name (--id); aliases share one address.
    if (!demangler.isFunction() || demangler.isSpecialName())
      return;
    using Text = std::unique_ptr<char, decltype(&std::free)>;
    std::size_t capacity = 0;
    const Text qualified(demangler.getFunctionName(nullptr, &capacity), &std::free);
    if (!qualified || (!indexed && !::tracer::compat::starts_with(wanted, key(qualified.get()) + "(")))
      return;
    const Text params(demangler.getFunctionParameters(nullptr, &capacity), &std::free);
    const Text full(demangler.finishDemangle(nullptr, &capacity), &std::free);
    if (!params || !full)
      return;
    const std::string base = std::string(qualified.get()) + params.get();
    const std::string_view display(full.get());
    const auto start = display.find(base);
    if (start == display.npos || (!indexed && key(display.substr(start)) != wanted))
      return;
    if (match) {
      // Two names for the same code (C1/C2 or D1/D2 aliases) are one callable; different code
      // is ambiguous and is selected by linkage name.
      if (match->address != *address || match->size != elf.getSize())
        throw std::invalid_argument("selector matches multiple ELF function symbols");
      if (ignore_tags && match->display != std::string(display.substr(start)))
        throw std::invalid_argument(
            "selector matches ELF symbols with different ABI tags: " + match->display + ", " +
            std::string(display.substr(start)));
      return;
    }
    const Text result(demangler.getFunctionReturnType(nullptr, &capacity), &std::free);
    std::string receiver;
    if (member || (!indexed && demangler.hasFunctionQualifiers())) {
      const auto suffix = display.substr(start + base.size());
      if (suffix.find("const") != suffix.npos) receiver += "const ";
      if (suffix.find("volatile") != suffix.npos) receiver += "volatile ";
      receiver += "void*";
    }
    match = ElfCallable{linkage, params.get(), result ? result.get() : "",
                        std::move(receiver), *address, elf.getSize(),
                        std::string(display.substr(start))};
  };
  for (const auto &symbol : image.symbols())
    visit(symbol);
  for (const auto &symbol : image.getDynamicSymbolIterators())
    visit(symbol);
  if (ambiguous_indexed_linkage)
    throw std::invalid_argument("prepared function has no unique concrete ELF symbol");
  if (!match) {
    if (!indexed && !ignore_tags && selector.find("[abi:") == selector.npos)
      return select_symbol(image, selector, member, indexed, true);
    throw std::invalid_argument("no defined C++ ELF function matches the full selector");
  }
  return std::move(*match);
}

// Keeps the first compiler error so a failure can say what Clang could not resolve.
class FirstError final : public clang::TextDiagnosticPrinter {
public:
  explicit FirstError(clang::DiagnosticOptions &options)
      : clang::TextDiagnosticPrinter(llvm::errs(), options) {}
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &diagnostic) override {
    if (level >= clang::DiagnosticsEngine::Error && message.empty()) {
      llvm::SmallString<256> text;
      diagnostic.FormatDiagnostic(text);
      message = text.str().str();
    }
    clang::TextDiagnosticPrinter::HandleDiagnostic(level, diagnostic);
  }
  std::string message;
};

clang::QualType alias(clang::ASTContext &ast, llvm::StringRef name) {
  for (const auto *decl : ast.getTranslationUnitDecl()->lookup(&ast.Idents.get(name)))
    if (const auto *type = llvm::dyn_cast<clang::TypeAliasDecl>(decl))
      return type->getUnderlyingType();
  throw std::invalid_argument("invalid C++ signature");
}

// Without a type context only builtin types can be compiled: nothing defines a class.
void require_scalar(clang::QualType type, const std::string &selector, const std::string &role,
                    clang::ASTContext &ast) {
  const auto written = type;
  type = type.getNonReferenceType();
  while (type->isPointerType())
    type = type->getPointeeType();
  if (!type->isBuiltinType())
    throw std::invalid_argument(
        selector + ": " + role + " type '" + tracer::prepared::type_name(ast, written) +
        "' is not a builtin type, pointer or reference to one; its complete definition is "
        "needed as a type context (--include HEADER --compdb FILE)");
}
// A callable requested without its ABI tags is recorded with them, as an indexed package is.
bool indexed_has_tags(const std::string &text) { return text.find("[abi:") != text.npos; }

std::string tagged_selector(const std::string &requested, const ElfCallable &target) {
  if (indexed_has_tags(requested) || target.display == ::tracer::strip_abi_tags(target.display) ||
      target.display.find('(') == target.display.npos)
    return {};
  return target.display;
}
} // namespace

ResolvedCallable resolve_elf_signature(
    const llvm::object::ELF64LEObjectFile &image, const std::string &selector,
    const std::string &signature, bool member, const IndexedElfSignature *indexed,
    const TypeContext *context) {
  if (!indexed && !tracer::spliceable_type_text(signature))
    throw std::invalid_argument(
        "--signature requires a C++ function type such as 'const int&()' (it must not contain "
        "; { } # \\ or comments)");
  const auto target = select_symbol(image, selector, member, indexed);
  if (context) {
    SymbolContract contract;
    contract.symbol = target.symbol;
    contract.display = selector;
    contract.signature = signature;
    contract.encoded_parameters = target.parameters;
    contract.encoded_result = target.result;
    contract.receiver = target.receiver;
    if (indexed) {
      contract.result_type = indexed->result_type;
      contract.argument_types = indexed->argument_types;
      contract.declarations = indexed->preamble;
      contract.result_size = indexed->result_size;
      contract.argument_sizes = indexed->argument_sizes;
    }
    auto resolved = resolve_symbol_in_type_context(contract, *context);
    resolved.package_selector = tagged_selector(selector, target);
    return resolved;
  }
  if (!indexed && (!tracer::spliceable_type_text(target.parameters) ||
      (!target.result.empty() && !tracer::spliceable_type_text(target.result))))
    throw std::invalid_argument(
        selector + ": the symbol's parameter or result types cannot be compiled without a type "
        "context (--include HEADER --compdb FILE)");

  ResolvedCallable resolved;
  resolved.requires_typed_adapter = true;
  auto &package = *resolved.spec.mutable_prepared();
  package.set_format_version(tracer::prepared::typed_version);
  package.set_adapter_uses_default_toolchain(true);
  package.add_adapter_toolchain_options("--no-default-config");
  if (indexed) package.add_adapter_toolchain_options("--target=x86_64-unknown-linux-gnu");
  auto arguments = tracer::prepared::toolchain_arguments(
      package, {runtime::jit_flags.begin(), runtime::jit_flags.end()});
  arguments.insert(arguments.end(), {"-std=c++20", "-x", "c++",
                                     "-resource-dir=" TRACER_CLANG_RESOURCE_DIR});
  // The units keep a pointer to their diagnostic consumer, so each one is kept alive with them.
  struct Diagnostics {
    clang::DiagnosticOptions options;
    FirstError consumer{options};
  };
  std::deque<Diagnostics> consumers;
  const auto parse = [&](const std::string &source, const char *what = "signature") {
    auto &diagnostics = consumers.emplace_back().consumer;
    auto unit = clang::tooling::buildASTFromCodeWithArgs(
        source, tracer::clang_driver_arguments(arguments), "elf-signature.cc", "clang-tool",
        std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripDependencyFileAdjuster(), {}, &diagnostics);
    if (!unit || unit->getDiagnostics().hasErrorOccurred())
      throw std::invalid_argument(
          selector + ": Clang cannot compile the " + what + " (" + diagnostics.message +
          ")" + (indexed ? "" : "; a class or other named type needs its definition supplied as "
          "a type context (--include HEADER --compdb FILE)"));
    return unit;
  };

  const auto preamble = indexed ? indexed->preamble : "";
  auto types = parse(preamble + "using Supplied = " + signature + ";\n" +
                     (target.parameters.empty() ? "using Encoded = Supplied;\n"
                         : "using Encoded = void" + target.parameters + ";\n") +
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
  if (!indexed) require_scalar(result, selector, "result", ast);
  llvm::SHA256 identity;
  identity.update(target.symbol + ":" + target.receiver + ":" + compact(signature));
  const auto digest = identity.final();
  const auto scope = "tracer_elf_adapter_" +
      llvm::toHex(llvm::ArrayRef<std::uint8_t>(digest).take_front(16), true);
  std::string declarations = preamble + "namespace " + scope + " {\nusing Result = " +
      tracer::prepared::type_name(ast, result) + ";\n";
  std::string parameters = target.receiver.empty() ? "" : target.receiver + " receiver";
  for (unsigned i = 0; i < supplied->getNumParams(); ++i) {
    const auto type = supplied->getParamType(i);
    if (!ast.hasSameType(type, encoded->getParamType(i)))
      throw std::invalid_argument("signature parameters differ from the ELF symbol");
    if (!indexed) require_scalar(type, selector, "argument " + std::to_string(i), ast);
    const auto argument = "Arg" + std::to_string(i);
    declarations += "using " + argument + " = " + tracer::prepared::type_name(ast, type) + ";\n";
    if (!parameters.empty()) parameters += ", ";
    parameters += argument + " arg" + std::to_string(i);
  }
  declarations += "Result target(" + parameters + ") __asm__(\"" + target.symbol + "\");\n}\n";

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
  resolved.package_selector = tagged_selector(selector, target);
  return resolved;
}
} // namespace tracer::client
