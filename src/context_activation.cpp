#include "context_activation.hpp"

#include <algorithm>
#include <any>
#include <cctype>
#include <filesystem>
#include <functional>
#include <map>
#include <new>
#include <set>
#include <string_view>
#include <utility>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Version.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/raw_ostream.h>

#include "context_analysis.hpp"
#include "context_codec.hpp"
#include "context_facade.hpp"
#include "context_hooks.hpp"
#include "context_lifecycle.hpp"
#include "context_owner.hpp"
#include "context_package.hpp"
#include "context_paths.hpp"
#include "context_schema.hpp"
#include "context_stdlib.hpp"
#include "context_support_effects.hpp"
#include "tracer/compat.hpp"
#include "tracer/compiler_options.hpp"

namespace tracer {
namespace {
using namespace context_engine;

struct RecordFinder final : clang::RecursiveASTVisitor<RecordFinder> {
  explicit RecordFinder(std::string requested) : name(std::move(requested)) {}

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *record) {
    if (record->isThisDeclarationADefinition() && !record->isImplicit() &&
        context_class_type_name(*record) == name) {
      found = record;
    }
    return true;
  }

  std::string name;
  const clang::CXXRecordDecl *found = nullptr;
};

struct AliasFinder final : clang::RecursiveASTVisitor<AliasFinder> {
  bool VisitTypeAliasDecl(clang::TypeAliasDecl *declaration) {
    if (declaration->getName() == "__clangtracer_context_payload_type") {
      found = declaration;
    }
    return true;
  }

  const clang::TypeAliasDecl *found = nullptr;
};

// Keeps Clang's errors so a failed parse reports what was rejected.
class ParseErrorCollector final : public clang::DiagnosticConsumer {
public:
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    if (level < clang::DiagnosticsEngine::Error) {
      return;
    }
    llvm::SmallString<256> message;
    info.FormatDiagnostic(message);
    ContextSourceError error;
    error.message.assign(message.begin(), message.end());
    if (info.hasSourceManager() && info.getLocation().isValid()) {
      const auto location =
          info.getSourceManager().getPresumedLoc(info.getLocation());
      if (location.isValid()) {
        error.file = location.getFilename();
        error.line = location.getLine();
        error.column = location.getColumn();
      }
    }
    errors_.push_back(std::move(error));
  }

  std::vector<ContextSourceError> take() noexcept { return std::move(errors_); }

private:
  std::vector<ContextSourceError> errors_;
};

// Renders errors one per line, in Clang's file:line:column form.
std::string error_lines(const std::vector<ContextSourceError> &errors) {
  std::string text;
  for (const auto &error : errors) {
    if (!text.empty()) {
      text += '\n';
    }
    if (!error.file.empty()) {
      text += error.file + ":" + std::to_string(error.line) + ":" +
              std::to_string(error.column) + ": ";
    }
    text += "error: " + error.message;
  }
  return text;
}

std::unique_ptr<clang::ASTUnit>
parse_source(const std::string &source,
             const std::vector<std::string> &arguments,
             const std::string &file_name,
             std::vector<ContextSourceError> *errors = nullptr) {
  ParseErrorCollector collector;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "#include \"tracer/callback.hpp\"\n#include \"context_facade.hpp\"\n" +
          source,
      clang_driver_arguments(arguments), file_name, "clang-context-activation",
      std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripDependencyFileAdjuster(), {}, &collector);
  if (errors) {
    *errors = collector.take();
  }
  return ast;
}

// Appends the collected Clang errors to a parse failure summary.
std::string parse_failure(std::string summary,
                          const std::vector<ContextSourceError> &errors) {
  if (!errors.empty()) {
    summary += "\n" + error_lines(errors);
  }
  return summary;
}

std::string error_text(llvm::Error error) {
  return llvm::toString(std::move(error));
}

bool is_verified_native_record(clang::ASTContext &context,
                               const clang::CXXRecordDecl *declaration,
                               clang::SourceLocation location) {
  const auto filename = context.getSourceManager().getFilename(location);
  if (filename.empty()) {
    return false;
  }
  llvm::SmallString<256> physical;
  if (llvm::sys::fs::real_path(filename, physical)) {
    physical = filename;
  }
  const auto under = [&](llvm::StringRef root) {
    llvm::SmallString<256> canonical_root;
    if (llvm::sys::fs::real_path(root, canonical_root)) {
      return false;
    }
    return ::tracer::compat::starts_with(llvm::StringRef(physical),
                                         llvm::StringRef(canonical_root).str() + "/");
  };
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.FullyQualifiedName = true;
  policy.PrintAsCanonical = true;
#if CLANG_VERSION_MAJOR >= 22
  const auto canonical =
      clang::QualType(context.getCanonicalTagType(declaration).getTypePtr(), 0)
          .getAsString(policy);
#else
  const auto canonical = clang::QualType(declaration->getTypeForDecl(), 0)
                             .getCanonicalType()
                             .getAsString(policy);
#endif
  if (::tracer::compat::starts_with(canonical, "std::")) {
    llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
    while (!roots.empty()) {
      const auto pair = roots.split('|');
      roots = pair.second;
      if (!pair.first.empty() && under(pair.first)) {
        return true;
      }
    }
  }
  llvm::SmallString<256> engine_root;
  return !llvm::sys::fs::real_path(TRACER_CONTEXT_ENGINE_ROOT, engine_root) &&
         ::tracer::compat::starts_with(llvm::StringRef(physical),
                                       llvm::StringRef(engine_root).str() + "/") &&
         ::tracer::compat::starts_with(canonical, "tracer::context_engine::");
}

std::vector<std::string>
activation_arguments(const std::vector<std::string> &arguments) {
  auto result = arguments;
  // The native owner ABI is tied to the compiler-selected library below.
  // Otherwise a host GCC installation can supply newer optional headers
  // missing from that library, mixing incompatible standard-library versions.
  result.emplace_back("-nostdinc++");
  const auto headers = context_header_directory();
  result.emplace_back("-I" + headers);
  // Build-tree public headers are beside src; installed copies are nested in
  // the private compiler-header directory itself.
  result.emplace_back("-I" + headers + "/../include");
  llvm::StringRef roots(TRACER_CONTEXT_SYSTEM_ROOTS);
  while (!roots.empty()) {
    const auto pair = roots.split('|');
    roots = pair.second;
    if (!pair.first.empty()) {
      result.emplace_back("-isystem");
      result.push_back(pair.first.str());
    }
  }
  return result;
}

std::string expected_error(llvm::Expected<ContextSymbol> &value) {
  return error_text(value.takeError());
}

std::string ir_symbol(const ContextCompilationContract &contract,
                      const std::string &source_name) {
  const auto bridge =
      std::find_if(contract.bridges.begin(), contract.bridges.end(),
                   [&](const auto &candidate) {
                     return candidate.source_name == source_name;
                   });
  return bridge == contract.bridges.end() ? source_name : bridge->symbol;
}

const ContextCompilationContract::Bridge *
find_bridge(const ContextCompilationContract &contract,
            std::string_view source_name) {
  const auto found = std::find_if(
      contract.bridges.begin(), contract.bridges.end(),
      [&](const auto &bridge) { return bridge.source_name == source_name; });
  return found == contract.bridges.end() ? nullptr : &*found;
}

