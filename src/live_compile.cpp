#include "tracer/compat.hpp"
#include "live_compile.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/MultiplexConsumer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <llvm/IR/DebugInfo.h>

#include <filesystem>
#include <cctype>
#include <optional>
#include <tuple>
#include <stdexcept>

namespace tracer::live {
namespace {
std::string ir_digest(llvm::StringRef bytes);

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

using InspectUnit = std::function<void(clang::ASTContext &, clang::CodeGenerator &)>;

class InspectAST final : public clang::ASTConsumer {
public:
  InspectAST(const InspectUnit &inspect, clang::CodeGenerator &generator)
      : inspect_(inspect), generator_(generator) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    if (!context.getDiagnostics().hasErrorOccurred() && generator_.GetModule())
      inspect_(context, generator_);
  }

private:
  const InspectUnit &inspect_;
  clang::CodeGenerator &generator_;
};

class CallableTypes final : public clang::RecursiveASTVisitor<CallableTypes> {
public:
  CallableTypes(clang::ASTContext &context, clang::CodeGenerator &generator, CompiledUnit &unit)
      : context_(context), mangler_(context.createMangleContext()), unit_(unit) {
    // A separate mangling context can assign lambda ordinals in a different
    // order than IR emission, especially when templates defer instantiation.
    for (const auto &function : generator.GetModule()->functions()) {
      if (function.isDeclaration())
        continue;
      const auto *method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(
          generator.GetDeclForMangledName(function.getName()));
      if (method && method->getParent()->isLambda() && method->getOverloadedOperator() == clang::OO_Call)
        emitted_lambdas_.try_emplace(method->getCanonicalDecl(), function.getName());
    }
  }

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
    return visit_function(function);
  }

  bool VisitVarDecl(clang::VarDecl *variable) {
    if (!variable->isLocalVarDecl() || !variable->getIdentifier() || !variable->hasInit())
      return true;
    if (const auto *lambda = llvm::dyn_cast<clang::LambdaExpr>(
            variable->getInit()->IgnoreUnlessSpelledInSource()))
      lambda_variables_.insert_or_assign(lambda->getBeginLoc().getRawEncoding(),
                                         variable->getNameAsString());
    return true;
  }

