#include "repl_capture_install.hpp"

#include <stdexcept>

namespace capi {

Calls calls;

#define CAPI_DEFINE(name, type) \
  __attribute__((noinline)) int use_##name(const type &, const Sibling &) { \
    ++calls.name;                                                              \
    return 1;                                                                  \
  }
CAPI_CASES(CAPI_DEFINE)
#undef CAPI_DEFINE

// A class that no header declares: a callback that reads its members makes the agent rebuild
// the class from the target's debug data and import it. Plain is a member so that the rebuilt
// class declares it before the method that takes it.
struct Plain {
  int a = 1;
  int b = 2;
};
struct Holder {
  Plain keep;
  int total = 3;
  __attribute__((noinline)) int use(const Plain &value) {
    ++calls.receiver;
    return total + value.a;
  }
};

void run_once() {
  const Sibling sibling;
  const ThreadId thread_id;
  const ErrorCode error_code;
  const ExceptionPtr empty;
  ExceptionPtr set;
  set.error = std::make_exception_ptr(std::runtime_error("boom"));
  const Nested nested;
  const AnonStruct anon_struct;
  const AnonGroup anon_group;
  const Owner owner;
  const Local local;
  use_thread_id(thread_id, sibling);
  use_error_code(error_code, sibling);
  use_exception_ptr_empty(empty, sibling);
  use_exception_ptr_set(set, sibling);
  use_nested(nested, sibling);
  use_anon_struct(anon_struct, sibling);
  use_anon_group(anon_group, sibling);
  use_contain_private(owner, sibling);
  use_contain_anonymous(local, sibling);
  Holder().use(Plain());
}

} // namespace capi
