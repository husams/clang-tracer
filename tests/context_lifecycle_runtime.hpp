#pragma once

// Native test owner. Production admission still requires a privately bound,
// validated owner/support generation; this fixture does not enable that path.
#include "context_engine.hpp"

#include <map>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace engine {
namespace ce = tracer::context_engine;
inline thread_local ce::Context *installed = nullptr;
inline ce::Context &current_context() {
  if (!installed)
    throw ce::Error(ce::ErrorCode::invalid_frame);
  return *installed;
}
class CurrentContext final {
public:
  explicit CurrentContext(ce::Context &context) noexcept
      : previous_(std::exchange(installed, &context)) {}
  ~CurrentContext() noexcept { installed = previous_; }
  CurrentContext(const CurrentContext &) = delete;
  CurrentContext &operator=(const CurrentContext &) = delete;
  CurrentContext(CurrentContext &&) = delete;
  CurrentContext &operator=(CurrentContext &&) = delete;

private:
  ce::Context *previous_;
};
inline ce::Identity next_type_identity = 1;
template <class T> ce::Identity type_identity() {
  static const auto identity = next_type_identity++;
  return identity;
}
template <class T> struct LifecycleCounts {
  inline static unsigned made = 0, destroyed = 0;
};
inline std::map<std::type_index, std::shared_ptr<const ce::ScopeRegistration>>
    registrations;

template <class T> ce::StateRegistration state_registration() {
  static_assert(std::is_nothrow_destructible_v<T>);
  return {type_identity<T>(), ce::Policy::shared,
          [](const ce::ScopeInit &, std::any &payload) noexcept {
            try {
              payload = std::make_shared<T>();
              ++LifecycleCounts<T>::made;
              return ce::ErrorCode::ok;
            } catch (...) {
              return ce::ErrorCode::construction_failed;
            }
          },
          [](std::any &payload) noexcept {
            if (payload.has_value()) {
              payload.reset();
              ++LifecycleCounts<T>::destroyed;
            }
          },
          [](std::any &payload) noexcept -> void * {
            auto *value = std::any_cast<std::shared_ptr<T>>(&payload);
            return value ? value->get() : nullptr;
          }};
}
template <class Scope, class... T> void register_scope() {
  auto registration = std::make_shared<ce::ScopeRegistration>();
  registration->identity = type_identity<Scope>();
  registration->owner_code = std::make_shared<int>(1);
  registration->states = {state_registration<T>()...};
  registrations[std::type_index(typeid(Scope))] = std::move(registration);
}
template <class T> ce::SlotRequest request() {
  return {registrations.at(std::type_index(typeid(typename T::scope_type))),
          type_identity<T>()};
}
class InvocationFrame final {
public:
  explicit InvocationFrame(ce::Context &context) : frame_(context) {}
  template <class T> void push_create_or_attach() {
    frame_.create_or_attach(request<T>().registration);
  }
  void pop_all_and_restore() noexcept { frame_.pop_all_and_restore(); }

private:
  ce::InvocationFrame frame_;
};
template <class T, class F> void inspect(ce::Context &context, F &&function) {
  const auto slot = request<T>();
  ce::HookExit exit(context);
  ce::HookAccess access(context, std::span(&slot, 1));
  function(*static_cast<T *>(access.ref(slot)));
}
} // namespace engine
