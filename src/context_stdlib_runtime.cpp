// Compiled into a separately sealed support image, never the native engine.
// It retains the toolchain's ordinary standard object layouts/default
// allocator types; only private runtime entry-point binding is changed.
#include <any>
#include <exception>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

#include <pthread.h>
#include <unwind.h>

#include "context_stdlib_abi.hpp"

#ifndef __GLIBCXX__
#error This sealed image requires the matching libstdc++ toolchain profile
#endif
#ifndef TRACER_CONTEXT_ARCHIVE_SHA256
#define TRACER_CONTEXT_ARCHIVE_SHA256 "unpackaged"
#endif

namespace {
tracer::context_support::Kernel kernel;
bool installed = false;
[[noreturn]] void fail(unsigned reason) noexcept {
  if (installed)
    kernel.panic(kernel.domain, reason);
  __builtin_trap(); // The loader rejects images requiring pre-init execution.
}
void *allocate(std::size_t size, std::size_t alignment) noexcept {
  if (!installed)
    return nullptr;
  return kernel.allocate(kernel.domain, size ? size : 1, alignment);
}
void release(void *pointer, std::size_t size = 0,
             std::size_t alignment = 0) noexcept {
  if (pointer &&
      (!installed || kernel.release(kernel.domain, pointer, size, alignment)))
    fail(2);
}
constexpr std::size_t exception_header_bytes = 128;
constexpr std::size_t dependent_exception_bytes = 112;
static_assert(sizeof(void *) == 8,
              "only the checked x86_64 Itanium profile is admitted");
} // namespace

#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE <= 8
// These archives use weak function addresses as the gthread-active predicate.
// Keep it true so shared ownership remains atomic, while binding the predicate
// inside the sealed image. TLS/exception globals use the installed kernel;
// creating pthread keys is not an admitted private-runtime operation.
extern "C" int __pthread_key_create(pthread_key_t*, void (*)(void*)) noexcept {
  fail(11);
}

extern "C" int __clang_tracer_unwind___pthread_key_create(pthread_key_t*,
                                                          void (*)(void*)) noexcept {
  fail(11);
}
#endif

extern "C" __attribute__((visibility("default")))
const tracer::context_support::Profile *
clangtracer_stdlib_profile_v1() noexcept {
  static constexpr tracer::context_support::Profile profile{
      tracer::context_support::kKernelAbi,
      __GLIBCXX__,
      _GLIBCXX_RELEASE,
      _GLIBCXX_USE_DUAL_ABI,
      _GLIBCXX_USE_CXX11_ABI,
      sizeof(void *),
      sizeof(wchar_t),
      sizeof(std::any),
      alignof(std::any),
      sizeof(std::shared_ptr<void>),
      alignof(std::shared_ptr<void>),
      exception_header_bytes,
      dependent_exception_bytes,
      TRACER_CONTEXT_ARCHIVE_SHA256};
  return &profile;
}
extern "C" __attribute__((visibility("default"))) bool
clangtracer_stdlib_install_v1(
    const tracer::context_support::Kernel *value) noexcept {
  if (installed || !value ||
      value->abi != tracer::context_support::kKernelAbi ||
      value->bytes != sizeof(*value) || !value->domain || !value->allocate ||
      !value->release || !value->allocation_size ||
      !value->exception_allocate || !value->exception_release ||
      !value->exception_globals || !value->error_location || !value->panic ||
      !value->owns_exception || !value->private_unwind ||
      !value->set_private_unwind || !value->find_fde)
    return false;
  for (auto pointer : value->unwind)
    if (!pointer)
      return false;
  kernel = *value;
  installed = true;
  return true;
}

