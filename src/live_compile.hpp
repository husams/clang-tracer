#pragma once

#include "live_update.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace clang { class ASTContext; }

namespace llvm {
class LLVMContext;
class Module;
class Function;
class GlobalVariable;
}

namespace tracer::live {

struct CompiledUnit {
  // A module must be destroyed before its context.
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::set<std::string> inputs;
  std::map<std::string, std::string> record_layouts;
  std::map<std::string, std::pair<std::string, std::string>> types;
  std::map<std::string, unsigned> source_definitions;
  struct LambdaSource {
    std::string site;
    std::string expression;
    std::string binding;
  };
  std::map<std::string, LambdaSource> lambda_sites;
};

enum class CompileInspection { native_metadata, ir_only };

CompiledUnit compile_unit(const JitRequest &request, const TranslationUnit &unit,
                          llvm::LLVMContext *context = nullptr,
                          const std::function<void(clang::ASTContext &)> &inspect = {},
                          CompileInspection inspection = CompileInspection::ir_only,
                          const std::set<std::string> *changed_inputs = nullptr);

struct Definition {
  std::string implementation;
  std::string abi;
  std::string path;
  std::string type;
  unsigned line = 0;
  std::string alias_target;
  bool has_source_abi = false;
  bool supported_abi = false;
  bool local = false;
  bool discardable = false;
  std::optional<std::string> lambda_site;
  std::string lambda_expression;
  std::string lambda_binding;
  std::optional<unsigned> source_definition;
  std::set<std::string> callees;
  unsigned invokes = 0;
  bool nounwind = false;
  std::string attributes_without_nounwind;
  std::string normal_flow;
};

bool private_constant(const llvm::GlobalVariable &global);
bool standard_inline_constant(const llvm::GlobalVariable &global);
bool has_standard_stream_initializer(llvm::Module &module);
void omit_standard_stream_initializer(llvm::Module &module);
std::map<std::string, std::string> data_definitions(const CompiledUnit &unit);
std::string local_import_name(std::string_view path, std::string_view name);
std::string native_entry_import_name(std::string_view name);

bool supported_entry_abi(const llvm::Function &function);
std::map<std::string, Definition> definitions(const CompiledUnit &unit);

} // namespace tracer::live
