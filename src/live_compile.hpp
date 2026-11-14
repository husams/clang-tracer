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
}

namespace tracer::live {

struct CompiledUnit {
  // A module must be destroyed before its context.
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
  std::set<std::string> inputs;
  std::map<std::string, std::pair<std::string, std::string>> types;
};

CompiledUnit compile_unit(const JitRequest &request, const TranslationUnit &unit,
                          llvm::LLVMContext *context = nullptr,
                          const std::function<void(clang::ASTContext &)> &inspect = {});

struct Definition {
  std::string implementation;
  std::string abi;
  std::string path;
  std::string type;
  unsigned line = 0;
  std::string alias_target;
  bool has_source_abi = false;
  bool supported_abi = false;
};

bool supported_entry_abi(const llvm::Function &function);
std::map<std::string, Definition> definitions(const CompiledUnit &unit);

} // namespace tracer::live