// Executed before publishing the image: this exercises its own allocator,
// erased manager, RTTI and exception path, including an allocation failure.
extern "C" __attribute__((visibility("default"))) unsigned
clangtracer_stdlib_probe_v1(std::any *erased, unsigned *destroyed) noexcept {
  struct Probe {
    unsigned *destroyed;
    std::string text = std::string(1024, 'x');
    explicit Probe(unsigned *value) : destroyed(value) {}
    ~Probe() noexcept { ++*destroyed; }
  };
  try {
    std::vector<std::string> values(4, std::string(128, 'a'));
    std::map<int, std::string> tree;
    tree.emplace(1, values.front());
    if (tree.at(1).size() != 128)
      return 1;
    try {
      throw std::runtime_error("private exception");
    } catch (const std::runtime_error &) {
      auto saved = std::current_exception();
      try {
        std::rethrow_exception(saved);
      } catch (const std::runtime_error &) {
      }
    }
    bool failed = false;
    try {
      auto *large = ::operator new(64 * 1024 * 1024 + 1);
      ::operator delete(large);
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    if (!failed)
      return 2;
    *erased = std::make_shared<Probe>(destroyed);
    return 0;
  } catch (...) {
    return 3;
  }
}

extern "C" void *malloc(std::size_t bytes) noexcept {
  return allocate(bytes, alignof(std::max_align_t));
}
extern "C" void free(void *pointer) noexcept { release(pointer); }
extern "C" void *memcpy(void *to, const void *from,
                        std::size_t bytes) noexcept {
  auto *destination = static_cast<volatile unsigned char *>(to);
  auto *source = static_cast<const volatile unsigned char *>(from);
  for (std::size_t i = 0; i < bytes; ++i)
    destination[i] = source[i];
  return to;
}
extern "C" void *memmove(void *to, const void *from,
                         std::size_t bytes) noexcept {
  auto destination_address = reinterpret_cast<std::uintptr_t>(to);
  auto source_address = reinterpret_cast<std::uintptr_t>(from);
  if (destination_address <= source_address ||
      destination_address - source_address >= bytes)
    return memcpy(to, from, bytes);
  auto *destination = static_cast<volatile unsigned char *>(to);
  auto *source = static_cast<const volatile unsigned char *>(from);
  while (bytes) {
    --bytes;
    destination[bytes] = source[bytes];
  }
  return to;
}
extern "C" void *memset(void *to, int value, std::size_t bytes) noexcept {
  auto *destination = static_cast<volatile unsigned char *>(to);
  for (std::size_t i = 0; i < bytes; ++i)
    destination[i] = static_cast<unsigned char>(value);
  return to;
}
extern "C" int memcmp(const void *a, const void *b,
                      std::size_t bytes) noexcept {
  auto *left = static_cast<const volatile unsigned char *>(a);
  auto *right = static_cast<const volatile unsigned char *>(b);
  for (std::size_t i = 0; i < bytes; ++i)
    if (left[i] != right[i])
      return left[i] < right[i] ? -1 : 1;
  return 0;
}
extern "C" int bcmp(const void *a, const void *b, std::size_t n) noexcept {
  return memcmp(a, b, n);
}
extern "C" std::size_t strlen(const char *text) noexcept {
  auto *s = static_cast<const volatile char *>(text);
  std::size_t n = 0;
  while (s[n])
    ++n;
  return n;
}
extern "C" int strcmp(const char *a, const char *b) noexcept {
  for (std::size_t i = 0;; ++i) {
    const auto left = static_cast<unsigned char>(a[i]),
               right = static_cast<unsigned char>(b[i]);
    if (left != right)
      return left < right ? -1 : 1;
    if (!left)
      return 0;
  }
}
extern "C" int strncmp(const char *a, const char *b, std::size_t n) noexcept {
  for (std::size_t i = 0; i < n; ++i) {
    const auto left = static_cast<unsigned char>(a[i]),
               right = static_cast<unsigned char>(b[i]);
    if (left != right)
      return left < right ? -1 : 1;
    if (!left)
      break;
  }
  return 0;
}
extern "C" void *memchr(const void *p, int value, std::size_t n) noexcept {
  auto *s = static_cast<const unsigned char *>(p);
  for (std::size_t i = 0; i < n; ++i)
    if (s[i] == static_cast<unsigned char>(value))
      return const_cast<unsigned char *>(s + i);
  return nullptr;
}
extern "C" char *strchr(const char *p, int value) noexcept {
  for (;; ++p) {
    if (*p == static_cast<char>(value))
      return const_cast<char *>(p);
    if (!*p)
      return nullptr;
  }
}
extern "C" void *realloc(void *old, std::size_t n) noexcept {
  if (!old)
    return malloc(n);
  if (!n) {
    free(old);
    return nullptr;
  }
  if (!installed)
    fail(1);
  const auto old_size = kernel.allocation_size(kernel.domain, old);
  if (old_size == static_cast<std::size_t>(-1))
    fail(2);
  void *next = malloc(n);
  if (!next)
    return nullptr;
  memcpy(next, old, old_size < n ? old_size : n);
  free(old);
  return next;
}
extern "C" void *calloc(std::size_t count, std::size_t bytes) noexcept {
  if (bytes && count > static_cast<std::size_t>(-1) / bytes)
    return nullptr;
  const auto n = count * bytes;
  auto *p = malloc(n);
  if (p)
    memset(p, 0, n);
  return p;
}
extern "C" char *gettext(const char *text) noexcept {
  return const_cast<char *>(text);
}
extern "C" int *__errno_location() noexcept {
  if (!installed)
    fail(1);
  return kernel.error_location();
}
extern "C" void *__cxa_get_globals() noexcept {
  if (!installed)
    fail(1);
  return kernel.exception_globals();
}
extern "C" void *__cxa_get_globals_fast() noexcept {
  return __cxa_get_globals();
}
extern "C" void *__cxa_allocate_exception(std::size_t bytes) noexcept {
  if (!installed || bytes > 768)
    fail(3);
  auto *p = static_cast<unsigned char *>(
      kernel.exception_allocate(kernel.domain, bytes + exception_header_bytes));
  if (!p)
    fail(3);
  memset(p, 0, exception_header_bytes);
  return p + exception_header_bytes;
}
extern "C" void __cxa_free_exception(void *p) noexcept {
  if (p)
    kernel.exception_release(kernel.domain, static_cast<unsigned char *>(p) -
                                                exception_header_bytes);
}
extern "C" void *__cxa_allocate_dependent_exception() noexcept {
  if (!installed)
    fail(1);
  auto *p = kernel.exception_allocate(kernel.domain, dependent_exception_bytes);
  if (!p)
    fail(3);
  memset(p, 0, dependent_exception_bytes);
  return p;
}
extern "C" void __cxa_free_dependent_exception(void *p) noexcept {
  if (p)
    kernel.exception_release(kernel.domain, p);
}
extern "C" void abort() noexcept { fail(4); }
extern "C" [[noreturn]] void __stack_chk_fail() noexcept { fail(5); }
extern "C" [[noreturn]] void __assert_fail(const char *, const char *, unsigned,
                                           const char *) noexcept {
  fail(6);
}
extern "C" [[noreturn]] void __cxa_pure_virtual() { fail(7); }
extern "C" [[noreturn]] void __cxa_deleted_virtual() { fail(7); }
namespace std {
void terminate() noexcept { fail(8); }
namespace {
using Handler = void (*)();
Handler terminate_callback = &terminate;
Handler unexpected_callback = &terminate;
} // namespace
terminate_handler get_terminate() noexcept {
  return __atomic_load_n(&terminate_callback, __ATOMIC_ACQUIRE);
}
terminate_handler set_terminate(terminate_handler value) noexcept {
  return __atomic_exchange_n(&terminate_callback, value ? value : &terminate,
                             __ATOMIC_ACQ_REL);
}
Handler get_unexpected() noexcept {
  return __atomic_load_n(&unexpected_callback, __ATOMIC_ACQUIRE);
}
Handler set_unexpected(Handler value) noexcept {
  return __atomic_exchange_n(&unexpected_callback, value ? value : &terminate,
                             __ATOMIC_ACQ_REL);
}
void unexpected() { fail(8); }
void __glibcxx_assert_fail(const char *, int, const char *,
                           const char *) noexcept {
  fail(6);
}
} // namespace std
namespace __cxxabiv1 {
void __terminate(void (*)()) noexcept { fail(8); }
void __unexpected(void (*)()) { fail(8); }
} // namespace __cxxabiv1
namespace __gnu_cxx {
void __verbose_terminate_handler() { fail(8); }
} // namespace __gnu_cxx
extern "C" char __libc_single_threaded =
    0; // Always use the synchronized shared_ptr path.
extern "C" void *__dso_handle = &__dso_handle;
namespace {
struct Finalizer {
  void (*function)(void *);
  void *argument;
  void *dso;
};
Finalizer finalizers[128]{};
unsigned finalizer_count = 0;
} // namespace
extern "C" int __cxa_atexit(void (*function)(void *), void *argument,
                            void *dso) noexcept {
  if (finalizer_count == 128)
    return -1;
  finalizers[finalizer_count++] = {function, argument, dso};
  return 0;
}
extern "C" void __cxa_finalize(void *dso) noexcept {
  for (unsigned i = finalizer_count; i; --i) {
    auto &item = finalizers[i - 1];
    if (item.function && (!dso || item.dso == dso)) {
      auto function = item.function;
      item.function = nullptr;
      function(item.argument);
    }
  }
}
extern "C" int __cxa_guard_acquire(std::uint64_t *guard) noexcept {
  auto value = __atomic_load_n(guard, __ATOMIC_ACQUIRE);
  if (value & 1)
    return 0;
  std::uint64_t expected = 0;
  if (!__atomic_compare_exchange_n(guard, &expected, 2, false, __ATOMIC_ACQ_REL,
                                   __ATOMIC_ACQUIRE))
    fail(9);
  return 1;
}
extern "C" void __cxa_guard_release(std::uint64_t *guard) noexcept {
  __atomic_store_n(guard, 1, __ATOMIC_RELEASE);
}
extern "C" void __cxa_guard_abort(std::uint64_t *guard) noexcept {
  __atomic_store_n(guard, 0, __ATOMIC_RELEASE);
}

#define UNWIND_RETURN(Name, Index, Return, Parameters, Arguments)              \
  extern "C" Return __clang_tracer_unwind_##Name Parameters;                   \
  extern "C" Return Name Parameters {                                          \
    if (!installed || !kernel.unwind[tracer::context_support::Index])          \
      fail(10);                                                                \
    if (kernel.private_unwind())                                               \
      return __clang_tracer_unwind_##Name Arguments;                           \
    return reinterpret_cast<decltype(&Name)>(                                  \
        kernel.unwind[tracer::context_support::Index]) Arguments;              \
  }
UNWIND_RETURN(_Unwind_DeleteException, delete_exception, void,
              (_Unwind_Exception * e), (e))
UNWIND_RETURN(_Unwind_GetDataRelBase, get_data_base, _Unwind_Ptr,
              (_Unwind_Context * c), (c))
UNWIND_RETURN(_Unwind_GetIPInfo, get_ip_info, _Unwind_Ptr,
              (_Unwind_Context * c, int *before), (c, before))
UNWIND_RETURN(_Unwind_GetLanguageSpecificData, get_language_data, void *,
              (_Unwind_Context * c), (c))
UNWIND_RETURN(_Unwind_GetRegionStart, get_region_start, _Unwind_Ptr,
              (_Unwind_Context * c), (c))
UNWIND_RETURN(_Unwind_GetTextRelBase, get_text_base, _Unwind_Ptr,
              (_Unwind_Context * c), (c))
UNWIND_RETURN(_Unwind_SetGR, set_gr, void,
              (_Unwind_Context * c, int index, _Unwind_Word value),
              (c, index, value))
UNWIND_RETURN(_Unwind_SetIP, set_ip, void,
              (_Unwind_Context * c, _Unwind_Ptr value), (c, value))
UNWIND_RETURN(_Unwind_GetGR, get_gr, _Unwind_Word,
              (_Unwind_Context * c, int index), (c, index))
UNWIND_RETURN(_Unwind_GetIP, get_ip, _Unwind_Ptr, (_Unwind_Context * c), (c))
UNWIND_RETURN(_Unwind_GetCFA, get_cfa, _Unwind_Word, (_Unwind_Context * c), (c))
extern "C" _Unwind_Reason_Code
__clang_tracer_unwind__Unwind_RaiseException(_Unwind_Exception *);
extern "C" _Unwind_Reason_Code
__clang_tracer_unwind__Unwind_Resume_or_Rethrow(_Unwind_Exception *);
extern "C" void __clang_tracer_unwind__Unwind_Resume(_Unwind_Exception *);
extern "C" _Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Exception *e) {
  if (!installed)
    fail(10);
  const bool own = kernel.owns_exception(kernel.domain, e);
  kernel.set_private_unwind(own);
  return own ? __clang_tracer_unwind__Unwind_RaiseException(e)
             : reinterpret_cast<decltype(&_Unwind_RaiseException)>(
                   kernel.unwind[tracer::context_support::raise_exception])(e);
}
extern "C" _Unwind_Reason_Code _Unwind_Resume_or_Rethrow(_Unwind_Exception *e) {
  if (!installed)
    fail(10);
  const bool own = kernel.owns_exception(kernel.domain, e);
  kernel.set_private_unwind(own);
  return own ? __clang_tracer_unwind__Unwind_Resume_or_Rethrow(e)
             : reinterpret_cast<decltype(&_Unwind_Resume_or_Rethrow)>(
                   kernel.unwind[tracer::context_support::resume_or_rethrow])(
                   e);
}
extern "C" void _Unwind_Resume(_Unwind_Exception *e) {
  if (!installed || !kernel.unwind[tracer::context_support::resume])
    fail(10);
  const bool own = kernel.owns_exception(kernel.domain, e);
  kernel.set_private_unwind(own);
  if (own)
    __clang_tracer_unwind__Unwind_Resume(e);
  else
    reinterpret_cast<decltype(&_Unwind_Resume)>(
        kernel.unwind[tracer::context_support::resume])(e);
  __builtin_unreachable();
}
#undef UNWIND_RETURN

