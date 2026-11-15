#include "live_compile.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/IR/DebugInfo.h>

#include <filesystem>
#include <tuple>
#include <stdexcept>

namespace tracer::live {
namespace {
class IncludeInputs final : public clang::PPCallbacks {
public:
  explicit IncludeInputs(std::set<std::string> &inputs) : inputs_(inputs) {}

  void InclusionDirective(clang::SourceLocation, const clang::Token &,
                          llvm::StringRef, bool, clang::CharSourceRange,
                          clang::OptionalFileEntryRef file, llvm::StringRef,
                          llvm::StringRef, const clang::Module *, bool,
                          clang::SrcMgr::CharacteristicKind) override {
    if (file)
      inputs_.insert(std::filesystem::path(file->getName().str()).lexically_normal().string());
  }

private:
  std::set<std::string> &inputs_;
};

class InspectAST final : public clang::ASTConsumer {
public:
  explicit InspectAST(const std::function<void(clang::ASTContext &)> &inspect)
      : inspect_(inspect) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    if (!context.getDiagnostics().hasErrorOccurred())
      inspect_(context);
  }

private:
  const std::function<void(clang::ASTContext &)> &inspect_;
};

class CallableTypes final : public clang::RecursiveASTVisitor<CallableTypes> {
public:
  CallableTypes(clang::ASTContext &context,
           CompiledUnit &unit)
      : context_(context), mangler_(context.createMangleContext()), unit_(unit) {}

  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitRecordDecl(clang::RecordDecl *record) {
    const auto location = context_.getSourceManager().getExpansionLoc(record->getLocation());
    if (!record->isCompleteDefinition() || record->isDependentContext() ||
        record->isInvalidDecl() || record->isImplicit() ||
        context_.getSourceManager().isInSystemHeader(location) ||
        (!record->getIdentifier() && !record->getTypedefNameForAnonDecl()))
      return true;

    clang::PrintingPolicy policy(context_.getLangOpts());
    policy.FullyQualifiedName = true;
    std::string name;
    llvm::raw_string_ostream printed_name(name);
    record->getNameForDiagnostic(printed_name, policy, true);
    if (name.empty())
      return true;
    std::string fingerprint;
    llvm::raw_string_ostream layout(fingerprint);
    std::set<const clang::RecordDecl *> seen;
    append_record(record, layout, seen);
    unit_.record_layouts.insert_or_assign(name, std::move(fingerprint));
    return true;
  }

  bool VisitFunctionDecl(clang::FunctionDecl *function) {
    const auto location = context_.getSourceManager().getExpansionLoc(function->getLocation());
    if (!function->isThisDeclarationADefinition() || function->isDependentContext() ||
        function->isImplicit() || function->isInvalidDecl() ||
        context_.getSourceManager().isInSystemHeader(location))
      return true;

    clang::PrintingPolicy policy(context_.getLangOpts());
    policy.SuppressTagKeyword = true;
    policy.FullyQualifiedName = true;
    const auto type = function->getType().getCanonicalType().getAsString(policy);
    std::string display;
    llvm::raw_string_ostream text(display);
    function->getNameForDiagnostic(text, policy, true);
    text << " [" << type << ']';
    std::string fingerprint = type;
    llvm::raw_string_ostream layout(fingerprint);
    std::set<const clang::RecordDecl *> seen;
    if (const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function)) {
      layout << " static=" << method->isStatic() << " virtual=" << method->isVirtual();
      append_record(method->getParent(), layout, seen);
    }
    append_type(function->getReturnType(), layout, seen);
    for (const auto *param : function->parameters())
      append_type(param->getType(), layout, seen);

    const auto record = [&](clang::GlobalDecl declaration) {
      std::string name;
      llvm::raw_string_ostream symbol(name);
      if (mangler_->shouldMangleDeclName(function))
        mangler_->mangleName(declaration, symbol);
      else
        symbol << function->getName();
      unit_.types.insert_or_assign(name, std::pair{display, fingerprint});
    };
    if (const auto *constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(function)) {
      record({constructor, clang::Ctor_Complete});
      record({constructor, clang::Ctor_Base});
    } else if (const auto *destructor = llvm::dyn_cast<clang::CXXDestructorDecl>(function)) {
      record({destructor, clang::Dtor_Deleting});
      record({destructor, clang::Dtor_Complete});
      record({destructor, clang::Dtor_Base});
    } else {
      record(function);
    }
    return true;
  }

private:
  void append_type(clang::QualType input, llvm::raw_ostream &out,
                   std::set<const clang::RecordDecl *> &seen) {
    const auto type = input.getCanonicalType();
    if (const auto *array = context_.getAsArrayType(type))
      append_type(array->getElementType(), out, seen);
    else if (const auto *record = type.getNonReferenceType()->getAsRecordDecl())
      append_record(record, out, seen);
    else if (type->isPointerType())
      append_type(type->getPointeeType(), out, seen);
  }

