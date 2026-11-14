#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace tracer::live::managed {

using InstanceId = std::uint64_t;
using State = std::shared_ptr<void>;

// Exceptions stay on the side of the boundary that made the C++ call.
struct Diagnostic {
  char message[256]{};

  bool fail(const char* reason) noexcept {
    std::size_t i = 0;
    while (i + 1 < sizeof(message) && reason[i]) {
      message[i] = reason[i];
      ++i;
    }
    message[i] = '\0';
    return false;
  }
};

using Factory = bool (*)(const void* seed, State& output, Diagnostic& error) noexcept;
using Transform = bool (*)(const void* state, State& output, Diagnostic& error) noexcept;
using Validate = bool (*)(const void* state, Diagnostic& error) noexcept;

// Application-declared ABI identifier. Change it whenever the concrete
// layout or its interpretation changes, even for a same-size edit.
struct Layout {
  const char* id;
  const char* cpp_type;
  std::size_t size;
  std::size_t alignment;
};

template <class T>
constexpr Layout layout(const char* id, const char* cpp_type) noexcept {
  return {id, cpp_type, sizeof(T), alignof(T)};
}

struct Schema {
  Layout layout;
  Factory create;
  Validate validate = nullptr;
};

struct Change {
  const char* type;
  Layout previous;
  Schema next;
  Transform forward;
  Transform reverse = nullptr;
};

// A trusted update translation unit exports `clang_tracer_managed_update_v1`
// as a C-linkage function returning a pointer to a static Recipe.
struct Recipe {
  const Change* changes;
  std::size_t count;
};

template <class Operation>
bool checked(Diagnostic& error, Operation&& operation) noexcept {
  try {
    return std::forward<Operation>(operation)();
  } catch (const std::exception& ex) {
    return error.fail(ex.what());
  } catch (...) {
    return error.fail("unknown C++ exception");
  }
}

template <class T>
bool create_default(const void*, State& output, Diagnostic& error) noexcept {
  static_assert(std::is_nothrow_destructible_v<T>);
  return checked(error, [&] {
    output = std::make_shared<T>();
    return true;
  });
}

template <class T, class Seed, T (*Make)(const Seed&)>
bool create_from(const void* seed, State& output, Diagnostic& error) noexcept {
  static_assert(std::is_nothrow_destructible_v<T>);
  if (!seed) {
    return error.fail("managed factory requires a seed");
  }
  return checked(error, [&] {
    output = std::make_shared<T>(Make(*static_cast<const Seed*>(seed)));
    return true;
  });
}

template <class T, bool (*Check)(const T&)>
bool validate(const void* value, Diagnostic& error) noexcept {
  return checked(error, [&] { return Check(*static_cast<const T*>(value)); });
}

// Preserve declared compatible fields through C++ assignment. A changed
// field type or a tagged union needs an explicit transform. The destination
// is value-initialized, including its declared member initializers.
template <auto From, auto To>
struct Preserve {
  template <class Old, class New>
  static void copy(const Old& old_state, New& new_state) {
    static_assert(std::is_same_v<decltype((old_state.*From)),
                                 const std::remove_reference_t<decltype((new_state.*To))>&>,
                  "a changed field type requires an explicit transform");
    new_state.*To = old_state.*From;
  }
};

template <class Old, class New, class... Fields>
bool preserve(const void* state, State& output, Diagnostic& error) noexcept {
  static_assert(std::is_nothrow_destructible_v<New>);
  return checked(error, [&] {
    auto result = std::make_shared<New>();
    (Fields::copy(*static_cast<const Old*>(state), *result), ...);
    output = std::move(result);
    return true;
  });
}

template <class Old, class New, New (*Move)(const Old&)>
bool transform(const void* state, State& output, Diagnostic& error) noexcept {
  static_assert(std::is_nothrow_destructible_v<New>);
  return checked(error, [&] {
    output = std::make_shared<New>(Move(*static_cast<const Old*>(state)));
    return true;
  });
}

class Revision;

// All access, allocation and destruction for the managed group must take
// an Access lease. Handles are process-local identities, independent of
// storage addresses. Concrete pointers and references are scoped to `with`.
// The registry must outlive its leases and all associated live revisions.
// Factories and transforms transfer exclusive ownership of separate complete
// objects. The shared pointer only carries the module's typed deleter.
// All state access, including callbacks reached through existing native
// entries, must use the same registry gate.
class Registry {
  struct Binding {
    // Keep the defining JIT image alive until after its states die.
    std::shared_ptr<void> image;
    std::string layout_id;
    std::string cpp_type;
    std::size_t size;
    std::size_t alignment;
    Factory create;
    Validate validate;
  };

