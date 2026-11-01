#ifndef CLANG_TRACER_CONTEXT_HPP
#define CLANG_TRACER_CONTEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tracer {
class Call;
namespace detail {
struct ScopeInitAccess;
}

template <class Scope> struct state {
  using scope_type = Scope;
};

template <class Scope> struct shared_state {
  using scope_type = Scope;
};

template <class T> struct State {};

// Declaration surface for the engine's declaration pass. Definitions are
// supplied only by admitted generated adapters, never ambient JIT symbols.
// Activation validates each generated adapter's owner, support and unwind
// contracts before making the callback live.
class ScopeInit {
public:
  const Call &root_call() const;

private:
  friend struct detail::ScopeInitAccess;
  explicit ScopeInit(const Call &call) noexcept : root_call_(&call) {}
  const Call *root_call_;
};

class Context {
public:
  template <class T> T &get() const;
  template <class Scope, class Payload> Payload &get() const;
  template <class T, class Visitor> void with(Visitor &&) const;
  template <class Scope, class Payload, class Visitor>
  void with(Visitor &&) const;
  template <class Scope> void init_scope() const;
  template <class Scope, class Payload> void init_scope(Payload &&) const;
  template <class Scope> void end_scope() const;
  void event(std::string_view name, std::size_t max_items = 100) const;
  std::uint64_t thread_id() const noexcept;
};
} // namespace tracer

using tracer::ScopeInit;
using tracer::shared_state;
using tracer::state;
using tracer::State;

#endif
