#pragma once

#include "live_update.hpp"

#include <functional>
#include <map>
#include <memory>
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
};

enum class CompileInspection { native_metadata, ir_only };

CompiledUnit compile_unit(const JitRequest &request, const TranslationUnit &unit,
                          llvm::LLVMContext *context = nullptr,
                          const std::function<void(clang::ASTContext &)> &inspect = {},
                          CompileInspection inspection = CompileInspection::ir_only);

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
};

bool mergeable_literal(const llvm::GlobalVariable &global);
std::map<std::string, std::string> data_definitions(const CompiledUnit &unit);
std::string local_import_name(std::string_view path, std::string_view name);

bool supported_entry_abi(const llvm::Function &function);
std::map<std::string, Definition> definitions(const CompiledUnit &unit);

} // namespace tracer::live