  void append_record(const clang::RecordDecl *input, llvm::raw_ostream &out,
                     std::set<const clang::RecordDecl *> &seen) {
    const auto *record = input->getDefinition();
    if (!record || record->isInvalidDecl() || record->isDependentContext() ||
        !seen.insert(record).second)
      return;
    const auto &layout = context_.getASTRecordLayout(record);
    out << " record " << record->getQualifiedNameAsString() << ':'
        << layout.getSize().getQuantity() << ':' << layout.getAlignment().getQuantity();
    unsigned index = 0;
    for (const auto *field : record->fields()) {
      out << " field " << field->getNameAsString() << ':'
          << field->getType().getCanonicalType().getAsString() << ':' << layout.getFieldOffset(index++);
      if (field->isBitField())
        out << ':' << field->getBitWidthValue();
      append_type(field->getType(), out, seen);
    }
    if (const auto *cxx = llvm::dyn_cast<clang::CXXRecordDecl>(record)) {
      for (const auto &base : cxx->bases()) {
        const auto *type = base.getType()->getAsCXXRecordDecl();
        if (!type)
          continue;
        out << " base " << (base.isVirtual() ? layout.getVBaseClassOffset(type) :
                                           layout.getBaseClassOffset(type)).getQuantity();
        append_record(type, out, seen);
      }
      for (const auto *method : cxx->methods())
        if (method->isVirtual())
          out << " virtual " << method->getNameAsString() << ':'
              << method->getType().getCanonicalType().getAsString();
    }
  }

  clang::ASTContext &context_;
  std::unique_ptr<clang::MangleContext> mangler_;
  CompiledUnit &unit_;
};

class CompileAction final : public clang::EmitLLVMOnlyAction {
public:
  CompileAction(CompiledUnit &result, llvm::LLVMContext *context,
                const std::function<void(clang::ASTContext &)> &inspect)
      : clang::EmitLLVMOnlyAction(context), result_(result), inspect_(inspect) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &compiler, llvm::StringRef input) override {
    auto codegen = clang::EmitLLVMOnlyAction::CreateASTConsumer(compiler, input);
    if (!inspect_ || !codegen)
      return codegen;
    std::vector<std::unique_ptr<clang::ASTConsumer>> consumers;
    consumers.push_back(std::make_unique<InspectAST>(inspect_));
    consumers.push_back(std::move(codegen));
    return std::make_unique<clang::MultiplexConsumer>(std::move(consumers));
  }

  bool BeginSourceFileAction(clang::CompilerInstance &compiler) override {
    if (!clang::EmitLLVMOnlyAction::BeginSourceFileAction(compiler))
      return false;
    compiler.getPreprocessor().addPPCallbacks(std::make_unique<IncludeInputs>(result_.inputs));
    return true;
  }

  void EndSourceFileAction() override {
    clang::EmitLLVMOnlyAction::EndSourceFileAction();
    result_.module = takeModule();
  }

private:
  CompiledUnit &result_;
  const std::function<void(clang::ASTContext &)> &inspect_;
};

class Diagnostics final : public clang::DiagnosticConsumer {
public:
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &diagnostic) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
    if (level < clang::DiagnosticsEngine::Error)
      return;
    llvm::SmallString<256> message;
    diagnostic.FormatDiagnostic(message);
    if (!errors.empty())
      errors += '\n';
    if (diagnostic.getLocation().isValid() && diagnostic.hasSourceManager())
      errors += diagnostic.getLocation().printToString(diagnostic.getSourceManager()) + ": ";
    errors += message.str().str();
  }

  std::string errors;
};
std::string ir_digest(llvm::StringRef bytes) {
  return llvm::toHex(llvm::SHA256::hash(llvm::arrayRefFromStringRef(bytes)), true);
}

std::string initializer_text(const llvm::GlobalVariable &global) {
  std::string text;
  llvm::raw_string_ostream out(text);
  global.getInitializer()->printAsOperand(out, true, global.getParent());
  return text;
}

std::unique_ptr<llvm::Module> comparison_module(const llvm::Module &module) {
  auto copy = llvm::CloneModule(module);
  llvm::StripDebugInfo(*copy);
  // Clang numbers pooled literals across the entire translation unit.
  // Their contents and alignment, rather than that ordinal, define the
  // value used by each instruction and initializer.
  for (auto &global : copy->globals())
    if (mergeable_literal(global))
      global.setName(".live.literal." + ir_digest(initializer_text(global) + ":" +
          std::to_string(global.getAlign().valueOrOne().value())));
  return copy;
}
} // namespace

