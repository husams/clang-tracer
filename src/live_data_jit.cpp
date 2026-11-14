#include <algorithm>
#include <filesystem>
#include <mutex>
#include <set>
#include <stdexcept>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <tracer/compiler_options.hpp>

#include "live_data_internal.hpp"

namespace tracer::live::data {
namespace {
enum class Form { shared, pointer, reference };

struct Snapshot {
  JitRequest request;
  std::size_t entry = 0;
  std::vector<std::string> arguments;

  explicit Snapshot(const JitRequest& input)
      : request(input) {
    if (!input.entry_symbols.empty()) {
      throw std::invalid_argument("live data entry points are generated from source");
    }
    if (input.translation_units.size() > 1) {
      throw std::invalid_argument("live data expects a single C++ translation unit");
    }

    std::filesystem::path selected;
    if (!input.translation_units.empty()) {
      selected = input.translation_units.front().path;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < input.files.size(); ++i) {
      const std::filesystem::path path(input.files[i].path);
      if (!path.is_absolute()) {
        throw std::invalid_argument("live data source snapshots require absolute paths");
      }
      if (selected.empty() ? (path.extension() == ".cpp" || path.extension() == ".cc" ||
                              path.extension() == ".cxx" || path.extension() == ".C")
                           : path == selected) {
        entry = i;
        ++count;
      }
    }
    if (count != 1) {
      throw std::invalid_argument("live data requires exactly one C++ source file");
    }

    if (input.translation_units.empty()) {
      auto directory = std::filesystem::path(input.files[entry].path).parent_path();
      request.translation_units.push_back(
          {input.files[entry].path, directory.string(), input.compiler_arguments});
    }
    auto& unit = request.translation_units.front();
    arguments = unit.compiler_arguments;
    std::string unsupported;
    if (!std::filesystem::path(unit.directory).is_absolute() ||
        !safe_preflight_compiler_arguments(arguments.begin(), arguments.end(), true,
                                           &unsupported)) {
      throw std::invalid_argument("invalid live data build context" +
                                  (unsupported.empty() ? std::string{} : ": " + unsupported));
    }
    arguments.insert(arguments.end(), {"-working-directory", unit.directory,
                                       "-resource-dir=" TRACER_CLANG_RESOURCE_DIR, "-x", "c++"});
  }

  SourceFile& source() { return request.files[entry]; }

  std::unique_ptr<clang::ASTUnit> parse() {
    clang::tooling::FileContentMappings files;
    for (std::size_t i = 0; i < request.files.size(); ++i) {
      if (i != entry) {
        files.emplace_back(request.files[i].path, request.files[i].contents);
      }
    }
    auto unit = clang::tooling::buildASTFromCodeWithArgs(
        source().contents, arguments, source().path, "clang++",
        std::make_shared<clang::PCHContainerOperations>(),
        clang::tooling::getClangStripDependencyFileAdjuster(), files);
    if (!unit || unit->getDiagnostics().hasErrorOccurred()) {
      throw std::invalid_argument("Clang rejected " + source().path);
    }
    if (!unit->getASTContext().getTargetInfo().getTriple().isOSLinux() ||
        unit->getASTContext().getTargetInfo().getTriple().getArch() != llvm::Triple::x86_64) {
      throw std::invalid_argument("live data requires the Linux x86-64 C++ ABI");
    }
    return unit;
  }
};

std::string type_name(clang::ASTContext& ast, clang::QualType type) {
  clang::PrintingPolicy policy(ast.getLangOpts());
  policy.SuppressTagKeyword = true;
  policy.FullyQualifiedName = true;
  return type.getCanonicalType().getAsString(policy);
}

class Functions final : public clang::RecursiveASTVisitor<Functions> {
public:
  Functions(clang::SourceManager& source, std::string_view name)
      : source_(source)
      , name_(name) {}

  bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    if (decl->getQualifiedNameAsString() == name_ && decl->isThisDeclarationADefinition() &&
        source_.isWrittenInMainFile(source_.getExpansionLoc(decl->getLocation()))) {
      found.push_back(decl);
    }
    return true;
  }