  bool VisitLambdaExpr(clang::LambdaExpr *lambda) {
    const auto &sources = context_.getSourceManager();
    const auto *body = lambda->getBody();
    const auto first = body->getBeginLoc();
    const auto last = body->getEndLoc();
    CompiledUnit::LambdaSource source;
    if (first.isFileID() && last.isFileID() && !sources.isInSystemHeader(first)) {
      const auto [file, begin] = sources.getDecomposedLoc(first);
      const auto [end_file, end] = sources.getDecomposedLoc(last);
      const auto bytes = sources.getBufferData(file);
      if (file == end_file && begin < end && end < bytes.size()) {
        // A matching Itanium ordinal alone does not identify a closure.
        // Prove that only this lambda's body was edited in this source file.
        source.site = ir_digest(bytes.take_front(begin)) + ir_digest(bytes.drop_front(end + 1));
        if (const auto start = lambda->getBeginLoc(); start.isFileID()) {
          const auto [expression_file, offset] = sources.getDecomposedLoc(start);
          if (expression_file == file && offset <= begin) {
            source.expression = ir_digest(bytes.slice(offset, end + 1));
            const auto variable = lambda_variables_.find(start.getRawEncoding());
            if (variable != lambda_variables_.end())
              source.binding = variable->second + ":" + ir_digest(bytes.slice(offset, begin));
          }
        }
      }
    }
    auto *call = lambda->getCallOperator();
    if (auto *generic = call->getDescribedFunctionTemplate()) {
      for (auto *specialization : generic->specializations())
        visit_function(specialization, source);
      return true;
    }
    return visit_function(call, std::move(source));
  }

private:
  bool visit_function(clang::FunctionDecl *function, CompiledUnit::LambdaSource lambda_source = {}) {
    const auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(function);
    const bool lambda = method && method->getParent()->isLambda() &&
        method->getOverloadedOperator() == clang::OO_Call;
    const auto location = context_.getSourceManager().getExpansionLoc(function->getLocation());
    if (!function->isThisDeclarationADefinition() || function->isDependentContext() ||
        (function->isImplicit() && !lambda) || function->isInvalidDecl())
      return true;

    std::string display;
    std::string fingerprint;
    const bool system = context_.getSourceManager().isInSystemHeader(location);
    std::optional<unsigned> source_definition;
    if ((system || function->isInlined()) && function->doesThisDeclarationHaveABody()) {
      clang::ODRHash hash;
      hash.AddFunctionDecl(function);
      hash.AddStmt(function->getBody());
      source_definition = hash.CalculateHash();
    }
    if (system && !source_definition)
      return true;
    if (!system) {
      clang::PrintingPolicy policy(context_.getLangOpts());
      policy.SuppressTagKeyword = true;
      policy.FullyQualifiedName = true;
      const auto type = function->getType().getCanonicalType().getAsString(policy);
      llvm::raw_string_ostream text(display);
      function->getNameForDiagnostic(text, policy, true);
      text << " [" << type << ']';
      fingerprint = type;
      llvm::raw_string_ostream layout(fingerprint);
      std::set<const clang::RecordDecl *> seen;
      if (method) {
        layout << " static=" << method->isStatic() << " virtual=" << method->isVirtual();
        append_record(method->getParent(), layout, seen);
      }
      append_type(function->getReturnType(), layout, seen);
      for (const auto *param : function->parameters())
        append_type(param->getType(), layout, seen);
    }

    const auto record = [&](clang::GlobalDecl declaration) {
      std::string name;
      if (lambda) {
        const auto emitted = emitted_lambdas_.find(method->getCanonicalDecl());
        if (emitted == emitted_lambdas_.end())
          return;
        name = emitted->second.str();
      } else {
        llvm::raw_string_ostream symbol(name);
        if (mangler_->shouldMangleDeclName(function))
          mangler_->mangleName(declaration, symbol);
        else
          symbol << function->getName();
      }
      if (source_definition)
        unit_.source_definitions.insert_or_assign(name, *source_definition);
      if (lambda && !system) {
        auto &site = unit_.lambda_sites[name];
        if (!lambda_source.site.empty())
          site = lambda_source;
      }
      if (!system)
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
    const auto *cxx = llvm::dyn_cast<clang::CXXRecordDecl>(record);
    const auto name = cxx && cxx->isLambda() ? "(lambda)" : record->getQualifiedNameAsString();
    out << " record " << name << ':'
        << layout.getSize().getQuantity() << ':' << layout.getAlignment().getQuantity();
    clang::PrintingPolicy types(context_.getLangOpts());
    types.AnonymousTagLocations = false;
    unsigned index = 0;
    for (const auto *field : record->fields()) {
      out << " field " << field->getNameAsString() << ':'
          << field->getType().getCanonicalType().getAsString(types) << ':'
          << layout.getFieldOffset(index++);
      if (field->isBitField())
        out << ':' << field->getBitWidthValue();
      append_type(field->getType(), out, seen);
    }
    if (cxx) {
      if (cxx->isLambda())
        for (const auto &capture : cxx->captures()) {
          out << " capture " << static_cast<unsigned>(capture.getCaptureKind());
          if (capture.capturesVariable())
            out << ':' << capture.getCapturedVar()->getNameAsString();
        }
      if (!context_.getSourceManager().isInSystemHeader(record->getLocation()))
        out << " trivial-copy=" << cxx->isTriviallyCopyable()
            << " trivial-copy-ctor=" << cxx->hasTrivialCopyConstructor()
            << " trivial-move-ctor=" << cxx->hasTrivialMoveConstructor()
            << " trivial-destructor=" << cxx->hasTrivialDestructor();
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
  llvm::DenseMap<const clang::FunctionDecl *, llvm::StringRef> emitted_lambdas_;
  std::map<unsigned, std::string> lambda_variables_;
  CompiledUnit &unit_;
};

class CompileAction final : public clang::EmitLLVMOnlyAction {
public:
  CompileAction(CompiledUnit &result, llvm::LLVMContext *context,
                const InspectUnit &inspect)
      : clang::EmitLLVMOnlyAction(context), result_(result), inspect_(inspect) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &compiler, llvm::StringRef input) override {
    if (inspect_)
      compiler.getCodeGenOpts().ClearASTBeforeBackend = false;
    auto codegen = clang::EmitLLVMOnlyAction::CreateASTConsumer(compiler, input);
    if (!inspect_ || !codegen)
      return codegen;
    std::vector<std::unique_ptr<clang::ASTConsumer>> consumers;
    consumers.push_back(std::move(codegen));
    consumers.push_back(std::make_unique<InspectAST>(inspect_, *getCodeGenerator()));
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
  const InspectUnit &inspect_;
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

// LLVM assigns identified struct names within each context. Use their
// lowered representation for comparisons while retaining the source ABI
// fingerprint for C++ type identity and record layout.
class IRTypeNames {
public:
  explicit IRTypeNames(const llvm::Module &module) {
    for (auto *type : module.getIdentifiedStructTypes()) {
      if (!type->hasName() || type->isOpaque())
        continue;
      std::string spelling;
      llvm::raw_string_ostream out(spelling);
      type->print(out, false, true);
      names_.emplace(std::move(spelling), shape(type));
    }
  }

  std::string normalize(llvm::StringRef input) const {
    std::string result;
    result.reserve(input.size());
    const auto quoted_end = [&](std::size_t i) {
      const auto close = input.find('"', i + 1);
      return close == llvm::StringRef::npos ? input.size() : close + 1;
    };
    for (std::size_t i = 0; i < input.size();) {
      if (input[i] == '%' && i + 1 < input.size()) {
        auto end = i + 1;
        if (input[end] == '"') {
          end = quoted_end(end);
        } else {
          while (end < input.size() &&
                 (std::isalnum(static_cast<unsigned char>(input[end])) ||
                  llvm::StringRef("._-$").contains(input[end])))
            ++end;
        }
        auto token = input.slice(i, end);
        const auto found = names_.find(token);
        result += found == names_.end() ? token : llvm::StringRef(found->second);
        i = end;
      } else if (input[i] == '"') {
        const auto end = quoted_end(i);
        result += input.slice(i, end);
        i = end;
      } else {
        result += input[i++];
      }
    }
    return result;
  }

private:
  const std::string &shape(llvm::Type *type) {
    const auto item = shapes_.find(type);
    if (item != shapes_.end())
      return item->second;
    std::string description;
    llvm::raw_string_ostream out(description);
    if (const auto *record = llvm::dyn_cast<llvm::StructType>(type); record && !record->isOpaque()) {
      out << "record:" << record->isPacked();
      for (auto *field : record->elements())
        out << ':' << shape(field);
    } else if (const auto *array = llvm::dyn_cast<llvm::ArrayType>(type)) {
      out << "array:" << array->getNumElements() << ':' << shape(array->getElementType());
    } else {
      type->print(out, false, true);
    }
    return shapes_.try_emplace(type, "%live.type." + ir_digest(description)).first->second;
  }

  llvm::DenseMap<llvm::Type *, std::string> shapes_;
  std::map<std::string, std::string, std::less<>> names_;
};

std::string initializer_text(const llvm::GlobalVariable &global) {
  std::string text;
  llvm::raw_string_ostream out(text);
  global.getInitializer()->printAsOperand(out, true, global.getParent());
  return text;
}

std::string constant_owners(const llvm::GlobalVariable &global) {
  std::set<std::string> owners;
  llvm::SmallPtrSet<const llvm::User *, 32> visited;
  llvm::SmallVector<const llvm::User *> pending(global.users());
  while (!pending.empty()) {
    const auto *use = pending.pop_back_val();
    if (!visited.insert(use).second)
      continue;
    if (const auto *instruction = llvm::dyn_cast<llvm::Instruction>(use)) {
      owners.insert(instruction->getFunction()->getName().str());
    } else if (const auto *parent = llvm::dyn_cast<llvm::GlobalValue>(use);
               parent && !parent->hasPrivateLinkage()) {
      owners.insert(parent->getName().str());
    } else {
      pending.append(use->user_begin(), use->user_end());
    }
  }
  std::string names;
  for (const auto &owner : owners) {
    names += owner;
    names += '\n';
  }
  return names;
}

std::unique_ptr<llvm::Module> comparison_module(const llvm::Module &module) {
  auto copy = llvm::CloneModule(module);
  llvm::StripDebugInfo(*copy);
  // Clang can include different template pretty-printing details depending
  // on instantiation order. Ignore that argument only on libstdc++'s fatal
  // assertion path, keeping the assertion itself and every other operand.
  for (auto &function : *copy)
    for (auto &block : function)
      for (auto &instruction : block)
        if (auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction);
            call && call->arg_size() == 4 && call->getCalledFunction() &&
            call->getCalledFunction()->getName() == "_ZSt21__glibcxx_assert_failPKciS0_S0_") {
          const auto *description = llvm::dyn_cast<llvm::GlobalVariable>(
              call->getArgOperand(2)->stripPointerCasts());
          if (description && private_constant(*description)) {
            auto name = description->getName();
            if (name.consume_front("__PRETTY_FUNCTION__.") && name == function.getName())
              call->setArgOperand(2, llvm::Constant::getNullValue(call->getArgOperand(2)->getType()));
          }
        }
  const IRTypeNames types(*copy);
  // Clang numbers pooled literals across the entire translation unit.
  // Their contents and alignment, rather than that ordinal, define the
  // value used by each instruction and initializer.
  std::map<std::string, llvm::GlobalVariable *> literals;
  llvm::SmallPtrSet<llvm::GlobalVariable *, 32> visited;
  llvm::SmallVector<llvm::GlobalVariable *> redundant;
  std::map<std::string, unsigned> ordinals;
  const auto name_literal = [&](auto &&self, llvm::GlobalVariable &global) -> void {
    if (!private_constant(global) || !visited.insert(&global).second)
      return;
    // Aggregate constants can refer to pooled strings and other constants.
    // Visit referenced globals before printing the initializer. Collect the
    // references before coalescing because that can replace constants in IR.
    llvm::SmallVector<llvm::GlobalVariable *> references;
    llvm::SmallVector<llvm::Constant *> pending{global.getInitializer()};
    while (!pending.empty()) {
      auto *operand = pending.pop_back_val();
      if (auto *reference = llvm::dyn_cast<llvm::GlobalVariable>(operand))
        references.push_back(reference);
      else if (!llvm::isa<llvm::GlobalValue, llvm::BlockAddress>(operand))
        for (auto &nested : operand->operands())
          pending.push_back(llvm::cast<llvm::Constant>(nested.get()));
    }
    for (auto *reference : references)
      self(self, *reference);
    const auto identity = global.hasGlobalUnnamedAddr() ? std::string{} : constant_owners(global);
    const auto name = ".live.literal." + ir_digest(types.normalize(initializer_text(global)) + ":" +
        std::to_string(global.getAlign().valueOrOne().value()) + ":" + identity);
    if (global.hasGlobalUnnamedAddr()) {
      const auto [prior, inserted] = literals.emplace(name, &global);
      if (!inserted && global.getInitializer() == prior->second->getInitializer() &&
          global.getType() == prior->second->getType()) {
        global.replaceAllUsesWith(prior->second);
        redundant.push_back(&global);
        return;
      }
    }
    global.setName(name + "." + std::to_string(ordinals[name]++));
  };
  for (auto &global : copy->globals())
    name_literal(name_literal, global);
  for (auto *global : redundant)
    global->eraseFromParent();
  return copy;
}

void append_instruction_metadata(llvm::Function &function, llvm::raw_ostream &out) {
  llvm::DenseMap<const llvm::MDNode *, unsigned> nodes;
  const auto describe = [&](auto &&self, const llvm::Metadata *metadata) -> void {
    if (const auto *node = llvm::dyn_cast_or_null<llvm::MDNode>(metadata)) {
      const auto [prior, inserted] = nodes.try_emplace(node, nodes.size());
      out << '!' << prior->second;
      if (inserted) {
        out << ':' << node->getMetadataID() << ':' << node->isDistinct() << '{';
        for (const auto &operand : node->operands()) {
          self(self, operand.get());
          out << ',';
        }
        out << '}';
      }
    } else if (metadata) {
      metadata->printAsOperand(out, function.getParent());
    } else {
      out << "null";
    }
  };
  llvm::SmallVector<llvm::StringRef> kinds;
  function.getContext().getMDKindNames(kinds);
  unsigned index = 0;
  for (auto &block : function) {
    for (auto &instruction : block) {
      llvm::SmallVector<std::pair<unsigned, llvm::MDNode *>> attachments;
      instruction.getAllMetadataOtherThanDebugLoc(attachments);
      llvm::sort(attachments, [&](const auto &a, const auto &b) {
        return kinds[a.first] < kinds[b.first];
      });
      for (const auto &[kind, node] : attachments) {
        if (kinds[kind] != "srcloc") {
          out << index << ':' << kinds[kind] << '=';
          describe(describe, node);
          out << '\n';
        }
        instruction.setMetadata(kind, nullptr);
      }
      ++index;
    }
  }
}

std::string body_text(llvm::Function &body, const IRTypeNames &types) {
  std::string ir;
  llvm::raw_string_ostream out(ir);
  append_instruction_metadata(body, out);
  out << body.getAttributes().getFnAttrs().getAsString() << '\n';
  body.setAttributes(body.getAttributes().removeFnAttributes(body.getContext()));
  // Attribute groups are numbered across a module. Print their contents.
  for (auto &block : body)
    for (auto &instruction : block)
      if (auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
        const auto attrs = call->getAttributes();
        out << attrs.getFnAttrs().getAsString() << '\n';
        call->setAttributes(attrs.removeFnAttributes(body.getContext()));
      }
  body.print(out, nullptr, false, true);
  llvm::SmallVector<llvm::StringRef, 32> lines;
  llvm::StringRef(ir).split(lines, '\n');
  std::string result;
  for (auto line : lines) {
    if (line.trim().starts_with("#dbg_") || ::tracer::compat::starts_with(line, "; Function Attrs:"))
      continue;
    line = line.split(", !dbg !").first.split(" !dbg !").first.split(", !DIAssignID !").first;
    if (::tracer::compat::starts_with(line, "define "))
      line = line.split(" #").first;
    result += line;
    result += '\n';
  }
  return types.normalize(result);
}

void normalize_local_values(llvm::Function &body) {
  // The front end can emit non-escaping spill slots in a different order.
  // Promote only slots proved to have ordinary local loads and stores.
  llvm::SmallVector<llvm::AllocaInst *, 16> slots;
  for (auto &instruction : body.getEntryBlock())
    if (auto *slot = llvm::dyn_cast<llvm::AllocaInst>(&instruction);
        slot && llvm::isAllocaPromotable(slot))
      slots.push_back(slot);
  if (!slots.empty()) {
    llvm::DominatorTree dominators(body);
    llvm::PromoteMemToReg(slots, dominators);
  }
  for (auto &arg : body.args())
    arg.setName("");
  for (auto &block : body) {
    block.setName("");
    for (auto &instruction : block)
      instruction.setName("");
  }
}

std::string normal_flow(llvm::Function &body, const IRTypeNames &types) {
  llvm::ValueToValueMapTy mapping;
  auto *copy = llvm::CloneFunction(&body, mapping);
  copy->setName("__clangtracer_compare_" + body.getName());
  for (auto &block : *copy)
    if (auto *invoke = llvm::dyn_cast<llvm::InvokeInst>(block.getTerminator()))
      llvm::changeToCall(invoke);
  llvm::removeUnreachableBlocks(*copy);
  for (auto block = std::next(copy->begin()); block != copy->end();) {
    auto &candidate = *block++;
    llvm::MergeBlockIntoPredecessor(&candidate);
  }
  if (copy->hasPersonalityFn())
    copy->setPersonalityFn(nullptr);
  copy->removeFnAttr(llvm::Attribute::NoUnwind);
  for (auto &arg : copy->args())
    arg.setName("");
  for (auto &block : *copy) {
    block.setName("");
    for (auto item = block.begin(); item != block.end();) {
      auto &instruction = *item++;
      // Removing the unwind blocks leaves the exception spill slots unused.
      if (llvm::isa<llvm::AllocaInst>(instruction) && instruction.use_empty()) {
        instruction.eraseFromParent();
        continue;
      }
      instruction.setName("");
      if (auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction))
        call->removeFnAttr(llvm::Attribute::NoUnwind);
    }
  }
  const auto digest = ir_digest(body_text(*copy, types));
  copy->eraseFromParent();
  return digest;
}

struct StreamInitializer {
  llvm::GlobalVariable *table;
  llvm::Function *wrapper;
  llvm::Function *init;
  llvm::GlobalVariable *storage;
};

bool declares_standard_stream_initializer(const llvm::Module &module) {
  return llvm::StringRef(module.getModuleInlineAsm()).trim() ==
      ".globl _ZSt21ios_base_library_initv";
}

std::optional<StreamInitializer> standard_stream_initializer(llvm::Module &module) {
  auto *table = module.getNamedGlobal("llvm.global_ctors");
  auto *storage = module.getNamedGlobal("_ZStL8__ioinit");
  if (!table || !table->hasInitializer() || !table->use_empty() ||
      !storage || !storage->hasLocalLinkage() || !storage->hasNUses(2))
    return std::nullopt;
  const auto *list = llvm::dyn_cast<llvm::ConstantArray>(table->getInitializer());
  if (!list || list->getNumOperands() != 1)
    return std::nullopt;
  const auto *entry = llvm::dyn_cast<llvm::ConstantStruct>(list->getOperand(0));
  if (!entry || entry->getNumOperands() != 3 || !entry->getOperand(2)->isNullValue())
    return std::nullopt;
  auto *wrapper = llvm::dyn_cast<llvm::Function>(entry->getOperand(1));
  if (!wrapper || !wrapper->hasLocalLinkage() || !wrapper->hasOneUse() ||
      wrapper->size() != 1 || wrapper->front().size() != 2 ||
      !llvm::isa<llvm::ReturnInst>(wrapper->front().back()))
    return std::nullopt;
  const auto *call = llvm::dyn_cast<llvm::CallInst>(&wrapper->front().front());
  auto *init = call ? call->getCalledFunction() : nullptr;
  if (!init || !init->hasLocalLinkage() || !init->hasOneUse() ||
      init->size() != 1 || init->front().size() != 3 ||
      !llvm::isa<llvm::ReturnInst>(init->front().back()))
    return std::nullopt;
  const auto *construct = llvm::dyn_cast<llvm::CallInst>(&init->front().front());
  const auto *atexit = llvm::dyn_cast<llvm::CallInst>(init->front().back().getPrevNode());
  if (!construct || construct->arg_size() != 1 || construct->getArgOperand(0) != storage ||
      !construct->getCalledFunction() || !construct->getCalledFunction()->isDeclaration() ||
      construct->getCalledFunction()->getName() != "_ZNSt8ios_base4InitC1Ev" ||
      !atexit || atexit->arg_size() != 3 || atexit->getArgOperand(1) != storage ||
      !atexit->getCalledFunction() || !atexit->getCalledFunction()->isDeclaration() ||
      atexit->getCalledFunction()->getName() != "__cxa_atexit")
    return std::nullopt;
  const auto *destroy = llvm::dyn_cast<llvm::Function>(atexit->getArgOperand(0));
  if (!destroy || !destroy->isDeclaration() || destroy->getName() != "_ZNSt8ios_base4InitD1Ev" ||
      atexit->getArgOperand(2) != module.getNamedGlobal("__dso_handle"))
    return std::nullopt;
  return StreamInitializer{table, wrapper, init, storage};
}
} // namespace

bool private_constant(const llvm::GlobalVariable &global) {
  return global.hasPrivateLinkage() && global.isConstant() && global.hasInitializer() &&
         !global.isThreadLocal();
}

bool standard_inline_constant(const llvm::GlobalVariable &global) {
  const auto name = global.getName();
  if (!global.isConstant() || global.isThreadLocal())
    return false;
  if (global.hasLocalLinkage())
    return ::tracer::compat::starts_with(name, "_ZZNSt8__format") || ::tracer::compat::starts_with(name, "_ZNSt9__unicode");
  return (global.hasLinkOnceODRLinkage() || global.hasWeakODRLinkage()) &&
      (::tracer::compat::starts_with(name, "_ZSt") || ::tracer::compat::starts_with(name, "_ZNSt") || ::tracer::compat::starts_with(name, "_ZZNSt8__format") ||
       ::tracer::compat::starts_with(name, "_ZTVNSt") || ::tracer::compat::starts_with(name, "_ZTINSt") ||
       ::tracer::compat::starts_with(name, "_ZTSNSt") || ::tracer::compat::starts_with(name, "_ZTVSt") ||
       ::tracer::compat::starts_with(name, "_ZTISt") || ::tracer::compat::starts_with(name, "_ZTSSt"));
}

bool has_standard_stream_initializer(llvm::Module &module) {
  return declares_standard_stream_initializer(module) || standard_stream_initializer(module).has_value();
}

void omit_standard_stream_initializer(llvm::Module &module) {
  if (declares_standard_stream_initializer(module))
    module.setModuleInlineAsm("");
  if (const auto init = standard_stream_initializer(module)) {
    init->table->eraseFromParent();
    init->wrapper->removeDeadConstantUsers();
    init->wrapper->eraseFromParent();
    init->init->eraseFromParent();
    init->storage->eraseFromParent();
  }
}

std::string local_import_name(std::string_view path, std::string_view name) {
  return "__clangtracer_live_local_" + ir_digest(path) + "_" + std::string(name);
}

std::string native_entry_import_name(std::string_view name) {
  return "__clangtracer_live_native_" + std::string(name);
}

std::map<std::string, std::string> data_definitions(const CompiledUnit &unit) {
  auto module = comparison_module(*unit.module);
  const IRTypeNames types(*module);
  omit_standard_stream_initializer(*module);
  std::map<std::string, std::string> data;
  for (const auto &global : module->globals()) {
    if (!global.hasInitializer() || private_constant(global))
      continue;
    const auto size = module->getDataLayout().getTypeAllocSize(global.getValueType());
    data.emplace(global.getName().str(), std::to_string(size.getKnownMinValue()) + ":" +
        std::to_string(global.getAlign().valueOrOne().value()) + ":" +
        std::to_string(global.isConstant()) + ":" + std::to_string(global.isThreadLocal()) +
        ":" + types.normalize(initializer_text(global)));
  }
  return data;
}

CompiledUnit compile_unit(const JitRequest &request, const TranslationUnit &unit,
                          llvm::LLVMContext *context,
                          const std::function<void(clang::ASTContext &)> &inspect,
                          CompileInspection inspection,
                          const std::set<std::string> *changed_inputs) {
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
  InspectUnit scan;
  if (inspect || inspection == CompileInspection::native_metadata)
    scan = [&](clang::ASTContext &ast, clang::CodeGenerator &generator) {
      if (inspect)
        inspect(ast);
      if (inspection == CompileInspection::native_metadata && !ast.getDiagnostics().hasErrorOccurred() &&
          (!changed_inputs || ::tracer::compat::contains((*changed_inputs), unit.path) ||
           ::tracer::compat::ranges::any_of(result.inputs,
               [&](const auto &input) { return ::tracer::compat::contains((*changed_inputs), input); }))) {
        CallableTypes visitor(ast, generator, result);
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
  const IRTypeNames types(module);
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
    std::string lowered_abi;
    llvm::raw_string_ostream abi(lowered_abi);
    function.getFunctionType()->print(abi);
    abi << " cc=" << function.getCallingConv() << ' ';
    abi << function.getAttributes().getRetAttrs().getAsString();
    for (unsigned param = 0; param < function.arg_size(); ++param)
      abi << ':' << function.getAttributes().getParamAttrs(param).getAsString();
    item.abi = types.normalize(lowered_abi);
    if (const auto info = unit.types.find(function.getName().str()); info != unit.types.end()) {
      item.type = info->second.first;
      item.abi += " source=" + info->second.second;
      item.has_source_abi = true;
    }
    item.supported_abi = supported_entry_abi(function);
    item.local = function.hasLocalLinkage();
    item.discardable = item.local || function.hasLinkOnceODRLinkage() ||
                       function.hasWeakODRLinkage() || function.hasAvailableExternallyLinkage();
    if (const auto site = unit.lambda_sites.find(function.getName().str()); site != unit.lambda_sites.end()) {
      item.lambda_site = site->second.site;
      item.lambda_expression = site->second.expression;
      item.lambda_binding = site->second.binding;
    } else if (item.local && function.getName().contains("$_") &&
             llvm::StringRef(item.type).contains("operator()"))
      item.lambda_site.emplace();
    if (const auto source = unit.source_definitions.find(function.getName().str());
        source != unit.source_definitions.end())
      item.source_definition = source->second;
    if (item.source_definition && !item.has_source_abi) {
      item.nounwind = function.hasFnAttribute(llvm::Attribute::NoUnwind);
      item.attributes_without_nounwind = function.getAttributes()
          .removeFnAttribute(function.getContext(), llvm::Attribute::NoUnwind)
          .getFnAttrs().getAsString();
    }
    // Parameter lowering can include opaque pointers. Compare the bodies only
    // after independently checking the native declaration and LLVM ABI.
    auto &body = *normalized->getFunction(function.getName());
    for (const auto &block : body)
      for (const auto &instruction : block)
        if (const auto *call = llvm::dyn_cast<llvm::CallBase>(&instruction)) {
          if (llvm::isa<llvm::InvokeInst>(call))
            ++item.invokes;
          if (const auto *callee = call->getCalledFunction();
              callee && !callee->isIntrinsic() && callee->getName() != "__clang_call_terminate" &&
              callee->getName() != "__cxa_begin_catch" && callee->getName() != "__cxa_end_catch")
            item.callees.insert(callee->getName().str());
        }
    if (item.source_definition && item.discardable && !item.local) {
      normalize_local_values(body);
      if (!item.callees.empty())
        item.normal_flow = normal_flow(body, types);
    }
    item.implementation = body_text(body, types);
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
    item.discardable = item.local || alias.hasLinkOnceODRLinkage() || alias.hasWeakODRLinkage();
    item.abi += " alias " + item.alias_target + " source=" + typed->second.second;
    result.emplace(alias.getName().str(), std::move(item));
  }
  for (auto &[symbol, item] : result) {
    if (item.has_source_abi || !::tracer::compat::starts_with(std::string_view(symbol), "_ZT"))
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