bool mergeable_literal(const llvm::GlobalVariable &global) {
  return global.hasPrivateLinkage() && global.hasGlobalUnnamedAddr() && global.isConstant() &&
         global.hasInitializer() && global.getName().starts_with(".str") &&
         llvm::isa<llvm::ConstantDataArray>(global.getInitializer());
}

std::string local_import_name(std::string_view path, std::string_view name) {
  return "__clangtracer_live_local_" + ir_digest(path) + "_" + std::string(name);
}

std::map<std::string, std::string> data_definitions(const CompiledUnit &unit) {
  auto module = comparison_module(*unit.module);
  std::map<std::string, std::string> data;
  for (const auto &global : module->globals()) {
    if (!global.hasInitializer() || global.getName().starts_with(".live.literal."))
      continue;
    const auto size = module->getDataLayout().getTypeAllocSize(global.getValueType());
    data.emplace(global.getName().str(), std::to_string(size.getKnownMinValue()) + ":" +
        std::to_string(global.getAlign().valueOrOne().value()) + ":" +
        std::to_string(global.isConstant()) + ":" + std::to_string(global.isThreadLocal()) +
        ":" + initializer_text(global));
  }
  return data;
}

CompiledUnit compile_unit(const JitRequest &request, const TranslationUnit &unit,
                          llvm::LLVMContext *context,
                          const std::function<void(clang::ASTContext &)> &inspect,
                          CompileInspection inspection) {
  CompiledUnit result;
  if (!context) {
    result.context = std::make_unique<llvm::LLVMContext>();
    context = result.context.get();
  }
  auto memory = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
  auto filesystem = llvm::makeIntrusiveRefCnt<llvm::vfs::OverlayFileSystem>(
      llvm::vfs::createPhysicalFileSystem());
  filesystem->pushOverlay(memory);
  if (const auto error = filesystem->setCurrentWorkingDirectory(unit.directory))
    throw std::runtime_error("invalid compile working directory: " + error.message());
  for (const auto &source : request.files) {
    if (!std::filesystem::path(source.path).is_absolute() ||
        !memory->addFile(source.path, 0, llvm::MemoryBuffer::getMemBuffer(source.contents)))
      throw std::runtime_error("invalid or duplicate source snapshot path: " + source.path);
  }
  clang::FileSystemOptions options;
  options.WorkingDir = unit.directory;
  auto files = llvm::makeIntrusiveRefCnt<clang::FileManager>(options, filesystem);
  std::vector<std::string> arguments{"clang++"};
  arguments.insert(arguments.end(), unit.compiler_arguments.begin(), unit.compiler_arguments.end());
  arguments.insert(arguments.end(), {"-resource-dir", TRACER_CLANG_RESOURCE_DIR,
                                     "-fsyntax-only", "-g", "-fno-discard-value-names",
                                     "-x", "c++", unit.path});
  Diagnostics diagnostics;
  std::function<void(clang::ASTContext &)> scan = inspect;
  if (inspection == CompileInspection::native_metadata)
    scan = [&](clang::ASTContext &ast) {
      if (inspect)
        inspect(ast);
      if (!ast.getDiagnostics().hasErrorOccurred()) {
        CallableTypes visitor(ast, result);
        visitor.TraverseDecl(ast.getTranslationUnitDecl());
      }
    };
  clang::tooling::ToolInvocation invocation(std::move(arguments),
      std::make_unique<CompileAction>(result, context, scan), files.get());
  invocation.setDiagnosticConsumer(&diagnostics);
  if (!invocation.run() || !result.module)
    throw std::runtime_error(unit.path + ": " + (diagnostics.errors.empty()
        ? "Clang did not produce LLVM IR" : diagnostics.errors));
  if (result.module->getTargetTriple().getArch() != llvm::Triple::x86_64 ||
      !result.module->getTargetTriple().isOSLinux() ||
      result.module->getDataLayout().getPointerSize() != 8 ||
      llvm::verifyModule(*result.module))
    throw std::runtime_error(unit.path + ": incompatible Linux x86-64 LLVM module");
  return result;
}

bool supported_entry_abi(const llvm::Function &function) {
  const auto convention = function.getCallingConv();
  return (convention == llvm::CallingConv::C || convention == llvm::CallingConv::X86_64_SysV ||
          (convention == llvm::CallingConv::Win64 && !function.isVarArg())) &&
         !function.hasFnAttribute(llvm::Attribute::Naked);
}

