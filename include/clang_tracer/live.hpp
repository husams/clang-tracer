#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>
#include <type_traits>

namespace tracer::live::data { class TargetImage; }

namespace clang_tracer {
namespace live_detail {
template <class T>
constexpr std::string_view type_name() {
  constexpr std::string_view signature = __PRETTY_FUNCTION__;
  constexpr std::string_view prefix = "T = ";
  const auto begin = signature.find(prefix) + prefix.size();
  return signature.substr(begin,
                          std::min(signature.find(';', begin), signature.rfind(']')) - begin);
}

struct ObjectView {
  std::string_view type;
  std::string_view root;
  void* object = nullptr;
  const std::shared_ptr<void>* owner = nullptr;
  const char* skip_reason = nullptr;
  std::size_t size = 0;
  std::size_t alignment = 0;
  bool scoped = false;
};
} // namespace live_detail

// Each registration runs synchronously. Copy shared storage under the
// application's synchronization, then call add after releasing any locks
// the update will take. The pointee must live for as long as that ownership
// is held, including for an alias or cast. The update must use synchronized
// application APIs or run under an application guard covering its writes.
class ObjectSink {
public:
  using Register = void (*)(void*, const live_detail::ObjectView&);

  ObjectSink(void* context, Register emit)
      : context_(context)
      , emit_(emit) {}

  ObjectSink(const ObjectSink&) = delete;
  ObjectSink& operator=(const ObjectSink&) = delete;

  template <class T>
  void add(const std::shared_ptr<T>& owner, std::string_view root = {}) {
    auto object = view<T>(root);
    // An alias from an empty shared_ptr can have a non-null get().
    if (!owner || !owner.use_count()) {
      object.skip_reason = "shared owner is empty";
    } else {
      const std::shared_ptr<void> hold(owner);
      object.object = hold.get();
      object.owner = &hold;
      emit_(context_, object);
      return;
    }
    emit_(context_, object);
  }

  template <class T>
  void add(const std::weak_ptr<T>& owner, std::string_view root = {}) {
    if (auto hold = owner.lock()) {
      add(hold, root);
    } else {
      auto object = view<T>(root);
      object.skip_reason = "weak owner expired";
      emit_(context_, object);
    }
  }

  // guard must protect this object's lifetime and mutations for the
  // entire call. Lifetime and lock scope end when this registration returns.
  template <class T, class Mutex>
  void borrow(T& value, const std::unique_lock<Mutex>& guard, std::string_view root = {}) {
    auto object = view<T>(root);
    if (guard.owns_lock()) {
      object.object = std::addressof(value);
      object.scoped = true;
    } else {
      object.skip_reason = "borrow guard is unlocked";
    }
    emit_(context_, object);
  }

  template <class T>
  void skip(std::string_view root, const char* reason = "lifetime unverified") {
    auto object = view<T>(root);
    object.skip_reason = reason;
    emit_(context_, object);
  }

private:
  friend class tracer::live::data::TargetImage;

  void register_verified(const live_detail::ObjectView& value) {
    emit_(context_, value);
  }

  template <class T>
  static live_detail::ObjectView view(std::string_view root) {
    static_assert(std::is_class_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>,
                  "discovery requires a mutable concrete class view");
    return {
        live_detail::type_name<T>(), root, nullptr, nullptr, nullptr, sizeof(T), alignof(T), false};
  }

  void* context_;
  Register emit_;
};
} // namespace clang_tracer
