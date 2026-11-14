#include "live_mock.hpp"

#include <filesystem>
#include <set>
#include <stdexcept>

#include <llvm/IR/Attributes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include "live_compile.hpp"

namespace tracer::live {
namespace {
namespace fs = std::filesystem;
constexpr std::string_view prefix = "clangtracer_live_mock_";
constexpr std::string_view original_symbol = "clangtracer_live_mock_original_v1";
constexpr std::string_view recursion_symbol = "clangtracer_live_mock_top_v1";

bool identifier(std::string_view value) {
  if (value.empty() ||
      value.find_first_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") !=
          std::string_view::npos) {
    return false;
  }
  return value.front() < '0' || value.front() > '9';
}

bool qualified_identifier(std::string_view value) {
  if (value.starts_with("::")) {
    value.remove_prefix(2);
  }
  while (true) {
    const auto split = value.find("::");
    if (!identifier(value.substr(0, split))) {
      return false;
    }
    if (split == std::string_view::npos) {
      return true;
    }
    value.remove_prefix(split + 2);
  }
}

std::string quote(std::string_view input) {
  std::string result{"\""};
  for (const unsigned char byte : input) {
    if (byte == '\\' || byte == '"' || byte == '?') {
      result += '\\';
      result += static_cast<char>(byte);
    } else if (byte < 32 || byte >= 127) {
      result += '\\';
      result += static_cast<char>('0' + (byte >> 6));
      result += static_cast<char>('0' + ((byte >> 3) & 7));
      result += static_cast<char>('0' + (byte & 7));
    } else {
      result += static_cast<char>(byte);
    }
  }
  return result + '"';
}

void append_include(std::string& code, std::string_view header) {
  // A spelling inside #include is parsed as a header token, not a C string.
  if (header.empty() || header.find_first_of("\"\\\r\n") != std::string_view::npos ||
      header.find('\0') != std::string_view::npos) {
    throw std::invalid_argument("invalid mock declaration header path");
  }
  code += "#include \"" + std::string(header) + "\"\n";
}

std::string joined(std::string_view first, std::string_view rest) {
  if (first.empty()) {
    return std::string(rest);
  }
  return std::string(first) + (rest.empty() ? "" : ", ") + std::string(rest);
}

struct GeneratedCallable {
  std::string result;
  std::string native_types;
  std::string native_parameters;
  std::string native_arguments;
  std::string callback_types;
  std::string callback_arguments;
  std::string context_parameters;
  std::string context_arguments;
  std::string exception_spec;
};

GeneratedCallable describe(std::string& types, const MockCallable& callable,
                           const std::string& tag) {
  if (!callable.address || !callable.native_definition || callable.result.empty()) {
    throw std::invalid_argument("missing verified native callable: " + callable.selector);
  }
  GeneratedCallable result;
  result.result = tag + "_result";
  result.exception_spec = callable.is_noexcept ? " noexcept" : "";
  types += "using " + result.result + " = " + callable.result + ";\n";
  if (!callable.receiver.empty()) {
    const auto receiver = tag + "_receiver";
    types += "using " + receiver + " = " + callable.receiver + ";\n";
    result.native_types = receiver + "*";
    result.native_parameters = result.native_types + " self";
    result.native_arguments = "self";
    result.callback_types = receiver + "&";
    result.callback_arguments = "*self";
    result.context_parameters = result.callback_types + " self";
    result.context_arguments = "__builtin_addressof(self)";
  }
  for (std::size_t i = 0; i < callable.parameters.size(); ++i) {
    const auto param = tag + "_parameter_" + std::to_string(i);
    const auto name = "arg" + std::to_string(i);
    types += "using " + param + " = " + callable.parameters[i] + ";\n";
    // A second boundary for a nontrivial value parameter introduces moves and
    // destruction where the native callable owns one parameter lifetime.
    types += "static_assert(std::is_reference_v<" + param +
             "> || "
             "std::is_scalar_v<" +
             param + ">, \"unsupported mock by-value parameter\");\n";
    result.native_types = joined(result.native_types, param);
    result.native_parameters = joined(result.native_parameters, param + " " + name);
    const auto forwarded = "std::forward<" + param + ">(" + name + ")";
    result.native_arguments = joined(result.native_arguments, forwarded);
    result.callback_types = joined(result.callback_types, param);
    result.callback_arguments = joined(result.callback_arguments, forwarded);
    result.context_parameters = joined(result.context_parameters, param + " " + name);
    result.context_arguments = joined(result.context_arguments, forwarded);
  }
  return result;
}

// Compare the actual SysV lowering on each side. Ignore optimization hints
// such as noundef or dereferenceable. Record layout and callable qualifiers
// come separately from the verified original compilation and headers.
std::string abi_type(const llvm::Function& function) {
  std::string result;
  llvm::raw_string_ostream output(result);
  output << "cc " << function.getCallingConv() << ' ';
  output << function.getParent()->getDataLayoutStr() << ' ';
  auto attributes = [&](llvm::AttributeSet set) {
    for (auto kind :
         {llvm::Attribute::SExt, llvm::Attribute::ZExt, llvm::Attribute::InReg,
          llvm::Attribute::StructRet, llvm::Attribute::ByVal, llvm::Attribute::ByRef,
          llvm::Attribute::InAlloca, llvm::Attribute::Preallocated, llvm::Attribute::Nest,
          llvm::Attribute::SwiftSelf, llvm::Attribute::SwiftError}) {
      auto item = set.getAttribute(kind);
      if (!item.isValid()) {
        continue;
      }
      output << ' ' << static_cast<unsigned>(kind);
      if (item.isTypeAttribute()) {
        auto* type = item.getValueAsType();
        if (!type->isSized()) {
          throw std::invalid_argument("unsized lowered mock ABI type");
        }
        const auto& layout = function.getParent()->getDataLayout();
        output << ':' << layout.getTypeAllocSize(type) << ':'
               << layout.getABITypeAlign(type).value();
        if (const auto align = set.getAlignment()) {
          output << ':' << align->value();
        }
      }
    }
  };
  function.getReturnType()->print(output);
  attributes(function.getAttributes().getRetAttrs());
  for (const auto& argument : function.args()) {
    output << ';';
    argument.getType()->print(output);
    attributes(function.getAttributes().getParamAttrs(argument.getArgNo()));
  }
  if (function.isVarArg()) {
    output << ";...";
  }
  return result;
}

void verify_abi(const llvm::Function* generated, const MockCallable& callable) {
  const auto& native = *callable.native_definition;
  if (!generated || native.isDeclaration() || native.isVarArg() ||
      native.getCallingConv() != llvm::CallingConv::C || abi_type(*generated) != abi_type(native)) {
    throw std::invalid_argument("native mock ABI mismatch for " + callable.selector);
  }
}
} // namespace

JitRequest build_live_mock(const MockCommand& command, const MockCompileContext& context) {
  if (!qualified_identifier(command.callback) || command.selector.empty() || !context.original ||
      context.original == context.target.address ||
      command.bindings.size() != context.bindings.size()) {
    throw std::invalid_argument("incomplete verified original or dependency bindings for mock");
  }

  JitRequest request;
  request.files = context.files;
  const auto context_path =
      (fs::path(command.source.path).parent_path() / "live_mock_context.hpp").string();
  // Preserve the actual callback source in the snapshot and compile a separate
  // synthetic unit that includes it alongside the generated context.
  const auto unit = command.source.path + ".clang-tracer-live.cpp";
  std::string header = "#pragma once\n#include <string_view>\n"
                       "#include <type_traits>\n#include <utility>\n";
  for (const auto& path : context.declaration_headers) {
    append_include(header, path);
  }
  header += "namespace clangtracer_live_mock_detail {\n";
  const auto target = describe(header, context.target, "target");
  const auto original = std::string(original_symbol);
  header += "extern \"C\" " + target.result + " " + original + "(" + target.native_types + ")" +
            target.exception_spec + ";\n";
  request.imports.emplace(original, context.original);
  const auto add_native_import = [&](const MockCallable& callable) {
    const auto linkage = callable.native_definition->getName().str();
    if (std::string_view(linkage).starts_with(prefix)) {
      throw std::invalid_argument("native mock binding uses a reserved linkage: " + linkage);
    }
    const auto [item, inserted] = request.imports.emplace(linkage, callable.address);
    if (!inserted && item->second != callable.address) {
      throw std::invalid_argument("conflicting mock import identity: " + callable.selector);
    }
  };
  add_native_import(context.target);

  std::string methods;
  methods += target.result + " original(" + target.context_parameters + ") const" +
             target.exception_spec + " { return " + original + "(" + target.context_arguments +
             "); }\n";
  std::set<std::string> aliases{"original", "fixture_text"};
  std::vector<std::string> imports;
  for (std::size_t i = 0; i < context.bindings.size(); ++i) {
    const auto& option = command.bindings[i];
    const auto& binding = context.bindings[i];
    if (!identifier(option.alias) || option.alias.starts_with(prefix) ||
        !aliases.insert(option.alias).second || option.selector.empty()) {
      throw std::invalid_argument("invalid or duplicate mock binding alias: " + option.alias);
    }
    const auto index = std::to_string(i);
    const auto callable = describe(header, binding, "binding_" + index);
    const auto symbol = std::string(prefix) + "binding_v1_" + index;
    header += "extern \"C\" " + callable.result + " " + symbol + "(" + callable.native_types + ")" +
              callable.exception_spec + ";\n";
    methods += callable.result + " " + option.alias + "(" + callable.context_parameters +
               ") const" + callable.exception_spec + " { return " + symbol + "(" +
               callable.context_arguments + "); }\n";
    request.imports.emplace(symbol, binding.address);
    add_native_import(binding);
    imports.push_back(symbol);
  }
  const auto fixture =
      command.fixture ? std::string_view(command.fixture->contents) : std::string_view{};
  header += "struct Context {\n" + methods +
            "std::string_view fixture_text() const noexcept { return {" + quote(fixture) + ", " +
            std::to_string(fixture.size()) + "}; }\n};\n}\n" +
            "using LiveMockContext = clangtracer_live_mock_detail::Context;\n";
  request.files.push_back({context_path, std::move(header)});
  request.files.push_back(command.source);

  std::string code;
  append_include(code, context_path);
  append_include(code, command.source.path);
  code += "\nnamespace clangtracer_live_mock_detail {\n";
  if (command.recursive_original) {
    code += R"cpp(
extern "C" void *clangtracer_live_mock_top_v1(void *, bool) noexcept;
struct Frame {
  const void *key;
  Frame *previous;
};
struct FrameScope {
  Frame frame;
  explicit FrameScope(const void *key, Frame *previous) noexcept : frame{key, previous} {
    clangtracer_live_mock_top_v1(&frame, true);
  }
  ~FrameScope() { clangtracer_live_mock_top_v1(frame.previous, true); }
};
)cpp";
  }
  code += "extern \"C\" " + target.result + " " + std::string(mock_entry_symbol) + "(" +
          target.native_parameters + ")" + target.exception_spec + " {\n";
  if (command.recursive_original) {
    code += "auto *top = static_cast<Frame *>(" + std::string(recursion_symbol) +
            "(nullptr, false));\n"
            "const auto key = reinterpret_cast<const void *>(&" +
            original +
            ");\n"
            "for (auto *frame = top; frame; frame = frame->previous)\n"
            "  if (frame->key == key) return " +
            original + "(" + target.native_arguments +
            ");\n"
            "FrameScope scope(key, top);\n";
  }
  const auto callback =
      command.callback.starts_with("::") ? command.callback : "::" + command.callback;
  code += "using WithContext = " + target.result + " (*)(" +
          joined("const Context &", target.callback_types) + ")" + target.exception_spec +
          ";\n"
          "using Plain = " +
          target.result + " (*)(" + target.callback_types + ")" + target.exception_spec +
          ";\n"
          "using Callback = decltype(&" +
          callback +
          ");\n"
          "static_assert(std::is_convertible_v<Callback, WithContext> || "
          "std::is_convertible_v<Callback, Plain>, \"mock callback type must match verified "
          "callable\");\n";
  if (command.fixture) {
    code += "static_assert(std::is_convertible_v<Callback, WithContext>, "
            "\"mock with fixture requires const LiveMockContext &\");\n";
  }
  code += "return [&](auto fn) -> " + target.result +
          " {\n"
          "if constexpr (std::is_convertible_v<decltype(fn), WithContext>)\n"
          "  return fn(" +
          joined("Context{}", target.callback_arguments) +
          ");\n"
          "else return fn(" +
          target.callback_arguments +
          ");\n"
          "}(&" +
          callback + ");\n}\n}\n";
  request.files.push_back({unit, std::move(code)});
  request.translation_units.push_back({unit, context.directory, context.compiler_arguments});
  request.entry_symbols.emplace_back(mock_entry_symbol);

