#include <cerrno>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#include "tracer/callback.hpp"

namespace {
thread_local unsigned suppression = 0;
int constructed = 0, destroyed = 0, failures = 0, originals = 0, posts = 0;
enum class Mode {
  healthy,
  bad_constructor,
  bad_pre,
  bad_post,
  throwing_target
};
thread_local Mode mode = Mode::healthy;

void require(bool value, const char *message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}
} // namespace

extern "C" bool tracer_callback_lifecycle_enter(unsigned long long) noexcept {
  if (suppression) {
    return false;
  }
  ++suppression;
  return true;
}

extern "C" void tracer_callback_cleanup_enter() noexcept { ++suppression; }

extern "C" void tracer_callback_lifecycle_leave() noexcept { --suppression; }

extern "C" void tracer_callback_failure(unsigned long long) noexcept {
  ++failures;
}

struct Interceptor {
  int value = 10;

  Interceptor() {
    require(suppression == 1, "constructor ran without suppression");
    ++constructed;
    errno = EDOM;
    if (mode == Mode::bad_constructor) {
      throw 1;
    }
  }

  ~Interceptor() noexcept {
    if (suppression != 1) {
      std::terminate();
    }
    ++destroyed;
    errno = ERANGE;
  }

  long long pre(const Call &) {
    value += 32;
    if (mode == Mode::bad_pre) {
      throw 2;
    }
    return value;
  }

  long long post(const Call &) {
    ++posts;
    if (mode == Mode::bad_post) {
      throw 3;
    }
    return value;
  }
};

// Exactly the bridge emitted by the production generator, compiled as C++.
#include "callback_fixture.hpp"

void invoke() {
  tracer::TraceContext raw;
  tracer_invocation_instance instance(raw);
  require(errno == EAGAIN, "construction changed target errno");
  bool ran = false;
  try {
    const auto value = tracer_user_dispatch(&raw, &instance, &ran);
    if (ran) {
      require(value == 42, "invocation did not start with fresh member state");
    }
  } catch (int) {
    ++failures;
  }
  require(suppression == 0,
          "lifecycle suppression leaked across original dispatch");
  ++originals;
  if (mode == Mode::throwing_target) {
    throw 73;
  }
  raw.phase = tracer::TracePhase::Post;
  try {
    const auto value = tracer_user_dispatch(&raw, &instance, &ran);
    if (ran) {
      require(value == 42, "PRE and POST did not use the same object");
    }
  } catch (int) {
    ++failures;
  }
}

int main() {
  static_assert(!std::is_copy_constructible_v<tracer_invocation_instance>);
  static_assert(!std::is_move_constructible_v<tracer_invocation_instance>);
  try {
    for (Mode test : {Mode::healthy, Mode::healthy, Mode::bad_constructor,
                      Mode::bad_pre, Mode::bad_post, Mode::throwing_target}) {
      mode = test;
      errno = EAGAIN;
      try {
        invoke();
      } catch (int value) {
        require(test == Mode::throwing_target && value == 73,
                "target exception changed");
      }
      require(suppression == 0 && errno == EAGAIN,
              "cleanup changed target context");
    }
    require(constructed == 6 && destroyed == 5,
            "instance lifetime count differs");
    require(originals == 6 && posts == 3 && failures == 3,
            "failure continuation retried or skipped target/hooks");
    std::cout << "generated callback invocation lifecycle passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