std::string lifecycle_closure_fingerprint(
    llvm::Module &module,
    const std::array<std::pair<std::string, std::string>, 4> &roots) {
  std::set<const llvm::Function *> functions;
  std::set<const llvm::Function *> processed_functions;
  std::set<const llvm::GlobalVariable *> globals;
  std::vector<const llvm::GlobalVariable *> discovered_globals;
  std::vector<const llvm::Function *> pending;
  std::map<const llvm::Function *, std::string> root_roles;
  for (const auto &[symbol, role] : roots) {
    auto *function = module.getFunction(symbol);
    if (!function) {
      return {};
    }
    root_roles.emplace(function, role);
    pending.push_back(function);
  }

  std::set<const llvm::Value *> visited_constants;
  std::function<void(const llvm::Value *)> visit_value;
  visit_value = [&](const llvm::Value *value) {
    if (!value || !visited_constants.insert(value).second) {
      return;
    }
    if (const auto *function = llvm::dyn_cast<llvm::Function>(value)) {
      if (functions.insert(function).second && !function->isDeclaration()) {
        pending.push_back(function);
      }
      return;
    }
    if (const auto *global = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
      if (globals.insert(global).second) {
        discovered_globals.push_back(global);
        if (global->hasInitializer()) {
          visit_value(global->getInitializer());
        }
      }
      return;
    }
    if (const auto *alias = llvm::dyn_cast<llvm::GlobalAlias>(value)) {
      visit_value(alias->getAliasee());
      return;
    }
    if (const auto *constant = llvm::dyn_cast<llvm::Constant>(value)) {
      for (const auto &operand : constant->operands()) {
        visit_value(operand.get());
      }
    }
  };

  // Preserve declarations as dependencies in function IR, but only walk
  // definitions reachable from the four owner entrypoints.
  while (!pending.empty()) {
    const auto *function = pending.back();
    pending.pop_back();
    functions.insert(function);
    if (!processed_functions.insert(function).second) {
      continue;
    }
    for (const auto &instruction : llvm::instructions(function)) {
      for (const auto &operand : instruction.operands()) {
        visit_value(operand.get());
      }
    }
  }

  std::vector<const llvm::Function *> ordered_functions(functions.begin(),
                                                        functions.end());
  const auto stable_function_name = [](llvm::StringRef name) {
    std::string normalized = name.str();
    const auto marker = normalized.find("clangtracer_owner_");
    if (marker != std::string::npos) {
      auto digit = marker + std::string_view("clangtracer_owner_").size();
      auto end = digit;
      while (end < normalized.size() &&
             std::isdigit(static_cast<unsigned char>(normalized[end]))) {
        ++end;
      }
      if (end > digit && end < normalized.size() && normalized[end] == '_') {
        normalized.replace(digit, end - digit, "#");
      }
    }
    return normalized;
  };
  std::sort(
      ordered_functions.begin(), ordered_functions.end(),
      [&](const auto *left, const auto *right) {
        const auto left_root = root_roles.find(left);
        const auto right_root = root_roles.find(right);
        if (left_root != root_roles.end() || right_root != root_roles.end()) {
          if (left_root == root_roles.end())
            return false;
          if (right_root == root_roles.end())
            return true;
          return left_root->second < right_root->second;
        }
        const auto left_name = stable_function_name(left->getName());
        const auto right_name = stable_function_name(right->getName());
        return left_name == right_name ? left->getName() < right->getName()
                                       : left_name < right_name;
      });
  const auto &ordered_globals = discovered_globals;

  // CloneModule shares identified StructTypes through the source LLVMContext.
  // Renaming those types would mutate the module that is subsequently loaded
  // and can change its byval/sret ABI names. A bitcode round-trip gives the
  // fingerprint clone an independent context before canonicalization.
  llvm::SmallVector<char, 0> bitcode;
  llvm::raw_svector_ostream bitcode_stream(bitcode);
  llvm::WriteBitcodeToFile(module, bitcode_stream);
  auto fingerprint_context = std::make_unique<llvm::LLVMContext>();
  auto parsed_copy = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(llvm::StringRef(bitcode.data(), bitcode.size()),
                            "clangtracer-owner-lifecycle-fingerprint"),
      *fingerprint_context);
  if (!parsed_copy) {
    llvm::consumeError(parsed_copy.takeError());
    return {};
  }
  auto copy = std::move(*parsed_copy);
  copy->setModuleIdentifier("");
  copy->setSourceFileName("");
  llvm::StripDebugInfo(*copy);
  const auto base_type_name = [](llvm::StringRef raw_name) {
    std::string name = raw_name.str();
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < name.size() &&
        std::all_of(name.begin() + static_cast<std::ptrdiff_t>(dot + 1),
                    name.end(),
                    [](unsigned char ch) { return std::isdigit(ch); })) {
      name.resize(dot);
    }
    return name;
  };
  std::function<std::string(const llvm::Type *, unsigned)> canonical_type;
  canonical_type = [&](const llvm::Type *type, unsigned depth) -> std::string {
    if (!type || depth > 32) {
      return "type-depth-limit";
    }
    if (const auto *structure = llvm::dyn_cast<llvm::StructType>(type)) {
      std::string key = "struct:" + (structure->hasName()
                                         ? base_type_name(structure->getName())
                                         : std::string("anonymous"));
      key += structure->isPacked() ? ":packed{" : ":unpacked{";
      if (!structure->isOpaque()) {
        for (const auto *element : structure->elements()) {
          key += canonical_type(element, depth + 1);
          key.push_back(';');
        }
      } else {
        key += "opaque";
      }
      key.push_back('}');
      return key;
    }
    if (const auto *integer = llvm::dyn_cast<llvm::IntegerType>(type)) {
      return "i" + std::to_string(integer->getBitWidth());
    }
    if (const auto *pointer = llvm::dyn_cast<llvm::PointerType>(type)) {
      return "ptr-as" + std::to_string(pointer->getAddressSpace());
    }
    if (const auto *array = llvm::dyn_cast<llvm::ArrayType>(type)) {
      return "array[" + std::to_string(array->getNumElements()) + "]" +
             canonical_type(array->getElementType(), depth + 1);
    }
    if (const auto *vector = llvm::dyn_cast<llvm::VectorType>(type)) {
      const auto count = vector->getElementCount();
      return std::string(count.isScalable() ? "scalable-vector[" : "vector[") +
             std::to_string(count.getKnownMinValue()) + "]" +
             canonical_type(vector->getElementType(), depth + 1);
    }
    if (const auto *function = llvm::dyn_cast<llvm::FunctionType>(type)) {
      std::string key =
          "function(" + canonical_type(function->getReturnType(), depth + 1);
      for (const auto *parameter : function->params()) {
        key += ";" + canonical_type(parameter, depth + 1);
      }
      key += function->isVarArg() ? ";... )" : ")";
      return key;
    }
    std::string printed;
    llvm::raw_string_ostream type_stream(printed);
    type->print(type_stream);
    type_stream.flush();
    return printed;
  };
  const auto digest_hex = [](llvm::StringRef value) {
    llvm::SHA256 hash;
    hash.update(value);
    const auto digest = hash.final();
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (const auto byte : digest) {
      result.push_back(hex[byte >> 4]);
      result.push_back(hex[byte & 0x0f]);
    }
    return result;
  };
  std::vector<std::string> canonical_type_names;
  const auto identified_types = copy->getIdentifiedStructTypes();
  canonical_type_names.reserve(identified_types.size());
  for (auto *type : identified_types) {
    if (!type->hasName()) {
      continue;
    }
    const auto name = "__context_type_" + digest_hex(canonical_type(type, 0));
    canonical_type_names.push_back(name);
    type->setName(name);
  }
  std::map<std::string, const llvm::Function *> selected_functions;
  for (const auto *function : ordered_functions) {
    selected_functions.emplace(function->getName().str(), function);
  }
  std::map<const llvm::Function *, llvm::Function *> cloned_functions;
  for (const auto *original : ordered_functions) {
    if (auto *clone = copy->getFunction(original->getName())) {
      cloned_functions.emplace(original, clone);
    }
  }
  std::map<const llvm::GlobalVariable *, llvm::GlobalVariable *> cloned_globals;
  for (const auto *original : ordered_globals) {
    if (auto *clone = copy->getGlobalVariable(original->getName(), true)) {
      cloned_globals.emplace(original, clone);
    }
  }
  for (auto &function : *copy) {
    const auto original = selected_functions.find(function.getName().str());
    if (original == selected_functions.end()) {
      continue;
    }
    const auto root = root_roles.find(original->second);
    if (root != root_roles.end()) {
      function.setName("__context_root_" + root->second);
    }
  }
  unsigned helper_index = 0;
  for (const auto *original : ordered_functions) {
    if (::tracer::compat::contains(root_roles, original)) {
      continue;
    }
    auto *function = cloned_functions[original];
    if (function && !function->isDeclaration() &&
        (::tracer::compat::contains(function->getName(), "clangtracer_owner_") ||
         function->hasLocalLinkage())) {
      function->setName("__context_helper_" + std::to_string(helper_index++));
    }
  }
  unsigned global_index = 0;
  for (const auto *original : ordered_globals) {
    if (auto *global = cloned_globals[original]) {
      global->setName("__context_global_" + std::to_string(global_index++));
    }
  }

  std::string normalized;
  llvm::raw_string_ostream stream(normalized);
  stream << "clangtracer.owner.lifecycle.v2\n";
  const auto emit_attributes = [&](const llvm::AttributeList& attributes, unsigned argument_count) {
    stream << " ret{" << attributes.getRetAttrs().getAsString() << "}"
           << " fn{" << attributes.getFnAttrs().getAsString() << "}";
    for (unsigned index = 0; index < argument_count; ++index) {
      stream << " arg" << index << '{' << attributes.getParamAttrs(index).getAsString() << '}';
    }
  };
  const auto normalize_attribute_group_ids = [](llvm::StringRef text) {
    std::string result;
    result.reserve(text.size());
    bool quoted = false;
    bool escaped = false;
    bool comment = false;
    for (std::size_t index = 0; index < text.size();) {
      const char ch = text[index];
      if (comment) {
        result.push_back(ch);
        ++index;
        if (ch == '\n') {
          comment = false;
        }
      } else if (quoted) {
        result.push_back(ch);
        ++index;
        if (escaped) {
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == '"') {
          quoted = false;
        }
      } else if (ch == '"') {
        quoted = true;
        result.push_back(ch);
        ++index;
      } else if (ch == ';') {
        comment = true;
        result.push_back(ch);
        ++index;
      } else if (ch == '#' && index + 1 < text.size() &&
                 std::isdigit(static_cast<unsigned char>(text[index + 1]))) {
        // LLVM assigns these group IDs at module scope, so unrelated IR can
        // renumber them. The complete attribute contents are emitted below.
        result += "#attribute-group";
        index += 2;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index]))) {
          ++index;
        }
      } else {
        result.push_back(text[index++]);
      }
    }
    return result;
  };
  for (const auto *original : ordered_functions) {
    auto *function = cloned_functions[original];
    if (!function) {
      return {};
    }
    if (function->isDeclaration()) {
      stream << "dependency ";
      stream << canonical_type(function->getFunctionType(), 0);
      stream << " cc=" << function->getCallingConv() << ' ' << function->getName();
      emit_attributes(function->getAttributes(), function->arg_size());
      stream << '\n';
      continue;
    }
    for (auto &argument : function->args())
      argument.setName("");
    for (auto &block : *function) {
      block.setName("");
      for (auto &instruction : block)
        instruction.setName("");
    }
    std::string printed_function;
    llvm::raw_string_ostream function_stream(printed_function);
    function->print(function_stream);
    function_stream.flush();
    stream << "function " << normalize_attribute_group_ids(printed_function) << '\n';
    emit_attributes(function->getAttributes(), function->arg_size());
    stream << '\n';
    unsigned call_index = 0;
    for (const auto& instruction : llvm::instructions(function)) {
      if (const auto* call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
        stream << "call-attributes[" << call_index++ << ']';
        emit_attributes(call->getAttributes(), call->arg_size());
        stream << '\n';
      }
    }
  }
  for (const auto *original : ordered_globals) {
    const auto *global = cloned_globals[original];
    if (!global) {
      return {};
    }
    stream << "global ";
    global->getValueType()->print(stream);
    stream << ' ' << global->getName() << " = ";
    if (global->hasInitializer()) {
      global->getInitializer()->printAsOperand(stream, false);
    } else {
      stream << "declaration";
    }
    stream << '\n';
  }
  stream.flush();
  // LLVM uniquifies two identified structs with the same canonical name by
  // appending a numeric suffix. Such aliases are structurally identical and
  // their numbering can depend on unrelated template instantiations in the
  // callback translation unit, so erase only those suffixes on our private
  // canonical type namespace before hashing.
  for (const auto &name : canonical_type_names) {
    const auto marker = name + ".";
    std::size_t position = 0;
    while ((position = normalized.find(marker, position)) !=
           std::string::npos) {
      auto end = position + marker.size();
      while (end < normalized.size() &&
             std::isdigit(static_cast<unsigned char>(normalized[end]))) {
        ++end;
      }
      if (end > position + marker.size()) {
        normalized.erase(position + name.size(), end - position - name.size());
        position += name.size();
      } else {
        position = end;
      }
    }
  }
  llvm::SHA256 hash;
  hash.update(normalized);
  const auto digest = hash.final();
  static constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(digest.size() * 2);
  for (const auto byte : digest) {
    result.push_back(hex[byte >> 4]);
    result.push_back(hex[byte & 0x0f]);
  }
  return result;
}