  std::vector<const clang::FunctionDecl*> found;

private:
  clang::SourceManager& source_;
  std::string_view name_;
};

std::vector<const clang::FunctionDecl*> functions(clang::ASTUnit& unit, std::string_view name) {
  Functions finder(unit.getSourceManager(), name);
  finder.TraverseDecl(unit.getASTContext().getTranslationUnitDecl());
  if (finder.found.empty()) {
    throw std::invalid_argument("no source definition for " + std::string(name));
  }
  for (const auto* fn : finder.found) {
    const auto* type = fn->getType()->getAs<clang::FunctionProtoType>();
    if (llvm::isa<clang::CXXMethodDecl>(fn) ||
        fn->getTemplatedKind() != clang::FunctionDecl::TK_NonTemplate ||
        !fn->getReturnType()->isVoidType() || !type || type->isVariadic() ||
        type->getCallConv() != clang::CC_C || !fn->getNumParams() || fn->isDeleted() ||
        fn->isConsteval()) {
      throw std::invalid_argument("expected ordinary void C++ definition for " + std::string(name));
    }
  }
  return finder.found;
}

struct Argument {
  std::string type;
  Form form;
  std::size_t size, alignment;
};

Argument argument(clang::ASTContext& ast, clang::QualType parameter) {
  parameter = parameter.getCanonicalType().getUnqualifiedType();
  if (parameter->isRValueReferenceType()) {
    throw std::invalid_argument("update requires an lvalue reference");
  }
  auto underlying = parameter->getPointeeType();
  Form form = Form::pointer;
  if (parameter->isLValueReferenceType()) {
    form = Form::reference;
  } else if (!parameter->isPointerType()) {
    const auto* shared = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
        parameter->getAsCXXRecordDecl());
    if (!shared ||
        shared->getSpecializedTemplate()->getQualifiedNameAsString() != "std::shared_ptr" ||
        shared->getTemplateArgs().size() != 1) {
      throw std::invalid_argument("update requires shared_ptr<T>, T* or T&");
    }
    underlying = shared->getTemplateArgs().get(0).getAsType();
    form = Form::shared;
  }
  const auto* record = underlying.isNull() ? nullptr : underlying->getAsCXXRecordDecl();
  if (!record || underlying.hasQualifiers() || !record->getDefinition() ||
      !record->getIdentifier() || !record->isExternallyVisible()) {
    throw std::invalid_argument("update requires mutable, complete named C++ class views");
  }
  return {type_name(ast, underlying), form,
          static_cast<std::size_t>(ast.getTypeSizeInChars(underlying).getQuantity()),
          static_cast<std::size_t>(ast.getTypeAlignInChars(underlying).getQuantity())};
}

std::string generated_name(std::size_t index) {
  return "tracer_live_data_update_" + std::to_string(index);
}

std::string expression(const Argument& parameter, bool selected) {
  if (!selected) {
    return parameter.form == Form::shared ? "{}" : "nullptr";
  }
  if (parameter.form == Form::shared) {
    return "std::shared_ptr<" + parameter.type + ">(*hold, static_cast<" + parameter.type +
           "*>(object))";
  }
  return (parameter.form == Form::reference ? "*static_cast<" : "static_cast<") + parameter.type +
         "*>(object)";
}

void append_isolated(SourceFile& source, const std::string& adapter) {
  source.contents += "\n\n#include <clang_tracer/live.hpp>\n" + adapter;
}
} // namespace

