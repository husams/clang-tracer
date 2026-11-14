#include "live_migration.hpp"

#include <algorithm>
#include <filesystem>
#include <set>

#include "live_update.hpp"

namespace tracer::live::managed {
namespace {
struct StaleLayout : std::runtime_error {
  using std::runtime_error::runtime_error;
};

constexpr std::string_view RecipeSymbol = "clang_tracer_managed_update_v1";
constexpr std::string_view HookSymbol = "clang_tracer_managed_after_v1";

bool all_or_no_hook(const HookOptions& hook) {
  return hook.file.empty() == hook.function.empty() && hook.file.empty() == hook.type.empty();
}

bool function_name(std::string_view name) {
  while (!name.empty()) {
    const auto separator = name.find("::");
    const auto identifier = name.substr(0, separator);
    constexpr std::string_view Head = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr std::string_view Tail =
        "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (identifier.empty() || Head.find(identifier.front()) == Head.npos ||
        identifier.find_first_not_of(Tail) != Tail.npos) {
      return false;
    }
    if (separator == name.npos) {
      return true;
    }
    name.remove_prefix(separator + 2);
  }
  return false;
}

std::string include_quoted(const std::string& path) {
  if (path.find_first_of("\n\r\"\\") != path.npos) {
    throw std::invalid_argument("managed source path cannot be included: " + path);
  }
  return "\"" + path + "\"";
}

std::string current_error() {
  try {
    throw;
  } catch (const std::exception& error) {
    return error.what();
  } catch (...) {
    return "unknown C++ exception";
  }
}

CodeResult publish(const CodeAction& action) {
  try {
    return action();
  } catch (...) {
    return {CodeState::restoration_required, current_error()};
  }
}

bool same_layout(const auto& layout, const auto& binding) {
  return layout.id == binding.layout_id && layout.cpp_type == binding.cpp_type &&
         layout.size == binding.size && layout.alignment == binding.alignment;
}
} // namespace

UpdateOptions parse_update(std::span<const std::string> arguments) {
  UpdateOptions result;
  for (std::size_t i = 0; i != arguments.size(); ++i) {
    const auto& argument = arguments[i];
    if (!argument.starts_with("--")) {
      result.paths.push_back(argument);
      continue;
    }
    std::string* value = nullptr;
    if (argument == "--after-file") {
      value = &result.hook.file;
    } else if (argument == "--after-function") {
      value = &result.hook.function;
    } else if (argument == "--for-each") {
      value = &result.hook.type;
    }
    if (!value || !value->empty() || ++i == arguments.size() || arguments[i].empty() ||
        arguments[i].starts_with("--")) {
      throw std::invalid_argument("invalid managed update option: " + argument);
    }
    *value = arguments[i];
  }
  if (result.paths.empty() || !all_or_no_hook(result.hook) ||
      (!result.hook.file.empty() && !function_name(result.hook.function))) {
    throw std::invalid_argument("usage: %live update FILE [FILE ...] "
                                "[--after-file FILE --after-function FUNCTION --for-each TYPE]");
  }
  return result;
}

Revision::Revision(Registry& registry, std::uint64_t generation)
    : registry_(registry)
    , generation_(generation) {}

Revision::~Revision() {
  // Keep access gated and the possibly installed image pinned even if the
  // common owner disconnects without performing recovery.
  std::lock_guard lock(registry_.mutex_);
  if (registry_.sealed_by_ == this) {
    registry_.sealed_by_ = nullptr;
  }
}