bool native_layout_matches(const ContextCompilationContract::Bridge &bridge,
                           std::string_view canonical_suffix, std::size_t bytes,
                           std::size_t alignment) {
  const auto found = std::find_if(
      bridge.records.begin(), bridge.records.end(), [&](const auto &record) {
        auto name = record.name;
        for (const auto prefix :
             {std::string_view("std::__1::"), std::string_view("std::__2::"),
              std::string_view("std::__cxx11::")}) {
          const auto position = name.find(prefix);
          if (position != std::string::npos) {
            name.replace(position, prefix.size(), "std::");
          }
        }
        return name.size() >= canonical_suffix.size() &&
               name.compare(name.size() - canonical_suffix.size(),
                            canonical_suffix.size(), canonical_suffix) == 0;
      });
  return found != bridge.records.end() && found->bytes == bytes &&
         found->alignment == alignment;
}

llvm::Expected<ContextCompilationContract>
native_owner_abi_contract(const GeneratedContextOwner &owner,
                          const std::vector<std::string> &arguments,
                          const std::string &file_name) {
  std::string source = "#include \"context_engine.hpp\"\n"
                       "#include \"context_codec.hpp\"\n"
                       "#include <any>\n";
  for (const auto &state : owner.states) {
    source += "::tracer::context_engine::ErrorCode " + state.construct +
              "(const ::tracer::context_engine::ScopeInit&, std::any&) "
              "noexcept { return ::tracer::context_engine::ErrorCode::ok; }\n";
    source += "void " + state.destroy + "(std::any&) noexcept {}\n";
    source +=
        "void* " + state.address + "(std::any&) noexcept { return nullptr; }\n";
    source +=
        "bool " + state.serialize +
        "(::tracer::context_engine::BoundedProtoWriter&, const void*, "
        "::tracer::context_engine::EncodeBudget&) noexcept { return false; }\n";
  }
  clang::IgnoringDiagConsumer ignored;
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      source, clang_driver_arguments(arguments), file_name,
      "clang-context-native-owner-abi",
      std::make_shared<clang::PCHContainerOperations>(),
      clang::tooling::getClangStripDependencyFileAdjuster(), {}, &ignored);
  if (!ast || ast->getDiagnostics().hasErrorOccurred()) {
    return llvm::createStringError(
        llvm::inconvertibleErrorCode(),
        "native owner ABI declarations failed to parse");
  }
  return context_compilation_contract(*ast, {});
}