  // Keep all typed imports visible in IR even if the callback does not call
  // one of them. The proof-only unit is never sent to the target.
  auto proof = request;
  auto& proof_code = proof.files.back().contents;
  proof_code += "\nnamespace clangtracer_live_mock_detail {\n"
                "[[gnu::used]] static const auto proof_original = &" +
                original + ";\n";
  for (std::size_t i = 0; i < imports.size(); ++i) {
    proof_code +=
        "[[gnu::used]] static const auto proof_" + std::to_string(i) + " = &" + imports[i] + ";\n";
  }
  proof_code += "}\n";
  auto compiled = compile_unit(proof, proof.translation_units.front());
  std::set<std::string> snapshotted;
  for (const auto& file : proof.files) {
    snapshotted.insert(file.path);
  }
  for (const auto& dependency : compiled.inputs) {
    const auto inside = [&](const fs::path& root) {
      const auto relative = fs::path(dependency).lexically_relative(root);
      return !root.empty() && !relative.empty() && *relative.begin() != "..";
    };
    if (!snapshotted.contains(dependency) &&
        (inside(context.source_root) || inside(fs::path(command.source.path).parent_path()))) {
      throw std::invalid_argument("mock has an unsnapshotted application include: " + dependency);
    }
  }
  verify_abi(compiled.module->getFunction(mock_entry_symbol), context.target);
  verify_abi(compiled.module->getFunction(original_symbol), context.target);
  const auto verify_direct = [&](const MockCallable& callable) {
    if (const auto* declaration =
            compiled.module->getFunction(callable.native_definition->getName())) {
      verify_abi(declaration, callable);
    }
  };
  verify_direct(context.target);
  for (std::size_t i = 0; i < imports.size(); ++i) {
    verify_abi(compiled.module->getFunction(imports[i]), context.bindings[i]);
    verify_direct(context.bindings[i]);
  }
  return request;
}
} // namespace tracer::live