  struct Instance {
    std::shared_ptr<const Binding> binding;
    std::shared_ptr<void> transform_image;
    State storage;
  };

  struct Type {
    std::shared_ptr<const Binding> binding;
    std::map<InstanceId, Instance> instances;
  };

public:
  class Access {
  public:
    Access(const Access&) = delete;
    Access& operator=(const Access&) = delete;

    ~Access() { registry_.leave(); }

    void add_type(std::string_view name, const Schema& schema) {
      auto binding = Registry::bind(schema);
      if (name.empty() ||
          !registry_.types_.try_emplace(std::string(name), Type{std::move(binding), {}}).second) {
        throw std::invalid_argument("invalid or duplicate managed type");
      }
    }

    InstanceId create(std::string_view name, const void* seed = nullptr) {
      auto& type = registry_.types_.at(std::string(name));
      if (registry_.last_id_ == std::numeric_limits<InstanceId>::max()) {
        throw std::overflow_error("managed instance identifiers exhausted");
      }
      State state;
      Diagnostic error;
      if (!type.binding->create(seed, state, error) ||
          !Registry::valid_state(state, *type.binding) ||
          (type.binding->validate && !type.binding->validate(state.get(), error))) {
        throw std::runtime_error(error.message[0] ? error.message
                                                  : "managed factory produced invalid state");
      }
      const auto id = ++registry_.last_id_;
      type.instances.emplace(id, Instance{type.binding, {}, std::move(state)});
      return id;
    }

    bool erase(std::string_view name, InstanceId id) {
      if (borrowing_) {
        throw std::logic_error("managed state cannot be erased during a borrow");
      }
      return registry_.types_.at(std::string(name)).instances.erase(id) != 0;
    }

    template <class T, class F>
    void with(std::string_view name, InstanceId id, std::string_view layout_id, F&& visit) {
      static_assert(std::is_void_v<std::invoke_result_t<F, T&>>,
                    "managed access cannot return a reference or value");
      const auto& instance = registry_.types_.at(std::string(name)).instances.at(id);
      const auto& binding = *instance.binding;
      if (binding.layout_id != layout_id || binding.size != sizeof(T) ||
          binding.alignment != alignof(T)) {
        throw std::runtime_error("managed access layout mismatch");
      }

      struct Borrow {
        bool& active;
        const bool previous;

        ~Borrow() { active = previous; }
      } borrow{borrowing_, std::exchange(borrowing_, true)};

      std::forward<F>(visit)(*static_cast<T*>(instance.storage.get()));
    }

  private:
    friend class Registry;

    explicit Access(Registry& registry)
        : registry_(registry) {
      registry_.enter();
    }

    Registry& registry_;
    bool borrowing_ = false;
  };

  [[nodiscard]] Access access() { return Access(*this); }

  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_.load(); }

private:
  friend class Revision;
  using Clock = std::chrono::steady_clock;
  enum class Gate { open, draining, exclusive, sealed };

  static std::shared_ptr<const Binding> bind(const Schema& schema,
                                             std::shared_ptr<void> image = {}) {
    const auto& layout = schema.layout;
    if (!layout.id || !*layout.id || !layout.cpp_type || !*layout.cpp_type || !layout.size ||
        !layout.alignment || (layout.alignment & (layout.alignment - 1)) || !schema.create) {
      throw std::invalid_argument("invalid managed schema");
    }
    return std::make_shared<Binding>(Binding{std::move(image), layout.id, layout.cpp_type,
                                             layout.size, layout.alignment, schema.create,
                                             schema.validate});
  }

  static bool valid_state(const State& state, const Binding& binding) {
    return state && state.use_count() == 1 &&
           reinterpret_cast<std::uintptr_t>(state.get()) % binding.alignment == 0;
  }

  void enter() {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto self = std::this_thread::get_id();
    if (self == owner_) {
      throw std::logic_error("managed access cannot reenter its safe point");
    }
    changed_.wait(lock, [&] { return gate_ == Gate::open && owner_ == std::thread::id{}; });
    owner_ = self;
  }

  void leave() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    owner_ = {};
    changed_.notify_all();
  }

  std::mutex mutex_;
  std::condition_variable changed_;
  Gate gate_ = Gate::open;
  std::thread::id owner_;
  const Revision* sealed_by_ = nullptr;
  std::shared_ptr<void> sealed_image_;
  std::atomic<std::uint64_t> generation_{0};
  std::map<std::string, Type, std::less<>> types_;
  InstanceId last_id_ = 0;
};

} // namespace tracer::live::managed