std::unique_ptr<Revision> stage(Registry& registry, const JitRequest& request,
                                const HookOptions& hook) {
  if (!all_or_no_hook(hook) || (!hook.file.empty() && !function_name(hook.function))) {
    throw std::invalid_argument("incomplete or invalid managed hook options");
  }
  const auto generation = registry.generation();
  auto migration = request;
  // Require exact exported lifecycle symbols in the shared ORC image.
  if (migration.imports.contains(std::string(RecipeSymbol)) ||
      migration.imports.contains(std::string(HookSymbol))) {
    throw std::invalid_argument(
        "managed lifecycle symbols must be defined by the submitted source");
  }
  if (std::find(migration.entry_symbols.begin(), migration.entry_symbols.end(), RecipeSymbol) ==
      migration.entry_symbols.end()) {
    migration.entry_symbols.emplace_back(RecipeSymbol);
  }
  const auto in_hook = [&](const auto& file) { return file.path == hook.file; };
  if (!hook.file.empty()) {
    if (std::none_of(migration.files.begin(), migration.files.end(), in_hook)) {
      throw std::invalid_argument("managed hook source snapshot is missing");
    }
    std::erase_if(migration.files, in_hook);
    std::erase_if(migration.translation_units, in_hook);
  }
  if (migration.translation_units.empty() && migration.files.size() == 1) {
    const auto& source = migration.files.front();
    const auto path = std::filesystem::path(source.path);
    const auto extension = path.extension();
    if (extension == ".h" || extension == ".hpp" || extension == ".hh" || extension == ".hxx") {
      const auto unit = (path.parent_path() / ".clang-tracer-managed-update.cpp").string();
      migration.translation_units = {
          {unit, path.parent_path().string(), request.compiler_arguments}};
      const auto wrapper = "#include " + include_quoted(source.path) + "\n";
      migration.files.push_back({unit, wrapper});
    }
  }
  auto image = compile_in_target(migration);
  using GetRecipe = const Recipe* (*)();
  const auto getter = reinterpret_cast<GetRecipe>(image->lookup(RecipeSymbol));
  const auto* recipe = getter ? getter() : nullptr;
  if (!recipe || !recipe->changes || !recipe->count) {
    throw RebuildRequired("Rebuild required: no managed migration recipe");
  }

  auto revision = std::unique_ptr<Revision>(new Revision(registry, generation));
  revision->image_ = image;
  revision->has_native_code_ =
      std::any_of(request.entry_symbols.begin(), request.entry_symbols.end(),
                  [](const auto& name) { return name != RecipeSymbol; });
  std::set<std::string_view> selected;
  std::string hook_cpp_type;
  std::size_t hook_size = 0;
  std::size_t hook_alignment = 0;
  for (std::size_t i = 0; i != recipe->count; ++i) {
    const auto& change = recipe->changes[i];
    if (!change.type || !*change.type || !selected.emplace(change.type).second ||
        !change.previous.id || !change.previous.cpp_type || !change.forward) {
      throw std::invalid_argument("invalid or duplicate managed migration recipe entry");
    }
    revision->add_change(change, image);
    if (hook.type == change.type) {
      hook_cpp_type = revision->types_.back().after->cpp_type;
      hook_size = change.next.layout.size;
      hook_alignment = change.next.layout.alignment;
    }
  }
  if (hook.file.empty()) {
    return revision;
  }
  if (hook_cpp_type.empty()) {
    throw std::invalid_argument("--for-each does not name a type in the managed migration");
  }

  auto hook_request = request;
  const auto profile =
      std::find_if(request.translation_units.begin(), request.translation_units.end(), in_hook);
  const auto file = std::filesystem::path(hook.file);
  const auto wrapper = (file.parent_path() / ".clang-tracer-managed-after.cpp").string();
  const auto directory =
      profile == request.translation_units.end() ? file.parent_path().string() : profile->directory;
  const auto& arguments = profile == request.translation_units.end() ? request.compiler_arguments
                                                                     : profile->compiler_arguments;
  hook_request.translation_units = {{wrapper, directory, arguments}};
  hook_request.entry_symbols = {std::string(HookSymbol)};
  const std::string hook_body =
      "#include " + include_quoted(hook.file) +
      "\n"
      "#include <tracer/live_migration.hpp>\n"
      "namespace managed = tracer::live::managed;\n"
      "extern \"C\" bool " +
      std::string(HookSymbol) +
      "(void *p, managed::Diagnostic &error) noexcept {\n"
      "  using State = " +
      hook_cpp_type +
      ";\n"
      "  static_assert(sizeof(State) == " +
      std::to_string(hook_size) + " && alignof(State) == " + std::to_string(hook_alignment) +
      ");\n"
      "  return managed::checked(error, [&] {\n"
      "    return [&]<class T>(T *) {\n"
      "      if constexpr (requires { static_cast<void (*)(T &)>(&" +
      hook.function +
      "); }) {\n"
      "        static_cast<void (*)(T &)>(&" +
      hook.function +
      ")(*static_cast<T *>(p));\n"
      "        return true;\n"
      "      } else {\n"
      "        return static_cast<bool (*)(T &)>(&" +
      hook.function +
      ")(*static_cast<T *>(p)) || error.fail(\"managed hook rejected state\");\n"
      "      }\n"
      "    }(static_cast<State *>(p));\n"
      "  });\n"
      "}\n";
  hook_request.files.push_back({wrapper, hook_body});
  auto hook_image = compile_in_target(hook_request);
  revision->hook_ = reinterpret_cast<Revision::Hook>(hook_image->lookup(HookSymbol));
  if (!revision->hook_) {
    throw std::runtime_error("managed after hook did not materialize");
  }
  revision->hook_image_ = std::move(hook_image);
  revision->hook_type_ = hook.type;
  return revision;
}

