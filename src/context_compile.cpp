#include <string>
#include <string_view>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <clang/Interpreter/PartialTranslationUnit.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <llvm/Support/raw_ostream.h>

#include "context_binding.hpp"
#include "tracer/compiler_options.hpp"

namespace tracer {
llvm::Error initialize_private_context_target();

llvm::Expected<CompiledContextModule>
compile_context_module(const std::string &source,
                       const std::vector<std::string> &compiler_arguments,
                       const std::vector<std::string> &state_types) {
  for (const auto &argument : compiler_arguments) {
    const std::string_view value(argument);
    if (value == "-include" || value.rfind("-include", 0) == 0 ||
        value == "-imacros" || value.rfind("-imacros", 0) == 0) {
      return llvm::createStringError(
          llvm::inconvertibleErrorCode(),
          "forced headers must be lowered into controlled source before "
          "creating a context interpreter");
    }
  }
  if (auto error = initialize_private_context_target()) {
    return std::move(error);
  }
  clang::IncrementalCompilerBuilder builder;
  const auto driver_arguments = clang_driver_arguments(compiler_arguments);
  std::vector<const char *> arguments;
  arguments.reserve(driver_arguments.size());
  for (const auto &argument : driver_arguments) {
    arguments.push_back(argument.c_str());
  }
  builder.SetCompilerArgs(arguments);
  auto compiler = builder.CreateCpp();
  if (!compiler) {
    return compiler.takeError();
  }
  auto interpreter = clang::Interpreter::create(std::move(*compiler));
  if (!interpreter) {
    return interpreter.takeError();
  }
  auto compilation_source = source;
  std::vector<std::string> type_aliases;
  if (!state_types.empty()) {
    std::string prefix = "clangtracer_context_contract_type_";
    for (std::size_t suffix = 0; source.find(prefix) != std::string::npos;
         ++suffix) {
      prefix =
          "clangtracer_context_contract_type_" + std::to_string(suffix) + "_";
    }
    compilation_source += "\n";
    type_aliases.reserve(state_types.size());
    for (std::size_t index = 0; index < state_types.size(); ++index) {
      auto alias = prefix + std::to_string(index);
      compilation_source +=
          "#ifdef " + alias + "\n#error reserved compiler type alias\n#endif\n";
      compilation_source +=
          "using " + alias + " = " + state_types[index] + ";\n";
      // A source can name a template specialization in a type position
      // without ever requiring its definition. The contract records complete
      // object layouts, so force completion here; incomplete payload roots
      // remain a compile-time rejection instead of an accepted opaque type.
      compilation_source +=
          "static_assert(sizeof(" + alias +
          ") > 0, \"context contract type must be complete\");\n";
      type_aliases.push_back(std::move(alias));
    }
  }
  std::string owned_input_marker = "clangtracer_context_owned_input_marker_v1";
  for (std::size_t suffix = 0;
       compilation_source.find(owned_input_marker) != std::string::npos;
       ++suffix) {
    owned_input_marker = "clangtracer_context_owned_input_marker_" +
                         std::to_string(suffix) + "_v1";
  }
  compilation_source += "\n#ifdef " + owned_input_marker +
                        "\n#error reserved owned-input marker\n#endif\nusing " +
                        owned_input_marker + " = int;\n";
  auto input = (*interpreter)->Parse(compilation_source);
  if (!input) {
    return input.takeError();
  }
  if (!input->TheModule || (*interpreter)
                               ->getCompilerInstance()
                               ->getDiagnostics()
                               .hasErrorOccurred()) {
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "context module compilation failed");
  }
  auto contract = context_compilation_contract(
      (*interpreter)->getASTContext(),
      (*interpreter)->getCompilerInstance()->getPreprocessor(), state_types,
      type_aliases, owned_input_marker);
  if (!contract) {
    return contract.takeError();
  }
  llvm::SmallVector<char, 0> bytes;
  llvm::raw_svector_ostream stream(bytes);
  llvm::WriteBitcodeToFile(*input->TheModule, stream);
  auto context = std::make_unique<llvm::LLVMContext>();
  auto module = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(llvm::StringRef(bytes.data(), bytes.size()),
                            "private-context-generation"),
      *context);
  if (!module) {
    return module.takeError();
  }
  return CompiledContextModule{
      llvm::orc::ThreadSafeModule(std::move(*module), std::move(context)),
      std::move(*contract)};
}
} // namespace tracer