bool validate_owner_bridge_abi(const ContextCompilationContract &owner,
                               const ContextCompilationContract &native,
                               const GeneratedContextOwner &generated,
                               std::string &error) {
  const auto validate =
      [&](const std::string &source_name, std::string_view role,
          std::initializer_list<std::string_view> roots) -> bool {
    const auto *actual = find_bridge(owner, source_name);
    const auto *expected = find_bridge(native, source_name);
    const auto summarize = [](const ContextTypeLayout &layout) {
      return layout.name + "[size=" + std::to_string(layout.bytes) +
             ",align=" + std::to_string(layout.alignment) +
             ",definition-bytes=" + std::to_string(layout.definition.size()) +
             "]";
    };
    if (!actual || !expected) {
      error = "unsupported_context_activation: native owner bridge " +
              std::string(role) + " is " +
              (actual ? "missing from native ABI probe"
                      : "missing from owner contract");
      return false;
    }
    if (actual->type != expected->type) {
      error = "unsupported_context_activation: native owner bridge signature " +
              std::string(role) + " differs (owner='" + actual->type +
              "', native='" + expected->type + "')";
      return false;
    }
    if (actual->records != expected->records) {
      if (actual->records.size() != expected->records.size()) {
        error = "unsupported_context_activation: native owner bridge layout " +
                std::string(role) + " has " +
                std::to_string(actual->records.size()) +
                " owner records, native ABI probe has " +
                std::to_string(expected->records.size());
      } else {
        for (std::size_t i = 0; i < actual->records.size(); ++i) {
          const auto &left = actual->records[i];
          const auto &right = expected->records[i];
          if (left == right)
            continue;
          error =
              "unsupported_context_activation: native owner bridge layout " +
              std::string(role) + " differs: owner=" + summarize(left) +
              ", native=" + summarize(right);
          break;
        }
      }
      return false;
    }
    for (const auto root : roots) {
      std::size_t bytes = 0, alignment = 0;
      if (root == "tracer::context_engine::ScopeInit") {
        bytes = sizeof(context_engine::ScopeInit);
        alignment = alignof(context_engine::ScopeInit);
      } else if (root == "std::any") {
        bytes = sizeof(std::any);
        alignment = alignof(std::any);
      } else if (root == "tracer::context_engine::BoundedProtoWriter") {
        bytes = sizeof(context_engine::BoundedProtoWriter);
        alignment = alignof(context_engine::BoundedProtoWriter);
      } else if (root == "tracer::context_engine::EncodeBudget") {
        bytes = sizeof(context_engine::EncodeBudget);
        alignment = alignof(context_engine::EncodeBudget);
      }
      if (!native_layout_matches(*expected, root, bytes, alignment)) {
        error = "unsupported_context_activation: native compiler layout "
                "does not match engine ABI for " +
                std::string(root);
        return false;
      }
    }
    return true;
  };
  for (const auto &state : generated.states) {
    if (!validate(state.construct, "construct",
                  {"tracer::context_engine::ScopeInit", "std::any"}) ||
        !validate(state.destroy, "destroy", {"std::any"}) ||
        !validate(state.address, "address", {"std::any"}) ||
        !validate(state.serialize, "serialize",
                  {"tracer::context_engine::BoundedProtoWriter",
                   "tracer::context_engine::EncodeBudget"})) {
      return false;
    }
  }
  return true;
}

const ContextClassPlan *select_class(const ContextDeclarations &declarations,
                                     const std::string &requested,
                                     std::string &error) {
  std::vector<const ContextClassPlan *> candidates;
  for (const auto &candidate : declarations.classes) {
    if (requested.empty() || candidate.type == requested) {
      candidates.push_back(&candidate);
    }
  }
  if (candidates.empty()) {
    error = "invalid_context_activation: selected interceptor was not found";
    return nullptr;
  }
  if (candidates.size() != 1) {
    error = "invalid_context_activation: interceptor selection is ambiguous";
    return nullptr;
  }
  return candidates.front();
}