void Revision::add_change(const Change& change, const std::shared_ptr<void>& image) {
  auto after = Registry::bind(change.next, image);
  if (after->layout_id == change.previous.id) {
    throw std::invalid_argument("managed migration requires a new layout identifier");
  }
  types_.push_back({change.type,
                    {change.previous.id, change.previous.cpp_type, change.previous.size,
                     change.previous.alignment},
                    {},
                    std::move(after),
                    change.forward,
                    change.reverse});
}

void Revision::prepare(bool reverse) {
  pending_.reserve(types_.size());
  std::set<const void*> storage_addresses;
  for (const auto& type : types_) {
    auto found = registry_.types_.find(type.name);
    if (found == registry_.types_.end()) {
      throw RebuildRequired("Rebuild required: application has no managed storage for " +
                            type.name);
    }
    auto& live = found->second;
    if ((!reverse && !same_layout(type.previous, *live.binding)) ||
        (reverse && live.binding != type.after)) {
      throw StaleLayout("stale managed input layout for " + type.name);
    }
    for (const auto& [id, instance] : live.instances) {
      (void)id;
      storage_addresses.insert(instance.storage.get());
    }
  }
  for (auto& type : types_) {
    auto& live = registry_.types_.at(type.name);
    if (!reverse) {
      type.before = live.binding;
    }
    auto& pending = pending_.emplace_back(Pending{&live, {reverse ? type.before : type.after, {}}});
    const auto convert = reverse ? type.reverse : type.forward;
    for (const auto& [id, instance] : live.instances) {
      State state;
      Diagnostic error;
      if (!convert(instance.storage.get(), state, error)) {
        throw std::runtime_error(type.name + " instance " + std::to_string(id) + ": " +
                                 (error.message[0] ? error.message : "migration failed"));
      }
      if (!Registry::valid_state(state, *pending.staged.binding) ||
          !storage_addresses.insert(state.get()).second) {
        throw std::runtime_error("migration must construct separate owned state for " + type.name);
      }
      pending.staged.instances.emplace(
          id, Registry::Instance{pending.staged.binding, type.after->image, std::move(state)});
    }
  }
}

void Revision::swap_states() noexcept {
  for (auto& pending : pending_) {
    pending.live->binding.swap(pending.staged.binding);
    pending.live->instances.swap(pending.staged.instances);
  }
}

void Revision::release_gate() noexcept {
  std::lock_guard<std::mutex> lock(registry_.mutex_);
  registry_.owner_ = {};
  registry_.gate_ = Registry::Gate::open;
  registry_.changed_.notify_all();
}

Result Revision::seal(std::string detail) {
  std::lock_guard<std::mutex> lock(registry_.mutex_);
  registry_.sealed_by_ = this;
  registry_.sealed_image_ = image_;
  registry_.owner_ = {};
  registry_.gate_ = Registry::Gate::sealed;
  return {Status::restoration_required, std::move(detail)};
}

Result Revision::apply(Deadline deadline, const Publication& code) {
  return transition(false, deadline, code);
}

Result Revision::revert(Deadline deadline, const Publication& code) {
  return transition(true, deadline, code);
}

