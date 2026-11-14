#include "live_extension_plan.hpp"

#include "debug_resolver.hpp"
#include "live_data.hpp"
#include "live_plan.hpp"

#include <filesystem>
#include <stdexcept>

namespace tracer::live {
namespace {
namespace api = clangtracer::v1;
using Extension = api::LiveAgentExtension;
using Action = api::LiveUpdateRequest;
namespace fs = std::filesystem;

std::string resolve_module(std::uint32_t pid, const std::string &executable, const std::string &selector) {
  if (selector.empty())
    return executable;
  std::vector<std::string> matches;
  for (const auto &path : client::list_debug_modules(pid)) {
    if (path == selector || fs::path(path).filename() == selector)
      matches.push_back(path);
  }
  if (matches.size() != 1)
    throw std::runtime_error("live module must select one mapped ELF: " + selector);
  return matches.front();
}

std::vector<std::string> arguments(const Action &request) {
  std::vector<std::string> args(request.arguments().begin(), request.arguments().end());
  for (const auto &option : request.options()) {
    args.push_back(option.name());
    args.push_back(option.value());
  }
  return args;
}

std::string option(const Action &request, const std::string &name) {
  std::string answer;
  for (const auto &item : request.options()) {
    if (item.name() != name)
      continue;
    if (!answer.empty() || item.value().empty())
      throw std::runtime_error("invalid or duplicate " + name);
    answer = item.value();
  }
  return answer;
}

api::LiveJitInput encode(const JitRequest &request, const std::vector<std::string> &paths) {
  api::LiveJitInput result;
  for (const auto &path : paths) {
    for (const auto &unit : request.translation_units) {
      if (unit.path != path)
        continue;
      auto *output = result.add_units();
      output->set_path(unit.path);
      output->set_directory(unit.directory);
      for (const auto &argument : unit.compiler_arguments)
        output->add_compiler_arguments(argument);
    }
  }
  for (const auto &snapshot : request.files) {
    auto *source = result.add_sources();
    source->set_path(snapshot.path);
    source->set_contents(snapshot.contents);
  }
  for (const auto &[name, address] : request.imports)
    (*result.mutable_imports())[name] = address;
  for (const auto &argument : request.compiler_arguments)
    result.add_compiler_arguments(argument);
  return result;
}
} // namespace

ExtensionPlan plan_extension(std::uint32_t pid, std::uint64_t process_start,
    const std::string &session, const std::string &executable, const Action &request) {
  ExtensionPlan result;
  auto &input = result.input;
  input.set_session(session);
  input.set_process_start(process_start);
  std::string discovery;
  std::vector<std::string> update_paths;
  std::string registry;
  std::string module;

  if (request.action() == Action::UPDATE_DATA) {
    auto parsed = parse_live_data_update(arguments(request));
    input.set_action(Extension::DATA_UPDATE);
    discovery = std::move(parsed.discover_file);
    input.set_discovery_function(parsed.discover_function);
    module = std::move(parsed.selection.module);
    if (parsed.selection.address)
      input.set_address(*parsed.selection.address);
    for (const auto &id : parsed.selection.scan_ids)
      input.add_scan_ids(id);
    update_paths.push_back(std::move(parsed.file));
  } else if (request.action() == Action::SCAN_HEAP) {
    auto parsed = parse_live_heap_scan(arguments(request));
    input.set_action(Extension::HEAP_SCAN);
    discovery = std::move(parsed.discover_file);
    input.set_discovery_function(parsed.discover_function);
    module = std::move(parsed.module);
    for (const auto &type : parsed.types)
      input.add_types(type);
  } else {
    input.set_action(Extension::MANAGED_UPDATE);
    for (const auto &source : request.sources())
      update_paths.push_back(source.path());
    const auto hook = option(request, "--after-file");
    if (!hook.empty())
      input.set_hook_file(fs::canonical(hook).string());
    input.set_hook_function(option(request, "--after-function"));
    input.set_hook_type(option(request, "--for-each"));
    if (input.hook_file().empty() != input.hook_function().empty() ||
        input.hook_file().empty() != input.hook_type().empty())
      throw std::runtime_error("the after file, function and type are required together");
    for (const auto &item : request.options()) {
      if (item.name() != "--after-file" && item.name() != "--after-function" &&
          item.name() != "--for-each" && item.name() != "--module" && item.name() != "--managed")
        throw std::runtime_error("unknown managed update option " + item.name());
    }
    // A required, exact-build C linkage entry supplies the application's stable registry.
    const auto explicit_mode = option(request, "--managed");
    if (!explicit_mode.empty() && explicit_mode != "true")
      throw std::runtime_error("use --managed true to run a managed layout recipe");
    if (explicit_mode.empty() && input.hook_file().empty())
      throw std::runtime_error("managed layout update requires --managed true or an after hook");
    registry = "clang_tracer_managed_registry_v1";
    module = option(request, "--module");
  }

  module = resolve_module(pid, executable, module);
  input.set_module(module);
  for (auto &path : update_paths)
    path = fs::canonical(path).string();
  if (!discovery.empty())
    discovery = fs::canonical(discovery).string();
  auto paths = update_paths;
  if (!discovery.empty())
    paths.push_back(discovery);
  if (!input.hook_file().empty())
    paths.push_back(input.hook_file());

  auto sources = extension_sources(pid, module, request, paths, registry);
  input.set_build_id(sources.build_id);
  input.set_registry(sources.entry);
  if (!input.hook_file().empty())
    update_paths.push_back(input.hook_file());
  if (!update_paths.empty())
    *input.mutable_update() = encode(sources.jit, update_paths);
  if (!discovery.empty())
    *input.mutable_discovery() = encode(sources.jit, {discovery});
  result.validate = std::move(sources.validate);
  return result;
}

} // namespace tracer::live
