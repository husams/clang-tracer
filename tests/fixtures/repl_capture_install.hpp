// Argument records that automatic value capture cannot encode (issue #182). Each function takes
// one such record and a record it can encode, and counts its calls.
#pragma once

#include <exception>
#include <string>
#include <system_error>
#include <thread>

namespace capi {

// The record every case passes beside the one that cannot be captured.
struct Sibling {
  int id = 7;
  std::string name = "ada";
};

// Standard-library records without a codec.
struct ThreadId {
  std::thread::id id = std::this_thread::get_id();
  int tail = 5;
};
struct ErrorCode {
  std::error_code code = std::make_error_code(std::errc::invalid_argument);
};
struct ExceptionPtr {
  std::exception_ptr error;
};
struct Middle {
  std::thread::id id = std::this_thread::get_id();
};
struct Nested {
  int head = 1;
  Middle middle;
};

// Members declared by the record that has them: nothing else can name their types.
#ifdef CAPI_SKEW
#define CAPI_PAD char pad[24] = {};
#else
#define CAPI_PAD
#endif
struct AnonStruct {
  struct {
    int x = 1;
  } inner;
  int tail = 3;
  CAPI_PAD
};
struct AnonGroup {
  struct {
    int x = 1;
  } a, b[2];
  int tail = 9;
};

// Types the schema accepts and the generated codec cannot spell.
class Owner {
  struct Secret {
    int v = 4;
  };

public:
  Secret secret;
  int tail = 6;
  CAPI_PAD
};
namespace {
struct Hidden {
  int v = 5;
};
} // namespace
struct Local {
  Hidden hidden;
  int tail = 8;
};

#define CAPI_CASES(X)                                                                     \
  X(thread_id, ThreadId) X(error_code, ErrorCode) X(exception_ptr_empty, ExceptionPtr) \
  X(exception_ptr_set, ExceptionPtr) X(nested, Nested) X(anon_struct, AnonStruct)       \
  X(anon_group, AnonGroup) X(contain_private, Owner) X(contain_anonymous, Local)

#define CAPI_DECLARE(name, type) int use_##name(const type &, const Sibling &);
CAPI_CASES(CAPI_DECLARE)
#undef CAPI_DECLARE

// How often each function ran.
struct Calls {
#define CAPI_COUNTER(name, type) int name = 0;
  CAPI_CASES(CAPI_COUNTER)
#undef CAPI_COUNTER
  int receiver = 0; // Holder::use, a class that only the fixture source declares
};
extern Calls calls;

// Calls every function once.
void run_once();

} // namespace capi
