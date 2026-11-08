#include "context_facade.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "tracer/compat.hpp"

namespace tracer::context_facade {
namespace {
struct ThreadBindings {
  GenerationBinding current{};
  std::array<GenerationBinding, 32> prior{};
  std::size_t depth = 0;
};
thread_local ThreadBindings thread_bindings{};
tracer::Context stateless_context;

std::string quote(const std::string &value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char c : value) {
    switch (c) {
    case '\\':
      out << "\\\\";
      break;
    case '"':
      out << "\\\"";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (c < 0x20)
        out << "\\x" << std::hex << static_cast<unsigned>(c);
      else
        out << static_cast<char>(c);
    }
  }
  out << '"';
  return out.str();
}

std::string type_name(const std::string &type) {
  static constexpr std::string_view builtins[] = {"void",
                                                  "bool",
                                                  "char",
                                                  "signed char",
                                                  "unsigned char",
                                                  "short",
                                                  "short int",
                                                  "unsigned short",
                                                  "unsigned short int",
                                                  "int",
                                                  "unsigned",
                                                  "unsigned int",
                                                  "long",
                                                  "long int",
                                                  "unsigned long",
                                                  "unsigned long int",
                                                  "long long",
                                                  "long long int",
                                                  "unsigned long long",
                                                  "unsigned long long int",
                                                  "float",
                                                  "double",
                                                  "long double",
                                                  "wchar_t",
                                                  "char8_t",
                                                  "char16_t",
                                                  "char32_t",
                                                  "std::nullptr_t"};
  if (std::find(std::begin(builtins), std::end(builtins), type) !=
      std::end(builtins))
    return type;
  std::string spelling = type;
  // QualType::getAsString() may include elaborated-type keywords, including
  // inside template arguments (for example, `class std::basic_string<...>`).
  // Those are diagnostic spelling, not valid in a type-id. Keep the exact
  // canonical string in metadata, and remove only these tokens for generated
  // C++ trait specializations.
  for (const std::string_view keyword : {"class ", "struct ", "enum "}) {
    std::size_t offset = 0;
    while ((offset = spelling.find(keyword, offset)) != std::string::npos) {
      spelling.erase(offset, keyword.size());
    }
  }
  return ::tracer::compat::starts_with(spelling, "::") ? spelling : "::" + spelling;
}

std::string descriptor_source(const RequestDescriptor &d) {
  std::ostringstream out;
  out << "{ " << d.state_id << "ULL, " << d.scope_id << "ULL, "
      << quote(d.state_type ? d.state_type : "") << ", "
      << quote(d.scope_type ? d.scope_type : "") << ", "
      << quote(d.payload_type ? d.payload_type : "") << ", "
      << (d.generic ? "true" : "false") << ", " << (d.shared ? "true" : "false")
      << " }";
  return out.str();
}
} // namespace

bool valid(const NativeFunctionTable &t) noexcept {
  return t.abi_version == 1 && t.current_context && t.frame_enter &&
         t.frame_exit && t.create_or_attach && t.hook_begin && t.hook_end &&
         t.access_begin && t.access_end && t.get && t.init_scope &&
         t.end_scope && t.event && t.thread_id;
}

std::uint64_t stable_identity(std::string_view lineage,
                              std::string_view canonical_type) noexcept {
  // FNV-1a with an explicit separator prevents concatenation ambiguity.
  std::uint64_t hash = 14695981039346656037ULL;
  auto mix = [&](unsigned char value) {
    hash ^= value;
    hash *= 1099511628211ULL;
  };
  for (const unsigned char c : lineage)
    mix(c);
  mix(0xff);
  for (const unsigned char c : canonical_type)
    mix(c);
  return hash ? hash : 1;
}

const NativeHookContext *active_native_hook() noexcept {
  const auto snapshot = context_execution::current();
  if (!snapshot.current_hook)
    return nullptr;
  const auto *hook =
      static_cast<const NativeHookContext *>(snapshot.current_hook);
  return hook->magic == NativeHookContext::kMagic ? hook : nullptr;
}

const GenerationBinding *active_generation() noexcept {
  if (const auto *hook = active_native_hook())
    if (hook->generation.table && hook->generation.runtime)
      return &hook->generation;
  return generation_binding();
}

void require_valid(const NativeFunctionTable &table) {
  if (!valid(table))
    throw std::invalid_argument("invalid context native function table");
}

void bind_generation(const NativeFunctionTable *table, Opaque runtime) {
  if (!table || !runtime)
    throw std::invalid_argument("invalid context generation binding");
  require_valid(*table);
  if (thread_bindings.depth == thread_bindings.prior.size())
    throw std::overflow_error(
        "context generation binding nesting limit exceeded");
  thread_bindings.prior[thread_bindings.depth++] = thread_bindings.current;
  thread_bindings.current = {table, runtime};
}

void clear_thread_generation() noexcept {
  if (!thread_bindings.depth) {
    thread_bindings.current = {};
    return;
  }
  thread_bindings.current = thread_bindings.prior[--thread_bindings.depth];
  thread_bindings.prior[thread_bindings.depth] = {};
}

const GenerationBinding *generation_binding() noexcept {
  return thread_bindings.current.table && thread_bindings.current.runtime
             ? &thread_bindings.current
             : nullptr;
}

tracer::Context &current_context() noexcept { return stateless_context; }

void emit_event(std::string_view name, std::size_t max_items) noexcept {
  const auto *hook = active_native_hook();
  if (!context_execution::event_allowed(
          context_execution::runtime_diagnostics(), name))
    return;
  if (!hook || !hook->trace_context || !hook->generation.table ||
      !hook->generation.runtime) {
    context_execution::runtime_diagnostics().no_active_hook(name);
    return;
  }
  (void)hook->generation.table->event(hook->generation.runtime,
                                      hook->trace_context, name, max_items);
}