std::map<std::string, Definition> definitions(const CompiledUnit &unit) {
  const auto &module = *unit.module;
  auto normalized = comparison_module(module);
  std::map<std::string, Definition> result;
  for (const auto &function : module.functions()) {
    if (function.isDeclaration() || function.isIntrinsic())
      continue;
    Definition item;
    if (const auto *debug = function.getSubprogram()) {
      item.path = (std::filesystem::path(debug->getDirectory().str()) /
                   debug->getFilename().str()).lexically_normal().string();
      item.line = debug->getLine();
      item.type = debug->getName().str();
    }
    llvm::raw_string_ostream abi(item.abi);
    function.getFunctionType()->print(abi);
    abi << " cc=" << function.getCallingConv() << ' ';
    abi << function.getAttributes().getRetAttrs().getAsString();
    for (unsigned param = 0; param < function.arg_size(); ++param)
      abi << ':' << function.getAttributes().getParamAttrs(param).getAsString();
    if (const auto info = unit.types.find(function.getName().str()); info != unit.types.end()) {
      item.type = info->second.first;
      abi << " source=" << info->second.second;
      item.has_source_abi = true;
    }
    item.supported_abi = supported_entry_abi(function);
    item.local = function.hasLocalLinkage();
    // Parameter lowering can include opaque pointers. Compare the bodies only
    // after independently checking the native declaration and LLVM ABI.
    std::string ir;
    llvm::raw_string_ostream implementation(ir);
    auto &body = *normalized->getFunction(function.getName());
    implementation << body.getAttributes().getFnAttrs().getAsString() << '\n';
    body.setAttributes(body.getAttributes().removeFnAttributes(body.getContext()));
    // Attribute group IDs also belong to the module. Keep their contents
    // separately so changes in elsewhere-emitted code cannot renumber them.
    for (auto &block : body)
      for (auto &instruction : block)
        if (auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
          const auto attrs = call->getAttributes();
          implementation << attrs.getFnAttrs().getAsString() << '\n';
          call->setAttributes(attrs.removeFnAttributes(body.getContext()));
        }
    body.print(implementation, nullptr, false, true);
    llvm::SmallVector<llvm::StringRef, 32> lines;
    llvm::StringRef(ir).split(lines, '\n');
    for (const auto line : lines) {
      if (line.trim().starts_with("#dbg_") || line.starts_with("; Function Attrs:"))
        continue;
      auto body = line.split(", !dbg !").first.split(" !dbg !").first;
      // Debug instrumentation gives implicit members unstable metadata IDs
      // even when their code is unchanged.
      body = body.split(", !DIAssignID !").first;
      if (body.starts_with("define "))
        body = body.split(" #").first;
      item.implementation += body.str();
      item.implementation += '\n';
    }
    result.emplace(function.getName().str(), std::move(item));
  }
  // Clang may emit the complete and base lifecycle variants as an alias
  // and one physical implementation. Include both in affected-entry checks.
  for (const auto &alias : module.aliases()) {
    const auto *target = llvm::dyn_cast_or_null<llvm::Function>(alias.getAliaseeObject());
    const auto typed = unit.types.find(alias.getName().str());
    if (!target || typed == unit.types.end() || alias.getValueType() != target->getValueType())
      continue;
    const auto body = result.find(target->getName().str());
    if (body == result.end() || !body->second.has_source_abi)
      continue;
    auto item = body->second;
    item.alias_target = body->first;
    item.local = alias.hasLocalLinkage();
    item.abi += " alias " + item.alias_target + " source=" + typed->second.second;
    result.emplace(alias.getName().str(), std::move(item));
  }
  for (auto &[symbol, item] : result) {
    if (item.has_source_abi || !std::string_view(symbol).starts_with("_ZT"))
      continue;
    const auto description = llvm::demangle(symbol);
    const auto delimiter = description.find(" thunk to ");
    if (delimiter == std::string::npos)
      continue;
    const auto callable = description.substr(delimiter + sizeof(" thunk to ") - 1);
    std::set<std::tuple<std::string, std::string, unsigned>> evidence;
    for (const auto &[body_symbol, body] : result)
      if (body.has_source_abi && llvm::demangle(body_symbol) == callable)
        evidence.emplace(body.abi, body.path, body.line);
    if (evidence.size() == 1) {
      const auto &[abi, path, line] = *evidence.begin();
      item.abi += " thunk " + symbol + " for " + abi;
      item.type = description;
      item.path = path;
      item.line = line;
      item.has_source_abi = true;
    }
  }
  return result;
}
} // namespace tracer::live
