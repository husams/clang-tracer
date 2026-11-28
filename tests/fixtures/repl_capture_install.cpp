#include "repl_capture_install.hpp"

#include <stdexcept>

namespace capi {

Calls calls;

__attribute__((noinline)) int use_all(const ThreadId &, const ErrorCode &,
                                      const ExceptionPtr &, const ExceptionPtr &,
                                      const Nested &, const AnonStruct &, const AnonGroup &,
                                      const Sibling &) {
  ++calls.all;
  return 1;
}

__attribute__((noinline)) int use_contained(const Owner &, const Local &, const Sibling &) {
  ++calls.contained;
  return 1;
}

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
  use_all(thread_id, error_code, empty, set, nested, anon_struct, anon_group, sibling);
  use_contained(Owner(), Local(), sibling);
  Holder().use(Plain());
}

} // namespace capi