extern "C" _Unwind_Reason_Code
__clang_tracer_support_personality(int, _Unwind_Action, _Unwind_Exception_Class,
                                   _Unwind_Exception *, _Unwind_Context *);
extern "C" _Unwind_Reason_Code
__gxx_personality_v0(int version, _Unwind_Action actions,
                     _Unwind_Exception_Class exception_class,
                     _Unwind_Exception *exception, _Unwind_Context *context) {
  if (!installed)
    fail(10);
  // Native engine errors and cancellation can enter after an earlier private
  // exception was caught. Determine the context ABI from this exception on
  // every phase rather than retaining the previous unwind's TLS mode.
  kernel.set_private_unwind(kernel.owns_exception(kernel.domain, exception));
  return __clang_tracer_support_personality(version, actions, exception_class,
                                            exception, context);
}

extern "C" const void *__clang_tracer_unwind__Unwind_Find_FDE(
    const void *pc, tracer::context_support::UnwindBases *bases) noexcept {
  if (!installed)
    fail(10);
  return kernel.find_fde(kernel.domain, reinterpret_cast<std::uintptr_t>(pc),
                         bases);
}
extern "C" void __clang_tracer_unwind_abort() noexcept { fail(10); }
extern "C" std::size_t __clang_tracer_unwind_strlen(const char *text) noexcept {
  return strlen(text);
}
extern "C" int
__clang_tracer_unwind_pthread_once(int *once, void (*initialize)()) noexcept {
  const auto value = __atomic_load_n(once, __ATOMIC_ACQUIRE);
  if (value == 2)
    return 0;
  int expected = 0;
  if (!__atomic_compare_exchange_n(once, &expected, 1, false, __ATOMIC_ACQ_REL,
                                   __ATOMIC_ACQUIRE))
    fail(10);
  initialize();
  __atomic_store_n(once, 2, __ATOMIC_RELEASE);
  return 0;
}

