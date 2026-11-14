#include "live_mock_plan.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>

#include <llvm/IR/Module.h>

#include "live_compile.hpp"
#include "live_mock.hpp"

namespace tracer::live {
Plan plan_live_mock(std::uint32_t pid, const std::string& executable,
                    const clangtracer::v1::LiveUpdateRequest& request, const Revision& active,
                    const std::function<bool()>& cancelled) {
  const auto command = read_live_mock(request);
  std::vector<std::string> selectors{command.selector};
  for (const auto& binding : command.bindings) {
    selectors.push_back(binding.selector);
  }
  const auto native = open_native_build(executable, request);
  std::vector<std::string> normalized;
  for (const auto& selector : selectors) {
    normalized.push_back(normalize_mock_selector(selector));
  }
  MockCompileContext context;
  context.source_root = native.source_root;
  context.files = native.baseline.files;
  std::set<std::string> inputs;
  for (const auto& file : native.baseline.files) {
    inputs.insert(file.path);
  }
  // Keep the defining LLVM modules alive until build_live_mock checks the
  // generated replacement and every explicit call edge against native IR.
  std::vector<std::unique_ptr<CompiledUnit>> compiled_units;
  std::map<std::string, MockDeclaration> proven;
  std::map<std::string, const TranslationUnit*> profiles;
  for (const auto& unit : native.baseline.translation_units) {
    if (cancelled && cancelled()) {
      throw Stale("native mock compilation cancelled");
    }
    std::map<std::string, MockDeclaration> declarations;
    auto compiled =
        std::make_unique<CompiledUnit>(compile_unit(native.baseline, unit, nullptr, [&](auto& ast) {
          declarations = describe_mock_callables(ast, normalized);
        }));
    for (const auto& dependency : compiled->inputs) {
      const auto relative =
          std::filesystem::path(dependency).lexically_relative(native.source_root);
      if (!relative.empty() && *relative.begin() != ".." && !inputs.contains(dependency)) {
        throw std::runtime_error("unrecorded mock build dependency: " + dependency);
      }
    }
    for (auto& [signature, declaration] : declarations) {
      auto* definition = compiled->module->getFunction(declaration.linkage);
      if (!definition || definition->isDeclaration()) {
        continue;
      }
      declaration.callable.native_definition = definition;
      for (const auto& header : declaration.headers) {
        if (!inputs.contains(header)) {
          throw std::runtime_error("mock declaration header is outside the build inventory: " +
                                   header);
        }
        if (std::find(context.declaration_headers.begin(), context.declaration_headers.end(),
                      header) == context.declaration_headers.end()) {
          context.declaration_headers.push_back(header);
        }
      }
      if (!proven.emplace(signature, std::move(declaration)).second) {
        throw std::runtime_error("ambiguous native mock defining translation unit: " + signature);
      }
      profiles.emplace(signature, &unit);
      if (signature == normalized.front()) {
        context.compiler_arguments = unit.compiler_arguments;
        context.directory = unit.directory;
      }
    }
    compiled_units.push_back(std::move(compiled));
  }

  std::vector<Entry> entries;
  for (std::size_t i = 0; i < selectors.size(); ++i) {
    const auto declaration = proven.find(normalized[i]);
    if (declaration == proven.end()) {
      throw std::runtime_error("no build-bound emitted declaration for mock callable: " +
                               selectors[i]);
    }
    const auto& profile = *profiles.at(normalized[i]);
    if (profile.compiler_arguments != context.compiler_arguments ||
        profile.directory != context.directory) {
      throw std::runtime_error("mock depends on a different native compile profile: " +
                               selectors[i]);
    }
    auto entry = bind_native_entry(pid, native.module, declaration->second.linkage, active, i == 0);
    auto callable = declaration->second.callable;
    callable.selector = selectors[i];
    callable.address = entry.address;
    if (i == 0) {
      context.target = std::move(callable);
      context.original = entry.original();
    } else {
      context.bindings.push_back(std::move(callable));
    }
    entries.push_back(std::move(entry));
  }

  Plan plan;
  plan.jit = build_live_mock(command, context);
  plan.entries.push_back(entries.front());
  plan.validate_sources = [pid, entries = std::move(entries), validate_build = native.revalidate] {
    validate_build();
    // Activation also validates overwritten entries and compares the expected
    // bytes. Ordinary helper addresses need mapping validation before publish.
    for (const auto& dependency : entries) {
      validate_entry(pid, dependency);
    }
  };
  auto& candidate = plan.candidates.emplace_back();
  candidate.set_state(clangtracer::v1::LiveUpdateStatus::PLANNED);
  candidate.set_symbol(plan.entries.front().symbol);
  candidate.set_signature(command.selector);
  candidate.set_source(command.source.path);
  candidate.set_module(plan.entries.front().module);
  candidate.set_address(plan.entries.front().address);
  return plan;
}

} // namespace tracer::live