UpdateProgram compile_update(const JitRequest& request) {
  Snapshot snapshot(request);
  auto ast = snapshot.parse();
  const auto& definitions = functions(*ast, "update");
  std::string adapter;

  struct Entry {
    std::string symbol;
    Argument argument;
  };

  std::vector<Entry> entries;
  std::set<std::string> seen;
  for (const auto* fn : definitions) {
    std::vector<Argument> args;
    for (const auto* param : fn->parameters()) {
      args.push_back(argument(ast->getASTContext(), param->getType()));
    }
    const Form form = args.front().form;
    if ((form == Form::reference && args.size() != 1) ||
        (form != Form::reference && definitions.size() != 1) ||
        std::ranges::any_of(args, [form](const auto& arg) { return arg.form != form; })) {
      throw std::invalid_argument("use one nullable update signature or distinct T& overloads");
    }

    std::string signature;
    for (const auto* param : fn->parameters()) {
      if (!signature.empty()) {
        signature += ", ";
      }
      signature += type_name(ast->getASTContext(), param->getType());
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (!seen.insert(args[i].type).second) {
        throw std::invalid_argument("ambiguous update type " + args[i].type);
      }
      auto name = generated_name(entries.size());
      const auto describe = name + "_type";
      adapter += "extern \"C\" const char *" + describe +
                 "(std::size_t *size) {\n"
                 "  constexpr auto name = clang_tracer::live_detail::type_name<" +
                 args[i].type +
                 ">();\n"
                 "  *size = name.size();\n  return name.data();\n}\n";
      adapter += "extern \"C\" void " + name +
                 "(void *object, const std::shared_ptr<void> *hold) {\n"
                 "  (void)hold;\n  static_cast<void (*)(" +
                 signature + ")>(&::update)(";
      for (std::size_t j = 0; j < args.size(); ++j) {
        if (j) {
          adapter += ", ";
        }
        adapter += expression(args[j], i == j);
      }
      adapter += ");\n}\n";
      snapshot.request.entry_symbols.push_back(name);
      snapshot.request.entry_symbols.push_back(describe);
      entries.push_back({std::move(name), args[i]});
    }
  }
  append_isolated(snapshot.source(), adapter);
  UpdateProgram result{compile_in_target(snapshot.request), {}};
  for (const auto& entry : entries) {
    using Describe = const char* (*)(std::size_t*);
    const auto describe = reinterpret_cast<Describe>(result.image->lookup(entry.symbol + "_type"));
    const auto call = reinterpret_cast<Invoke>(result.image->lookup(entry.symbol));
    if (!describe || !call) {
      throw std::runtime_error("cannot bind live data callback");
    }
    std::size_t size = 0;
    const auto* name = describe(&size);
    if (!name || !size ||
        !result.callbacks
             .emplace(std::string(name, size),
                      Callback{call, entry.argument.form == Form::shared, entry.argument.size,
                               entry.argument.alignment})
             .second) {
      throw std::runtime_error("cannot bind unique live data callback");
    }
  }
  return result;
}

DiscoveryProgram compile_discovery(const LiveDataDiscovery& discovery) {
  Snapshot snapshot(discovery.source);
  auto ast = snapshot.parse();
  const auto& definitions = functions(*ast, discovery.function);
  if (definitions.size() != 1 || definitions.front()->getNumParams() != 1 ||
      type_name(ast->getASTContext(), definitions.front()->getParamDecl(0)->getType()) !=
          "clang_tracer::ObjectSink &") {
    throw std::invalid_argument("discovery requires void " + discovery.function +
                                "(clang_tracer::ObjectSink&)");
  }
  constexpr auto entry = "tracer_live_data_discover";
  append_isolated(snapshot.source(), std::string("extern \"C\" void ") + entry +
                                         "(clang_tracer::ObjectSink &objects) {\n"
                                         "  static_cast<void (*)(clang_tracer::ObjectSink &)>(&::" +
                                         definitions.front()->getQualifiedNameAsString() +
                                         ")(objects);\n}\n");
  snapshot.request.entry_symbols.push_back(entry);
  auto image = compile_in_target(snapshot.request);
  const auto invoke = reinterpret_cast<Discover>(image->lookup(entry));
  if (!invoke) {
    throw std::runtime_error("cannot bind live object discovery");
  }
  return {std::move(image), invoke, discovery.function};
}

void retain(std::shared_ptr<JitImage> image) {
  // Update code can escape through application callbacks or an owner created
  // by the submitted code. Keep every invoked image resident in the target.
  struct Images {
    std::mutex mutex;
    std::vector<std::shared_ptr<JitImage>> images;
  };

  static const auto images = std::make_unique<Images>().release();
  std::lock_guard lock(images->mutex);
  images->images.push_back(std::move(image));
}
} // namespace tracer::live::data