void *operator new(std::size_t n) {
  auto *p = malloc(n);
  if (!p)
    throw std::bad_alloc();
  return p;
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void *operator new(std::size_t n, std::align_val_t alignment) {
  auto *p = allocate(n, static_cast<std::size_t>(alignment));
  if (!p)
    throw std::bad_alloc();
  return p;
}
void *operator new[](std::size_t n, std::align_val_t a) {
  return ::operator new(n, a);
}
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
  return malloc(n);
}
void *operator new[](std::size_t n, const std::nothrow_t &) noexcept {
  return malloc(n);
}
void *operator new(std::size_t n, std::align_val_t a,
                   const std::nothrow_t &) noexcept {
  return allocate(n, static_cast<std::size_t>(a));
}
void *operator new[](std::size_t n, std::align_val_t a,
                     const std::nothrow_t &) noexcept {
  return allocate(n, static_cast<std::size_t>(a));
}
void operator delete(void *p) noexcept { release(p); }
void operator delete[](void *p) noexcept { release(p); }
void operator delete(void *p, std::size_t n) noexcept { release(p, n); }
void operator delete[](void *p, std::size_t n) noexcept { release(p, n); }
void operator delete(void *p, std::align_val_t a) noexcept {
  release(p, 0, static_cast<std::size_t>(a));
}
void operator delete[](void *p, std::align_val_t a) noexcept {
  release(p, 0, static_cast<std::size_t>(a));
}
void operator delete(void *p, std::size_t n, std::align_val_t a) noexcept {
  release(p, n, static_cast<std::size_t>(a));
}
void operator delete[](void *p, std::size_t n, std::align_val_t a) noexcept {
  release(p, n, static_cast<std::size_t>(a));
}

extern "C" __attribute__((visibility("default"))) void
clangtracer_stdlib_anchors_v1(unsigned selector) {
  // Forces the documented error families into the frozen private archive
  // closure without running any user code or static initializer.
  if (selector == 1)
    throw std::bad_alloc();
  if (selector == 2)
    throw std::bad_cast();
  if (selector == 3)
    throw std::bad_array_new_length();
  if (selector == 4)
    throw std::logic_error("state support");
  if (selector == 5)
    throw std::length_error("state support");
  if (selector == 6)
    throw std::out_of_range("state support");
  if (selector == 7)
    throw std::runtime_error("state support");
  if (selector == 8)
    throw std::invalid_argument("state support");
}