Result Revision::transition(bool reverse, Deadline deadline, const Publication& code) {
  auto operation = std::unique_lock(operation_, std::try_to_lock);
  if (!operation.owns_lock()) {
    return {Status::blocked, "managed revision already in use"};
  }
  if (static_cast<bool>(code.install) != static_cast<bool>(code.restore)) {
    return {Status::blocked, "managed code publication requires an exact restore action"};
  }
  if ((!reverse && used_) || (reverse && !applied_)) {
    return {Status::stale, "managed revision is no longer eligible"};
  }
  if ((!reverse && has_native_code_ && !code.install) ||
      (reverse && has_native_code_ != static_cast<bool>(code.install))) {
    return {Status::blocked, "managed revision requires its coordinated native publication"};
  }
  if (reverse &&
      std::any_of(types_.begin(), types_.end(), [](const auto& type) { return !type.reverse; })) {
    return {Status::blocked, "managed revert requires a reverse transform for every affected type"};
  }
  {
    std::unique_lock lock(registry_.mutex_);
    if (registry_.gate_ == Registry::Gate::sealed) {
      return {Status::restoration_required, "managed group remains gated for code recovery"};
    }
    if (registry_.gate_ != Registry::Gate::open || registry_.owner_ == std::this_thread::get_id()) {
      return {Status::blocked, "managed group cannot enter its safe point"};
    }
    if ((!reverse && registry_.generation() != generation_) ||
        registry_.generation() == std::numeric_limits<std::uint64_t>::max()) {
      return {Status::stale, "managed revision changed after staging or generation exhausted"};
    }
    registry_.gate_ = Registry::Gate::draining;
    if (!registry_.changed_.wait_until(lock, deadline,
                                       [&] { return registry_.owner_ == std::thread::id{}; }) ||
        Registry::Clock::now() >= deadline) {
      registry_.gate_ = Registry::Gate::open;
      registry_.changed_.notify_all();
      return {Status::blocked, "managed safe point timed out"};
    }
    registry_.gate_ = Registry::Gate::exclusive;
    registry_.owner_ = std::this_thread::get_id();
  }

  bool code_installed = false;
  bool states_installed = false;
  std::size_t completed = 0;
  try {
    prepare(reverse);
    if (Registry::Clock::now() >= deadline) {
      throw std::runtime_error("managed safe point timed out before publication");
    }
    if (code.install) {
      auto result = publish(code.install);
      if (result.state == CodeState::restoration_required) {
        return seal(std::move(result.detail));
      }
      if (result.state == CodeState::stale) {
        throw StaleLayout(result.detail);
      }
      if (result.state == CodeState::unchanged) {
        throw std::runtime_error(result.detail);
      }
      code_installed = true;
    }
    swap_states();
    states_installed = true;
    for (std::size_t i = 0; i != pending_.size(); ++i) {
      const auto& name = types_[i].name;
      const auto& live = *pending_[i].live;
      for (const auto& [id, instance] : live.instances) {
        try {
          if (Registry::Clock::now() >= deadline) {
            throw std::runtime_error("managed safe point timed out");
          }
          Diagnostic diagnostic;
          if (!reverse && hook_ && hook_type_ == name &&
              !hook_(instance.storage.get(), diagnostic)) {
            throw std::runtime_error(diagnostic.message[0] ? diagnostic.message
                                                           : "managed hook failed");
          }
          const auto check = instance.binding->validate;
          if (check && !check(instance.storage.get(), diagnostic)) {
            throw std::runtime_error(diagnostic.message[0] ? diagnostic.message
                                                           : "migrated state failed validation");
          }
        } catch (...) {
          throw std::runtime_error(name + " instance " + std::to_string(id) + ": " +
                                   current_error());
        }
        ++completed;
      }
    }
    if (Registry::Clock::now() >= deadline) {
      throw std::runtime_error("managed safe point timed out before commit");
    }
    ++registry_.generation_;
    applied_ = !reverse;
    has_native_code_ = static_cast<bool>(code.install);
    used_ = true;
    pending_.clear();
    release_gate();
    return {Status::applied, {}, completed};
  } catch (...) {
    // Put back the exact old storage and factory before reopening access.
    if (states_installed) {
      swap_states();
    }
    if (code_installed) {
      auto restored = publish(code.restore);
      if (restored.state != CodeState::installed) {
        return seal(std::move(restored.detail));
      }
    }
    pending_.clear();
    release_gate();
    try {
      throw;
    } catch (const StaleLayout& error) {
      return {Status::stale, error.what()};
    } catch (const RebuildRequired& error) {
      return {Status::rebuild_required, error.what()};
    } catch (...) {
      return {Status::blocked, current_error()};
    }
  }
}

Result Revision::recover(const CodeAction& restore_code) {
  auto operation = std::unique_lock(operation_, std::try_to_lock);
  if (!operation.owns_lock()) {
    return {Status::blocked, "managed revision already in use"};
  }
  {
    std::lock_guard lock(registry_.mutex_);
    if (registry_.sealed_by_ != this) {
      return {Status::blocked, "no uncertain managed publication to recover"};
    }
    if (!restore_code) {
      return {Status::restoration_required, "managed code recovery action is missing"};
    }
    registry_.owner_ = std::this_thread::get_id();
  }
  auto result = publish(restore_code);
  if (result.state != CodeState::installed) {
    return seal(std::move(result.detail));
  }
  pending_.clear();
  {
    std::lock_guard lock(registry_.mutex_);
    registry_.sealed_by_ = nullptr;
    registry_.sealed_image_.reset();
  }
  release_gate();
  return {Status::applied, {}, 0};
}

} // namespace tracer::live::managed