std::uint64_t active_thread_id() noexcept {
  const auto *binding = active_generation();
  return binding && binding->table && binding->runtime
             ? binding->table->thread_id(binding->runtime)
             : 0;
}

} // namespace tracer::context_facade

namespace tracer {} // namespace tracer

namespace tracer::context_facade {

GeneratedContextFacade
generate_context_facade(const std::string &facade_namespace,
                        const std::string &bind_entrypoint,
                        const std::vector<FacadeIdentity> &identities) {
  GeneratedContextFacade result;
  if (facade_namespace.empty() || bind_entrypoint.empty()) {
    result.error =
        "invalid_context_facade: namespace and binding entrypoint are required";
    return result;
  }
  std::unordered_set<StateId> state_ids;
  std::unordered_set<std::string> scope_types;
  std::vector<std::pair<std::string, ScopeId>> scopes;
  result.descriptor_strings.reserve(identities.size() * 3);
  for (const auto &identity : identities) {
    const auto &dep = identity.dependency;
    if (!identity.scope_id || dep.scope_type.empty() ||
        (dep.generic ? (dep.payload_type.empty() || !identity.state_id)
                     : (dep.state_type.empty() || !identity.state_id))) {
      result.error = "invalid_context_facade: incomplete stable identity";
      return result;
    }
    if (!scope_types.insert(dep.scope_type).second) {
      const auto existing =
          std::find_if(scopes.begin(), scopes.end(), [&](const auto &s) {
            return s.first == dep.scope_type;
          });
      if (existing != scopes.end() && existing->second != identity.scope_id) {
        result.error = "invalid_context_facade: conflicting scope identity";
        return result;
      }
    } else {
      scopes.emplace_back(dep.scope_type, identity.scope_id);
    }
    if (!state_ids.insert(identity.state_id).second) {
      result.error = "invalid_context_facade: duplicate state identity";
      return result;
    }
    RequestDescriptor descriptor;
    descriptor.state_id = identity.state_id;
    descriptor.scope_id = identity.scope_id;
    descriptor.generic = dep.generic;
    descriptor.shared = dep.shared;
    descriptor.state_type = dep.state_type.c_str();
    descriptor.scope_type = dep.scope_type.c_str();
    descriptor.payload_type = dep.payload_type.c_str();
    result.descriptor_strings.push_back(dep.state_type);
    result.descriptor_strings.push_back(dep.scope_type);
    result.descriptor_strings.push_back(dep.payload_type);
    descriptor.state_type =
        result.descriptor_strings[result.descriptor_strings.size() - 3].c_str();
    descriptor.scope_type =
        result.descriptor_strings[result.descriptor_strings.size() - 2].c_str();
    descriptor.payload_type =
        result.descriptor_strings[result.descriptor_strings.size() - 1].c_str();
    result.descriptors.push_back(descriptor);
  }

  // Emit only after user declarations so all private trait specializations
  // name complete canonical types. The compilation copy includes this source
  // after user definitions and before generated wrappers.
  auto &out = result.definitions;
  out += "\nnamespace " + facade_namespace +
         " {\n"
         "using namespace ::tracer::context_facade;\n";
  out += "}\nnamespace tracer::context_facade {\n";
  for (const auto &[scope, id] : scopes)
    out += "template<> struct ScopeTraits<" + type_name(scope) +
           "> { static constexpr ::tracer::context_facade::ScopeId id = " +
           std::to_string(id) + "ULL; };\n";
  for (const auto &identity : identities) {
    const auto &d = identity.dependency;
    if (d.generic) {
      out +=
          "template<> struct PayloadTraits<" + type_name(d.scope_type) + ", " +
          type_name(d.payload_type) +
          "> { static constexpr ::tracer::context_facade::RequestDescriptor "
          "descriptor "
          "= " +
          descriptor_source(result.descriptors[&identity - identities.data()]) +
          "; };\n";
    } else {
      out +=
          "template<> struct StateTraits<" + type_name(d.state_type) +
          "> { static constexpr ::tracer::context_facade::RequestDescriptor "
          "descriptor "
          "= " +
          descriptor_source(result.descriptors[&identity - identities.data()]) +
          "; };\n";
    }
  }
  out += "}\nnamespace tracer {\n"
         "template<class T> T& Context::get() const { return *::" +
         facade_namespace +
         "::request<T>(); }\n"
         "template<class Scope, class Payload> Payload& Context::get() const { "
         "return *::" +
         facade_namespace +
         "::request_payload<Scope, Payload>(); }\n"
         "template<class T, class Visitor> void Context::with(Visitor&& "
         "visitor) const { ::" +
         facade_namespace +
         "::with_state<T>(std::forward<Visitor>(visitor)); }\n"
         "template<class Scope, class Payload, class Visitor> void "
         "Context::with(Visitor&& visitor) const { ::" +
         facade_namespace +
         "::with_payload<Scope, Payload>(std::forward<Visitor>(visitor)); }\n"
         "template<class Scope> void Context::init_scope() const { ::" +
         facade_namespace +
         "::init_scope<Scope>(); }\n"
         "template<class Scope, class Payload> void "
         "Context::init_scope(Payload&& payload) const { ::" +
         facade_namespace +
         "::init_scope<Scope>(std::forward<Payload>(payload)); }\n"
         "template<class Scope> void Context::end_scope() const { ::" +
         facade_namespace + "::end_scope<Scope>(); }\n}\n";
  // Member templates for arbitrary visitor types remain header-defined by the
  // private facade and dispatch through the emitted traits above.
  result.bind_entrypoint = bind_entrypoint;
  return result;
}

} // namespace tracer::context_facade