std::vector<std::string> state_types(const ContextDeclarations &declarations) {
  std::vector<std::string> result;
  result.reserve(declarations.states.size());
  for (const auto &state : declarations.states) {
    result.push_back(state.generic ? state.payload_type : state.state_type);
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

struct ResolvedFamilyLineage {
  ContextDependency dependency;
  std::array<std::uint8_t, 16> state{};
  std::array<std::uint8_t, 16> scope{};
  std::vector<ContextTypeProvenance> payload_provenance;
};

std::array<std::uint8_t, 16>
resolve_record_lineage(clang::ASTContext &context,
                       const clang::CXXRecordDecl *declaration,
                       const std::array<std::uint8_t, 16> &fallback,
                       std::vector<std::string> &diagnostics) {
  if (declaration->getDefinition()) {
    declaration = declaration->getDefinition();
  }
  const auto &source_manager = context.getSourceManager();
  const auto location =
      source_manager.getSpellingLoc(declaration->getLocation());
  if (location.isInvalid()) {
    auto unverified = context_package::PackageCatalog::distinct_unverified(
        "imported type source location is unavailable");
    if (diagnostics.size() < 16) {
      diagnostics.push_back(unverified.diagnostic);
    }
    return unverified.id;
  }
  if (source_manager.isWrittenInMainFile(location)) {
    return fallback;
  }
  const auto filename = source_manager.getFilename(location);
  if (filename.empty()) {
    auto unverified = context_package::PackageCatalog::distinct_unverified(
        "imported type source path is unavailable");
    if (diagnostics.size() < 16) {
      diagnostics.push_back(unverified.diagnostic);
    }
    return unverified.id;
  }

  // `isInSystemHeader` reflects how a header was included, not who owns its
  // declarations: project headers are often deliberately passed with
  // -isystem. Only physical compiler/engine headers receive fallback lineage.
  if (is_verified_native_record(context, declaration, location)) {
    return fallback;
  }

  auto lineage = context_package::PackageCatalog::admit_source(
      std::filesystem::path(filename.str()));
  const bool empty = std::all_of(lineage.id.begin(), lineage.id.end(),
                                 [](std::uint8_t byte) { return byte == 0; });
  if (empty) {
    lineage = context_package::PackageCatalog::distinct_unverified(
        "imported type package lineage could not be resolved");
  }
  if (!lineage.diagnostic.empty() && diagnostics.size() < 16) {
    std::string message =
        lineage.package_root.empty()
            ? lineage.diagnostic
            : lineage.package_root + ": " + lineage.diagnostic;
    if (message.size() > 256) {
      message.resize(256);
    }
    diagnostics.push_back(std::move(message));
  }
  return lineage.id;
}

std::array<std::uint8_t, 16>
resolve_type_lineage(clang::ASTContext &context, std::string_view type,
                     const std::array<std::uint8_t, 16> &fallback,
                     std::vector<std::string> &diagnostics) {
  RecordFinder finder{std::string(type)};
  finder.TraverseDecl(context.getTranslationUnitDecl());
  return finder.found ? resolve_record_lineage(context, finder.found, fallback,
                                               diagnostics)
                      : fallback;
}

struct PayloadProvenance final {
  std::array<std::uint8_t, 16> primary_lineage{};
  std::vector<ContextTypeProvenance> records;
  std::string error;
};

class PayloadProvenanceWalker final {
public:
  PayloadProvenanceWalker(clang::ASTContext &context,
                          const std::array<std::uint8_t, 16> &fallback,
                          std::vector<std::string> &diagnostics)
      : context_(context), fallback_(fallback), diagnostics_(diagnostics),
        policy_(context.getLangOpts()) {
    policy_.FullyQualifiedName = true;
    policy_.PrintAsCanonical = true;
  }

  PayloadProvenance collect(clang::QualType type) {
    if (!visit(type, 0)) {
      return {{},
              {},
              "invalid_context_activation: generic payload provenance "
              "traversal exceeded its bounded type graph"};
    }
    std::sort(records_.begin(), records_.end(),
              [](const auto &a, const auto &b) {
                return a.canonical_type < b.canonical_type;
              });
    return {primary_lineage_, std::move(records_), {}};
  }

private:
  bool visit(clang::QualType type, unsigned depth) {
    if (type.isNull() || depth > 32 || seen_.size() > 1024) {
      return depth <= 32 && seen_.size() <= 1024;
    }
    type = context_.getCanonicalType(type).getUnqualifiedType();
    if (const auto *array = context_.getAsArrayType(type)) {
      return visit(array->getElementType(), depth + 1);
    }
    if (type->isPointerType() || type->isReferenceType()) {
      return visit(type->getPointeeType(), depth + 1);
    }
    const auto *record = type->getAsCXXRecordDecl();
    if (!record) {
      return true;
    }
    const auto name = type.getAsString(policy_);
    if (!seen_.insert(name).second) {
      return true;
    }
    if (seen_.size() > 1024) {
      return false;
    }

    const auto *definition = record->getDefinition();
    const auto *source_decl = definition ? definition : record;
    const auto location =
        context_.getSourceManager().getSpellingLoc(source_decl->getLocation());
    const bool is_native =
        location.isValid() &&
        is_verified_native_record(context_, source_decl, location);
    if (!is_native) {
      const auto lineage = resolve_record_lineage(context_, source_decl,
                                                  fallback_, diagnostics_);
      if (!has_primary_lineage_) {
        primary_lineage_ = lineage;
        has_primary_lineage_ = true;
      }
      records_.push_back({name, lineage});
      if (records_.size() > 256) {
        return false;
      }
    }

    if (const auto *specialization =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                source_decl)) {
      for (const auto &argument : specialization->getTemplateArgs().asArray()) {
        if (argument.getKind() == clang::TemplateArgument::Type &&
            !visit(argument.getAsType(), depth + 1)) {
          return false;
        }
      }
    }
    // Standard-library implementation fields are intentionally opaque. Their
    // user type arguments are visited above; user records recurse through
    // their actual field and base declarations.
    if (!is_native && definition) {
      for (const auto *field : definition->fields()) {
        if (!visit(field->getType(), depth + 1)) {
          return false;
        }
      }
      for (const auto &base : definition->bases()) {
        if (!visit(base.getType(), depth + 1)) {
          return false;
        }
      }
    }
    return true;
  }

  clang::ASTContext &context_;
  const std::array<std::uint8_t, 16> &fallback_;
  std::vector<std::string> &diagnostics_;
  clang::PrintingPolicy policy_;
  std::set<std::string> seen_;
  std::vector<ContextTypeProvenance> records_;
  std::array<std::uint8_t, 16> primary_lineage_{};
  bool has_primary_lineage_ = false;
};

std::unique_ptr<clang::ASTUnit>
parse_payload_alias(clang::ASTUnit &unit, const std::string &payload_type,
                    const std::vector<std::string> &arguments,
                    const std::string &file_name, std::string &error) {
  const auto alias_source =
      unit.getSourceManager()
          .getBufferData(unit.getSourceManager().getMainFileID())
          .str() +
      "\nusing __clangtracer_context_payload_type = " + payload_type + ";\n";
  auto alias_ast =
      parse_source(alias_source, arguments, file_name + ".payload.cc");
  if (!alias_ast || alias_ast->getDiagnostics().hasErrorOccurred()) {
    error = "invalid_context_schema: generic payload type cannot be "
            "reconstructed: " +
            payload_type;
    return {};
  }
  return alias_ast;
}

std::vector<ResolvedFamilyLineage> resolve_family_lineages(
    clang::ASTUnit &unit, const ContextDeclarations &declarations,
    const std::array<std::uint8_t, 16> &fallback,
    const std::vector<std::string> &arguments, const std::string &file_name,
    std::vector<std::string> &diagnostics, std::string &error) {
  auto &context = unit.getASTContext();
  std::vector<ResolvedFamilyLineage> result;
  result.reserve(declarations.states.size());
  for (const auto &dependency : declarations.states) {
    const auto scope_lineage = resolve_type_lineage(
        context, dependency.scope_type, fallback, diagnostics);
    std::array<std::uint8_t, 16> state_lineage{};
    std::vector<ContextTypeProvenance> provenance;
    if (dependency.generic) {
      auto alias_ast = parse_payload_alias(unit, dependency.payload_type,
                                           arguments, file_name, error);
      if (!alias_ast) {
        return {};
      }
      AliasFinder finder;
      finder.TraverseDecl(alias_ast->getASTContext().getTranslationUnitDecl());
      if (!finder.found) {
        error = "invalid_context_schema: generic payload alias is absent";
        return {};
      }
      auto summary = PayloadProvenanceWalker(alias_ast->getASTContext(),
                                             fallback, diagnostics)
                         .collect(finder.found->getUnderlyingType());
      if (!summary.error.empty()) {
        error = std::move(summary.error);
        return {};
      }
      provenance = std::move(summary.records);
      state_lineage =
          provenance.empty() ? scope_lineage : summary.primary_lineage;
    } else {
      state_lineage = resolve_type_lineage(context, dependency.state_type,
                                           fallback, diagnostics);
    }
    result.push_back(
        {dependency, state_lineage, scope_lineage, std::move(provenance)});
  }
  return result;
}

const ResolvedFamilyLineage *
find_family_lineage(const std::vector<ResolvedFamilyLineage> &lineages,
                    const ContextDependency &dependency) {
  const auto found =
      std::find_if(lineages.begin(), lineages.end(), [&](const auto &lineage) {
        return lineage.dependency.state_type == dependency.state_type &&
               lineage.dependency.scope_type == dependency.scope_type &&
               lineage.dependency.shared == dependency.shared &&
               lineage.dependency.generic == dependency.generic &&
               lineage.dependency.payload_type == dependency.payload_type;
      });
  return found == lineages.end() ? nullptr : &*found;
}

bool add_schemas(clang::ASTUnit &unit, const ContextDeclarations &declarations,
                 const std::vector<ResolvedFamilyLineage> &lineages,
                 const std::vector<std::string> &arguments,
                 const std::string &file_name,
                 std::vector<std::shared_ptr<const SchemaRecord>> &schemas,
                 std::string &error) {
  for (const auto &dependency : declarations.states) {
    const auto *lineage = find_family_lineage(lineages, dependency);
    if (!lineage) {
      error = "invalid_context_schema: state family lineage is absent";
      return false;
    }
    StateFamilyKey family{lineage->state,
                          dependency.generic ? dependency.payload_type
                                             : dependency.state_type,
                          dependency.scope_type};
    SchemaResult built;
    if (!dependency.generic) {
      RecordFinder finder(dependency.state_type);
      finder.TraverseDecl(unit.getASTContext().getTranslationUnitDecl());
      if (!finder.found) {
        error = "invalid_context_schema: state declaration is absent: " +
                dependency.state_type;
        return false;
      }
      built = build_state_schema(unit.getASTContext(), *finder.found,
                                 std::move(family));
    } else {
      const std::string alias_source =
          unit.getSourceManager()
              .getBufferData(unit.getSourceManager().getMainFileID())
              .str() +
          "\nusing __clangtracer_context_payload_type = " +
          dependency.payload_type + ";\n";
      auto alias_ast =
          parse_source(alias_source, arguments, file_name + ".payload.cc");
      if (!alias_ast || alias_ast->getDiagnostics().hasErrorOccurred()) {
        error = "invalid_context_schema: generic payload type cannot be "
                "reconstructed: " +
                dependency.payload_type;
        return false;
      }
      AliasFinder finder;
      finder.TraverseDecl(alias_ast->getASTContext().getTranslationUnitDecl());
      if (!finder.found) {
        error = "invalid_context_schema: generic payload alias is absent";
        return false;
      }
      built = build_payload_schema(alias_ast->getASTContext(),
                                   finder.found->getUnderlyingType(),
                                   std::move(family));
    }
    if (!built) {
      error = built.error;
      return false;
    }
    schemas.push_back(std::move(built.record));
  }
  return true;
}

std::vector<context_facade::FacadeIdentity>
facade_identities(const ContextDeclarations &declarations,
                  const std::vector<ResolvedFamilyLineage> &lineages) {
  std::vector<context_facade::FacadeIdentity> result;
  result.reserve(declarations.states.size());
  for (const auto &dependency : declarations.states) {
    const auto type =
        dependency.generic ? dependency.payload_type : dependency.state_type;
    const auto *lineage = find_family_lineage(lineages, dependency);
    if (!lineage) {
      continue;
    }
    result.push_back(
        {dependency,
         context_state_identity(lineage->state, dependency.scope_type, type),
         context_scope_identity(lineage->scope, dependency.scope_type)});
  }
  return result;
}

std::string wrapper_source(const std::string &class_name,
                           const ContextClassPlan &plan) {
  std::string source = R"(
#include <new>
extern "C" unsigned long clangtracer_owner_anchor_v1() noexcept;
extern "C" bool clangtracer_context_construct(void* storage,
    const ::tracer::TraceContext* raw) {
  (void)raw;
  if (!storage || clangtracer_owner_anchor_v1() != 1) return false;
  try {
    ::new (storage) ::)" +
                       class_name + "(";
  bool first = true;
  for (const auto &dependency : plan.dependencies) {
    if (!first) {
      source += ", ";
    }
    first = false;
    source += "::tracer::State<::" + dependency.state_type + ">{}";
  }
  source += R"() ;
    return true;
  } catch (...) { return false; }
}
extern "C" bool clangtracer_context_pre(void* storage,
    const ::tracer::TraceContext* raw) {
  if (!storage || !raw || clangtracer_owner_anchor_v1() != 1) return false;
  try { ::tracer::Call call(*raw); static_cast<::)" +
            class_name +
            "*>(storage)->__tracer_invoke_pre(call); return true; } catch "
            "(...) { return false; }\n}\n";
  const bool has_pre =
      std::any_of(plan.hooks.begin(), plan.hooks.end(),
                  [](const auto &hook) { return hook.method == "pre"; });
  const bool has_post =
      std::any_of(plan.hooks.begin(), plan.hooks.end(),
                  [](const auto &hook) { return hook.method == "post"; });
  if (!has_pre) {
    const auto start = source.find("  try { ::tracer::Call call(*raw);");
    const auto end = source.find("\n}\n", start);
    if (start != std::string::npos && end != std::string::npos) {
      source.replace(
          start, end - start,
          "  return storage && raw && clangtracer_owner_anchor_v1() == 1;");
    }
  }
  if (has_post) {
    source += R"(
extern "C" bool clangtracer_context_post(void* storage,
    const ::tracer::TraceContext* raw) {
  if (!storage || !raw || clangtracer_owner_anchor_v1() != 1) return false;
  try { ::tracer::Call call(*raw); static_cast<::)" +
              class_name +
              "*>(storage)->__tracer_invoke_post(call); return true; } catch "
              "(...) { return false; }\n}\n";
  } else {
    source += "extern \"C\" bool clangtracer_context_post(void* storage, const "
              "::tracer::TraceContext* raw) { return storage && raw "
              "&& clangtracer_owner_anchor_v1() == 1; }\n";
  }
  source += "extern \"C\" void clangtracer_context_destroy(void* storage) "
            "{ using InterceptorType = ::" + class_name +
            "; if (storage) static_cast<InterceptorType*>(storage)"
            "->~InterceptorType(); }\n";
  return source;
}

std::vector<ContextNativeAddress>
native_addresses(decltype(&detail::tracer_enqueue_log) callback_log) {
  using namespace context_facade;
  if (!callback_log) {
    callback_log = [](const TraceContext *, const char *, std::size_t,
                      LogLevel) noexcept { return false; };
  }
  return {{"::tracer::context_facade::active_generation",
           reinterpret_cast<std::uintptr_t>(&active_generation)},
          {"::tracer::context_facade::current_context",
           reinterpret_cast<std::uintptr_t>(&current_context)},
          {"::tracer::context_facade::emit_event",
           reinterpret_cast<std::uintptr_t>(&emit_event)},
          {"::tracer::context_facade::active_thread_id",
           reinterpret_cast<std::uintptr_t>(&active_thread_id)},
          {"::tracer::detail::tracer_enqueue_log",
           reinterpret_cast<std::uintptr_t>(callback_log)}};
}

std::string entry_declarations() {
  return "extern \"C\" bool clangtracer_context_construct(void*, const "
         "::tracer::TraceContext*);\n"
         "extern \"C\" bool clangtracer_context_pre(void*, const "
         "::tracer::TraceContext*);\n"
         "extern \"C\" bool clangtracer_context_post(void*, const "
         "::tracer::TraceContext*);\n"
         "extern \"C\" void clangtracer_context_destroy(void*);\n";
}

bool add_entry_friends(clang::ASTUnit &unit, std::string &source,
                       const std::string &selected_class, std::string &error) {
  RecordFinder finder(selected_class);
  finder.TraverseDecl(unit.getASTContext().getTranslationUnitDecl());
  if (!finder.found) {
    error = "invalid_context_activation: selected interceptor disappeared "
            "during callback lowering";
    return false;
  }
  const auto close = finder.found->getBraceRange().getEnd();
  auto &manager = unit.getSourceManager();
  if (close.isInvalid() || close.isMacroID() ||
      !manager.isWrittenInMainFile(close)) {
    error = "unsupported_context_activation: wrapper friendship requires a "
            "direct class definition";
    return false;
  }
  // parse_source adds its private header prelude. Edit the exact checked
  // buffer so its token offsets still address the selected class.
  source = manager.getBufferData(manager.getMainFileID()).str();
  source.insert(manager.getFileOffset(close),
                "\n  friend bool ::clangtracer_context_pre(void*, const "
                "::tracer::TraceContext*);\n"
                "  friend bool ::clangtracer_context_post(void*, const "
                "::tracer::TraceContext*);\n");
  return true;
}

bool has_generated_macro_conflict(clang::Preprocessor &preprocessor,
                                  std::string_view generated) {
  for (std::size_t offset = 0; offset < generated.size();) {
    const auto identifier = [](unsigned char value) {
      return std::isalnum(value) || value == '_';
    };
    if (!std::isalpha(static_cast<unsigned char>(generated[offset])) &&
        generated[offset] != '_') {
      ++offset;
      continue;
    }
    const auto start = offset++;
    while (offset < generated.size() &&
           identifier(static_cast<unsigned char>(generated[offset]))) {
      ++offset;
    }
    if (preprocessor.isMacroDefined(generated.substr(start, offset - start))) {
      return true;
    }
  }
  return false;
}

} // namespace

ContextActivationGeneration::ContextActivationGeneration(
    std::shared_ptr<const PrivateContextCode> owner,
    std::shared_ptr<const PrivateContextCode> callback,
    std::vector<std::shared_ptr<const RegisteredContextScope>> scopes,
    std::vector<std::shared_ptr<const SchemaRecord>> schemas,
    std::string selected_class, std::size_t object_bytes,
    std::size_t object_alignment, std::uintptr_t construct, std::uintptr_t pre,
    std::uintptr_t post, std::uintptr_t destroy,
    std::vector<std::string> lineage_diagnostics, bool requires_stack,
    bool has_pre, bool has_post)
    : owner_code_(std::move(owner)), callback_code_(std::move(callback)),
      scopes_(std::move(scopes)), schemas_(std::move(schemas)),
      class_(std::move(selected_class)), object_bytes_(object_bytes),
      object_alignment_(object_alignment), construct_(construct), pre_(pre),
      post_(post), destroy_(destroy),
      lineage_diagnostics_(std::move(lineage_diagnostics)),
      requires_stack_(requires_stack), has_pre_(has_pre), has_post_(has_post) {}

ContextActivationResult
prepare_context_source(const ContextActivationRequest &request,
                       ContextRegistry *registry, bool preflight,
                       bool syntax_only) {
  ContextActivationResult result;
  const auto fail = [&](std::string message) {
    result.error = std::move(message);
    return result;
  };
  auto lineage_id = request.source_lineage_id;
  const bool missing_lineage = std::all_of(
      lineage_id.begin(), lineage_id.end(),
      [](std::uint8_t byte) { return byte == 0; });
  if (preflight && missing_lineage) {
    // This identity is used only while validating derived schemas. It is never
    // published to the context registry or retained after preflight returns.
    lineage_id = {0x43, 0x54, 0x50, 0x52, 0x45, 0x46, 0x4c, 0x49,
                  0x47, 0x48, 0x54, 0x31, 0x00, 0x00, 0x00, 0x01};
  }
  if (request.source.empty() || request.compiler_arguments.empty() ||
      request.maximum_object_bytes == 0 ||
      request.maximum_object_alignment == 0 ||
      (!preflight && missing_lineage)) {
    return fail("invalid_context_activation: missing source, compiler profile "
                "or lineage");
  }
  const auto arguments = activation_arguments(request.compiler_arguments);
  constexpr std::string_view reserved[] = {
      "clangtracer_context_construct", "clangtracer_context_pre",
      "clangtracer_context_post", "clangtracer_context_destroy",
      "clangtracer_owner_anchor_v1"};
  for (const auto symbol : reserved) {
    if (request.source.find(symbol) != std::string::npos) {
      return fail("invalid_context_activation: reserved entrypoint name is "
                  "already present: " +
                  std::string(symbol));
    }
  }

  const auto prepared = prepare_context_hook_parse_copy(
      request.source, arguments, request.file_name);
  if (!prepared.error.empty()) {
    return fail(prepared.error);
  }
  std::vector<ContextSourceError> parse_errors;
  auto ast = parse_source(prepared.source, arguments, request.file_name,
                          &parse_errors);
  if (!ast || ast->getDiagnostics().hasErrorOccurred()) {
    if (parse_errors.empty()) {
      return fail("invalid_context_activation: checked source parse failed");
    }
    // This is the callback as written, so Clang's errors are the report.
    result.source_errors = std::move(parse_errors);
    return fail(error_lines(result.source_errors));
  }
  auto declarations = analyze_context_declarations(ast->getASTContext());
  if (!declarations.error.empty()) {
    return fail(declarations.error);
  }
  std::vector<std::string> lineage_diagnostics;
  std::string lineage_error;
  const auto family_lineages = resolve_family_lineages(
      *ast, declarations, lineage_id, arguments,
      request.file_name, lineage_diagnostics, lineage_error);
  if (!lineage_error.empty()) {
    return fail(std::move(lineage_error));
  }
  auto input_copy = "#include \"tracer/callback.hpp\"\n" +
                    entry_declarations() + prepared.source;
  if (auto effect_error = validate_context_support_effects(*ast);
      !effect_error.empty()) {
    return fail(effect_error);
  }
  std::string selection_error;
  const auto *selected =
      select_class(declarations, request.interceptor_class, selection_error);
  if (!selected) {
    return fail(std::move(selection_error));
  }
  RecordFinder selected_record(selected->type);
  selected_record.TraverseDecl(ast->getASTContext().getTranslationUnitDecl());
  if (!selected_record.found) {
    return fail(
        "invalid_context_activation: selected class definition is absent");
  }
  GeneratedContextOwner generated_owner;
  if (declarations.states.empty()) {
    generated_owner.compilation_copy =
        "#include \"tracer/callback.hpp\"\n"
        "#include \"context_facade.hpp\"\n"
        "extern \"C\" unsigned long clangtracer_owner_anchor_v1() noexcept "
        "{ return 1; }\n";
  } else {
    generated_owner = generate_context_owner(*ast, true);
    if (!generated_owner.error.empty()) {
      return fail(generated_owner.error);
    }
  }
  for (auto &owner_state : generated_owner.states) {
    const auto *lineage =
        find_family_lineage(family_lineages, owner_state.type);
    if (!lineage) {
      return fail("invalid_context_activation: generated owner state has no "
                  "source lineage record");
    }
    owner_state.state_lineage_id = lineage->state;
    owner_state.scope_lineage_id = lineage->scope;
    owner_state.payload_provenance = lineage->payload_provenance;
  }
  auto schemas = std::vector<std::shared_ptr<const SchemaRecord>>{};
  if (!add_schemas(*ast, declarations, family_lineages, arguments,
                   request.file_name, schemas, result.error)) {
    return result;
  }
  const auto types = state_types(declarations);
  if (!declarations.states.empty()) {
    generated_owner.compilation_copy +=
        "\nextern \"C\" unsigned long clangtracer_owner_anchor_v1() noexcept { "
        "return 1; }\n";
  }


  std::vector<context_facade::FacadeIdentity> identities;
  for (const auto &identity :
       facade_identities(declarations, family_lineages)) {
    identities.push_back(identity);
  }
  const auto facade = context_facade::generate_context_facade(
      "clangtracer_context_private", "clangtracer_context_bind_v1", identities);
  if (!facade.error.empty()) {
    return fail(facade.error);
  }
  std::string callback_source = input_copy + facade.definitions;
  auto lifecycle_ast = parse_source(callback_source, arguments,
                                    request.file_name + ".lifecycle.cc",
                                    &parse_errors);
  if (!lifecycle_ast || lifecycle_ast->getDiagnostics().hasErrorOccurred()) {
    return fail(parse_failure(
        "invalid_context_activation: facade traits failed to parse",
        parse_errors));
  }
  auto lifecycle =
      generate_context_lifecycle(*lifecycle_ast, "clangtracer_context_private");
  if (!lifecycle.error.empty()) {
    return fail(lifecycle.error);
  }
  auto hooks_ast = parse_source(lifecycle.compilation_copy, arguments,
                                request.file_name + ".hooks.cc", &parse_errors);
  if (!hooks_ast || hooks_ast->getDiagnostics().hasErrorOccurred()) {
    return fail(parse_failure(
        "invalid_context_activation: lifecycle source failed to parse",
        parse_errors));
  }
  auto hooks =
      generate_context_hooks(*hooks_ast, "clangtracer_context_private");
  if (!hooks.error.empty()) {
    return fail(hooks.error);
  }
  auto hooks_source = hooks.compilation_copy;
  auto final_callback_ast =
      parse_source(hooks_source, arguments,
                   request.file_name + ".entrypoints.cc", &parse_errors);
  if (!final_callback_ast ||
      final_callback_ast->getDiagnostics().hasErrorOccurred()) {
    return fail(parse_failure("invalid_context_activation: lowered hook source "
                              "failed to reparse",
                              parse_errors));
  }
  if (!add_entry_friends(*final_callback_ast, hooks_source, selected->type,
                         result.error)) {
    return result;
  }
  RecordFinder final_record(selected->type);
  final_record.TraverseDecl(
      final_callback_ast->getASTContext().getTranslationUnitDecl());
  if (!final_record.found) {
    return fail("invalid_context_activation: lowered interceptor definition "
                "is absent");
  }
#if CLANG_VERSION_MAJOR >= 22
  const auto final_type =
      final_callback_ast->getASTContext().getCanonicalTagType(
          final_record.found);
#else
  const auto final_type =
      clang::QualType(final_record.found->getTypeForDecl(), 0);
#endif
  const auto object_bytes =
      static_cast<std::size_t>(final_callback_ast->getASTContext()
                                   .getTypeSizeInChars(final_type)
                                   .getQuantity());
  const auto object_alignment =
      static_cast<std::size_t>(final_callback_ast->getASTContext()
                                   .getTypeAlignInChars(final_type)
                                   .getQuantity());
  if (!object_bytes || object_bytes > request.maximum_object_bytes ||
      object_alignment > request.maximum_object_alignment) {
    return fail("unsupported_context_activation: lowered interceptor object "
                "layout exceeds native storage limits");
  }
  const auto wrappers = wrapper_source(selected->type, *selected);
  if (has_generated_macro_conflict(hooks_ast->getPreprocessor(), wrappers)) {
    return fail("unsupported_context_activation: active macro changes a "
                "generated entrypoint");
  }
  callback_source = hooks_source + wrappers;

  if (preflight && syntax_only) {
    auto wrapper_ast =
        parse_source(callback_source, arguments,
                     request.file_name + ".preflight.cc", &parse_errors);
    if (!wrapper_ast || wrapper_ast->getDiagnostics().hasErrorOccurred()) {
      return fail(parse_failure(
          "invalid_context_activation: lowered wrapper syntax failed",
          parse_errors));
    }
    result.preflight_success = true;
    return result;
  }

  auto standard_library = ContextStdlibImage::acquire();
  if (!standard_library) {
    return fail("unsupported_context_activation: private standard runtime: " +
                error_text(standard_library.takeError()));
  }
  if (!(*standard_library)->validate_runtime()) {
    return fail("unsupported_context_activation: private standard runtime "
                "failed admission");
  }
  auto owner_module = compile_context_module(generated_owner.compilation_copy,
                                             arguments, types);
  if (!owner_module) {
    return fail("invalid_context_owner: " +
                error_text(owner_module.takeError()));
  }
  if (!generated_owner.states.empty()) {
    auto fingerprint_error = owner_module->module.withModuleDo(
        [&](llvm::Module &module) -> llvm::Error {
          for (auto &state : generated_owner.states) {
            const std::array<std::pair<std::string, std::string>, 4> roots{{
                {ir_symbol(owner_module->contract, state.construct),
                 "construct"},
                {ir_symbol(owner_module->contract, state.destroy), "destroy"},
                {ir_symbol(owner_module->contract, state.address), "address"},
                {ir_symbol(owner_module->contract, state.serialize),
                 "serialize"},
            }};
            state.lifecycle_fingerprint =
                lifecycle_closure_fingerprint(module, roots);
            if (state.lifecycle_fingerprint.empty()) {
              return llvm::createStringError(
                  llvm::inconvertibleErrorCode(),
                  "owner lifecycle entrypoint closure could not be "
                  "fingerprinted");
            }
          }
          return llvm::Error::success();
        });
    if (fingerprint_error) {
      return fail("invalid_context_owner: " +
                  error_text(std::move(fingerprint_error)));
    }
  }
  auto native_owner_abi = native_owner_abi_contract(
      generated_owner, arguments, request.file_name + ".owner-abi.cc");
  if (!native_owner_abi) {
    return fail("unsupported_context_activation: " +
                error_text(native_owner_abi.takeError()));
  }
  if (!validate_owner_bridge_abi(owner_module->contract, *native_owner_abi,
                                 generated_owner, result.error)) {
    return result;
  }
  std::vector<std::string> owner_exports{"clangtracer_owner_anchor_v1"};
  for (const auto &state : generated_owner.states) {
    owner_exports.push_back(ir_symbol(owner_module->contract, state.construct));
    owner_exports.push_back(ir_symbol(owner_module->contract, state.destroy));
    owner_exports.push_back(ir_symbol(owner_module->contract, state.address));
    owner_exports.push_back(ir_symbol(owner_module->contract, state.serialize));
  }
  auto owner_code = PrivateContextCode::load(
      std::move(owner_module->module), std::move(owner_module->contract),
      std::move(owner_exports), {}, PrivateContextCode::Role::owner,
      *standard_library);
  if (!owner_code) {
    return fail("invalid_context_owner: " + error_text(owner_code.takeError()));
  }

  auto native_imports = ContextNativeSymbol::catalog(
      "#include \"context_facade.hpp\"\n", arguments,
      native_addresses(request.callback_log));
  if (!native_imports) {
    return fail("invalid_context_activation: native facade catalog failed: " +
                error_text(native_imports.takeError()));
  }
  const auto load_callback =
      [&](const std::shared_ptr<const PrivateContextCode> &bound_owner)
      -> llvm::Expected<std::shared_ptr<const PrivateContextCode>> {
    auto callback_module =
        compile_context_module(callback_source, arguments, types);
    if (!callback_module) {
      return callback_module.takeError();
    }
    auto anchor = bound_owner->symbol("clangtracer_owner_anchor_v1");
    if (!anchor) {
      return anchor.takeError();
    }
    return PrivateContextCode::load(
        std::move(callback_module->module),
        std::move(callback_module->contract),
        {"clangtracer_context_construct", "clangtracer_context_pre",
         "clangtracer_context_post", "clangtracer_context_destroy"},
        {*anchor}, PrivateContextCode::Role::callback, *standard_library,
        *native_imports);
  };
  auto callback_code = load_callback(*owner_code);
  if (!callback_code) {
    return fail("invalid_context_callback: " +
                error_text(callback_code.takeError()));
  }

  if (preflight) {
    for (const char *name : {"clangtracer_context_construct",
                             "clangtracer_context_pre",
                             "clangtracer_context_post",
                             "clangtracer_context_destroy"}) {
      auto symbol = (*callback_code)->symbol(name);
      if (!symbol) {
        return fail("invalid_context_callback: " +
                    expected_error(symbol));
      }
    }
    result.preflight_success = true;
    return result;
  }

  if (!registry) {
    return fail("invalid_context_activation: missing context registry");
  }

  ContextRegistryResult registered;
  if (!generated_owner.states.empty()) {
    registered = registry->register_owner(lineage_id, *owner_code,
                                         generated_owner, schemas);
    if (!registered) {
      return fail(registered.error);
    }
  }
  std::shared_ptr<const PrivateContextCode> effective_owner =
      generated_owner.states.empty() ? *owner_code : nullptr;
  for (const auto &scope : registered.scopes) {
    if (!scope || !scope->owner_generation) {
      return fail("incompatible_registration: frozen scope has no owner code");
    }
    if (effective_owner && effective_owner != scope->owner_generation) {
      return fail("incompatible_registration: scope groups pin different owner "
                  "generations");
    }
    effective_owner = scope->owner_generation;
  }
  if (!effective_owner) {
    return fail("incompatible_registration: no owner generation was frozen");
  }
  if (effective_owner != *owner_code) {
    callback_code = load_callback(effective_owner);
    if (!callback_code) {
      return fail("invalid_context_callback: reused owner binding failed: " +
                  error_text(callback_code.takeError()));
    }
  }
  auto symbol_address = [&](const char *name) -> std::uintptr_t {
    auto symbol = (*callback_code)->symbol(name);
    if (!symbol) {
      result.error = expected_error(symbol);
      return 0;
    }
    return symbol->address();
  };
  const auto construct = symbol_address("clangtracer_context_construct");
  const auto pre = symbol_address("clangtracer_context_pre");
  const auto post = symbol_address("clangtracer_context_post");
  const auto destroy = symbol_address("clangtracer_context_destroy");
  if (!construct || !pre || !post || !destroy) {
    return result;
  }
  const auto has_hook = [&](std::string_view method) {
    return std::any_of(
        selected->hooks.begin(), selected->hooks.end(),
        [&](const auto &hook) { return hook.method == method; });
  };
  result.generation = std::shared_ptr<const ContextActivationGeneration>(
      new ContextActivationGeneration(
          std::move(effective_owner), *callback_code,
          std::move(registered.scopes), std::move(schemas), selected->type,
          object_bytes, object_alignment, construct, pre, post, destroy,
          std::move(lineage_diagnostics),
          declarations.requires_stack_capture ||
              std::any_of(
                  selected->hooks.begin(), selected->hooks.end(),
                  [](const auto &hook) { return hook.captures_stack; }),
          has_hook("pre"), has_hook("post")));
  return result;
}

ContextActivationResult
activate_context_source(const ContextActivationRequest &request,
                        ContextRegistry &registry) {
  return prepare_context_source(request, &registry, false, false);
}

ContextPreflightResult
preflight_context_source(const ContextActivationRequest &request,
                         bool syntax_only) {
  auto prepared = prepare_context_source(request, nullptr, true, syntax_only);
  return {std::move(prepared.error), std::move(prepared.source_errors)};
}

} // namespace tracer
