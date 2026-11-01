#include "context_stdlib.hpp"
#include "context_execution.hpp"
#include "context_paths.hpp"
#include "context_unwind.hpp"
#include "engine_storage.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>

#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>

#if defined(__linux__) && defined(__x86_64__)
#include <dlfcn.h>
#include <setjmp.h>
#include <unwind.h>
#endif

namespace tracer {
namespace {
llvm::Error invalid(const std::string &reason) {
  return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                 "unsupported_state_support_profile: " +
                                     reason);
}
struct ActiveLane {
  void *image = nullptr;
  unsigned lane = 0;
  const void *guest = nullptr;
  ContextStdlibImage::UnwindLookup lookup = nullptr;
};
thread_local ActiveLane active_lane;
thread_local int support_error = 0;
thread_local bool private_unwinding = false;
constexpr unsigned kLanes = 64, kExceptions = 64;
constexpr std::size_t kExceptionBytes = 1024;

struct Lane {
  std::atomic<bool> occupied{false};
  std::atomic<std::uint64_t> exceptions{0};
  std::shared_ptr<const ContextStdlibImage> quarantine;
  alignas(16) std::byte buffers[kExceptions][kExceptionBytes];
};

#if defined(__linux__) && defined(__x86_64__)
struct UnwindBoundary {
  sigjmp_buf jump;
  std::uintptr_t cfa = 0;
  UnwindBoundary *previous = nullptr;
  const context_support::Kernel *kernel = nullptr;
  alignas(16) _Unwind_Exception exception{};
};
thread_local UnwindBoundary *active_boundary = nullptr;

_Unwind_Reason_Code stop_unwind(int, _Unwind_Action actions,
                                _Unwind_Exception_Class, _Unwind_Exception *,
                                _Unwind_Context *context, void *argument) {
  auto &boundary = *static_cast<UnwindBoundary *>(argument);
  auto cfa = reinterpret_cast<decltype(&_Unwind_GetCFA)>(
      boundary.kernel->unwind[context_support::get_cfa])(context);
  if ((actions & _UA_END_OF_STACK) || cfa >= boundary.cfa)
    siglongjmp(boundary.jump, 1);
  return _URC_NO_REASON;
}
void resume_forced(_Unwind_Reason_Code, _Unwind_Exception *exception) {
  // A catch-all is not allowed to consume a control unwind. Resume after its
  // normal catch cleanup (including restoration of native EH globals).
  if (active_boundary)
    reinterpret_cast<decltype(&_Unwind_Resume)>(
        active_boundary->kernel->unwind[context_support::resume])(exception);
  __builtin_trap();
}

llvm::Error audit_image(const std::string &path,
                        std::vector<std::string> &exports,
                        std::uintptr_t &eh_address, std::size_t &eh_size) {
  auto file = llvm::MemoryBuffer::getFile(path, false, false);
  if (!file)
    return invalid("cannot read packaged support image");
  auto object =
      llvm::object::ObjectFile::createObjectFile((*file)->getMemBufferRef());
  if (!object)
    return object.takeError();
  auto *elf = llvm::dyn_cast<llvm::object::ELF64LEObjectFile>(object->get());
  if (!elf || (*object)->getArch() != llvm::Triple::x86_64)
    return invalid("support image is not the checked ELF64 x86_64 profile");
  auto dynamics = elf->getELFFile().dynamicEntries();
  if (!dynamics)
    return dynamics.takeError();
  bool symbolic = false, immediate = false;
  for (const auto &entry : *dynamics) {
    const auto tag = entry.getTag();
    if (tag == llvm::ELF::DT_NEEDED || tag == llvm::ELF::DT_INIT ||
        tag == llvm::ELF::DT_FINI || tag == llvm::ELF::DT_INIT_ARRAY ||
        tag == llvm::ELF::DT_FINI_ARRAY || tag == llvm::ELF::DT_TEXTREL)
      return invalid("support image has an external dependency or "
                     "startup/finalizer callback");
    if (tag == llvm::ELF::DT_SYMBOLIC)
      symbolic = true;
    if (tag == llvm::ELF::DT_BIND_NOW)
      immediate = true;
    if (tag == llvm::ELF::DT_FLAGS) {
      symbolic |= entry.getVal() & llvm::ELF::DF_SYMBOLIC;
      immediate |= entry.getVal() & llvm::ELF::DF_BIND_NOW;
    }
  }
  if (!symbolic || !immediate)
    return invalid("support image is not privately/immediately bound");
  for (const auto &section : (*object)->sections()) {
    auto name = section.getName();
    if (!name)
      return name.takeError();
    if (section.getSize() && (*name == ".tdata" || *name == ".tbss" ||
                              *name == ".init_array" || *name == ".fini_array"))
      return invalid("unadmitted TLS/startup code in support image");
    if (*name == ".eh_frame") {
      eh_address = section.getAddress();
      eh_size = section.getSize();
    }
  }
  for (const auto &symbol : elf->getDynamicSymbolIterators()) {
    auto flags = symbol.getFlags();
    if (!flags)
      return flags.takeError();
    auto name = symbol.getName();
    if (!name)
      return name.takeError();
    if (name->empty())
      continue;
    if (*flags & llvm::object::SymbolRef::SF_Undefined)
      return invalid("unsealed support relocation: " + name->str());
    if ((*flags & llvm::object::SymbolRef::SF_Global) &&
        !(*flags & llvm::object::SymbolRef::SF_FormatSpecific))
      exports.push_back(name->str());
  }
  if (!eh_address || !eh_size)
    return invalid("support image has no EH metadata");
  return llvm::Error::success();
}
#endif
} // namespace

struct ContextStdlibImage::Impl {
  std::shared_ptr<const context_engine::StorageDomain> storage;
  Lane *lanes = nullptr;
  context_support::Kernel kernel;
  context_support::Profile profile{};
  std::vector<std::string> exports;
  ContextUnwindTable unwind;
  void *image = nullptr, *unwind_image = nullptr, *exception_image = nullptr;
  void *memory_image = nullptr;
  void *base = nullptr;
  void *(*get_globals)() = nullptr;
  std::uintptr_t find_fde = 0, forced_unwind = 0;
  using Finalize = void (*)(void *) noexcept;
  Finalize finalize = nullptr;

  ~Impl() {
#if defined(__linux__) && defined(__x86_64__)
    // Still pinned while all private finalizers return through native code.
    if (finalize)
      finalize(nullptr);
    if (image)
      ::dlclose(image);
    if (exception_image)
      ::dlclose(exception_image);
    if (unwind_image)
      ::dlclose(unwind_image);
    if (memory_image)
      ::dlclose(memory_image);
#endif
    if (lanes) {
      for (unsigned i = 0; i < kLanes; ++i)
        std::destroy_at(lanes + i);
      storage->release(lanes);
    }
  }

  static void *allocate(void *domain, std::size_t bytes,
                        std::size_t alignment) noexcept {
    if (active_lane.image == domain && active_lane.lane >= kLanes)
      return nullptr;
    return static_cast<Impl *>(domain)
        ->storage->allocate(bytes, alignment)
        .pointer;
  }
  static unsigned release(void *domain, void *pointer, std::size_t bytes,
                          std::size_t alignment) noexcept {
    return static_cast<unsigned>(static_cast<Impl *>(domain)->storage->release(
        pointer, bytes, alignment));
  }
  static std::size_t allocation_size(void *domain,
                                     const void *pointer) noexcept {
    return static_cast<Impl *>(domain)->storage->allocation_size(pointer);
  }
  static void *allocate_exception(void *domain, std::size_t bytes) noexcept {
    auto &self = *static_cast<Impl *>(domain);
    if (active_lane.image != &self || active_lane.lane >= kLanes ||
        bytes > kExceptionBytes)
      return nullptr;
    auto &lane = self.lanes[active_lane.lane];
    auto bits = lane.exceptions.load(std::memory_order_relaxed);
    if (bits == ~std::uint64_t{})
      return nullptr;
    const auto index = std::countr_zero(~bits);
    lane.exceptions.fetch_or(std::uint64_t{1} << index,
                             std::memory_order_relaxed);
    return lane.buffers[index];
  }
  static void release_exception(void *domain, void *pointer) noexcept {
    auto &self = *static_cast<Impl *>(domain);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (unsigned i = 0; i < kLanes; ++i) {
      const auto start =
          reinterpret_cast<std::uintptr_t>(self.lanes[i].buffers);
      if (address < start || address - start >= sizeof(self.lanes[i].buffers) ||
          (address - start) % kExceptionBytes)
        continue;
      const auto bit = (address - start) / kExceptionBytes;
      const auto mask = std::uint64_t{1} << bit;
      if (!(self.lanes[i].exceptions.fetch_and(~mask,
                                               std::memory_order_acq_rel) &
            mask))
        panic(domain, 11);
      return;
    }
    panic(domain, 12);
  }
  static bool owns_exception(void *domain, const void *pointer) noexcept {
    const auto &self = *static_cast<Impl *>(domain);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (unsigned i = 0; i < kLanes; ++i) {
      const auto begin =
          reinterpret_cast<std::uintptr_t>(self.lanes[i].buffers);
      if (address < begin || address - begin >= sizeof(self.lanes[i].buffers))
        continue;
      const auto bit = (address - begin) / kExceptionBytes;
      return self.lanes[i].exceptions.load(std::memory_order_acquire) &
             (std::uint64_t{1} << bit);
    }
    return false;
  }
  static bool is_private_unwind() noexcept { return private_unwinding; }
  static void set_private_unwind(bool value) noexcept {
    private_unwinding = value;
  }
  static const void *lookup_fde(void *domain, std::uintptr_t pc,
                                context_support::UnwindBases *bases) noexcept {
    if (!bases)
      return nullptr;
    auto &self = *static_cast<Impl *>(domain);
    if (const auto *fde = self.unwind.find(pc, *bases))
      return fde;
    if (active_lane.image == &self && active_lane.lookup)
      return active_lane.lookup(active_lane.guest, pc, *bases);
    return nullptr;
  }
  static void *exception_globals() noexcept {
    auto *self = static_cast<Impl *>(active_lane.image);
    return self ? self->get_globals() : nullptr;
  }
  static int *error_location() noexcept { return &support_error; }
  static void panic(void *domain, unsigned) noexcept {
    (void)domain;
    namespace ex = context_execution;
    ex::runtime_diagnostics().record(
        ex::Severity::critical, ex::Reason::invalid_ownership,
        ex::Phase::access, ex::Action::callback_skipped);
#if defined(__linux__) && defined(__x86_64__)
    auto &self = *static_cast<Impl *>(domain);
    if (active_boundary) {
      private_unwinding = false;
      active_boundary->exception.exception_class = 0x43545841424f5254ULL;
      active_boundary->exception.exception_cleanup = &resume_forced;
      using Forced =
          _Unwind_Reason_Code (*)(_Unwind_Exception *, _Unwind_Stop_Fn, void *);
      reinterpret_cast<Forced>(self.forced_unwind)(
          &active_boundary->exception, &stop_unwind, active_boundary);
      siglongjmp(active_boundary->jump, 1);
    }
#endif
    // The engine calls this only with an admitted native boundary. Keeping the
    // fallback non-returning avoids continuing with an invalid owning value.
    __builtin_trap();
  }
};

ContextStdlibImage::ContextStdlibImage(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
ContextStdlibImage::~ContextStdlibImage() = default;
const context_support::Profile &ContextStdlibImage::profile() const noexcept {
  return impl_->profile;
}
std::size_t ContextStdlibImage::outstanding_allocations() const noexcept {
  return impl_->storage->outstanding();
}

llvm::Expected<std::shared_ptr<const ContextStdlibImage>>
ContextStdlibImage::acquire() {
#if defined(__linux__) && defined(__x86_64__) &&                               \
    defined(TRACER_CONTEXT_STDLIB_PATH)
  struct Cache {
    std::mutex gate;
    std::condition_variable changed;
    bool building = false;
    std::weak_ptr<const ContextStdlibImage> image;
  };
  static Cache cache;
  {
    std::unique_lock lock(cache.gate);
    cache.changed.wait(lock, [&] { return !cache.building; });
    if (auto image = cache.image.lock())
      return image;
    cache.building = true;
  }
  struct Reset {
    Cache &cache;
    ~Reset() {
      {
        std::lock_guard lock(cache.gate);
        cache.building = false;
      }
      cache.changed.notify_all();
    }
  } reset{cache};
  auto impl = std::make_unique<Impl>();
  std::string image_path;
  try {
    image_path = context_standard_library_path();
  } catch (const std::exception &exception) {
    return invalid(exception.what());
  }
  std::uintptr_t eh_address = 0;
  std::size_t eh_size = 0;
  if (auto error = audit_image(image_path, impl->exports, eh_address, eh_size))
    return std::move(error);
  impl->image = ::dlopen(image_path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!impl->image)
    return invalid("cannot load the packaged private runtime");
  auto profile_function =
      reinterpret_cast<const context_support::Profile *(*)() noexcept>(
          ::dlsym(impl->image, "clangtracer_stdlib_profile_v1"));
  auto install =
      reinterpret_cast<bool (*)(const context_support::Kernel *) noexcept>(
          ::dlsym(impl->image, "clangtracer_stdlib_install_v1"));
  if (!profile_function || !install)
    return invalid("private runtime handshake is absent");
  Dl_info information{};
  if (!::dladdr(reinterpret_cast<void *>(profile_function), &information))
    return invalid("private runtime image membership is unavailable");
  impl->base = information.dli_fbase;
  const auto eh_runtime =
      reinterpret_cast<std::uintptr_t>(impl->base) + eh_address;
  if (auto error =
          impl->unwind.add_region(reinterpret_cast<const void *>(eh_runtime),
                                  eh_size, eh_runtime, llvm::Triple::x86_64))
    return std::move(error);
  impl->profile = *profile_function();
  const auto &profile = impl->profile;
  if (profile.abi != context_support::kKernelAbi ||
      profile.standard_library_date != __GLIBCXX__ ||
      profile.standard_library_release != _GLIBCXX_RELEASE ||
      profile.cxx11_abi != _GLIBCXX_USE_CXX11_ABI ||
      profile.dual_abi != _GLIBCXX_USE_DUAL_ABI ||
      profile.pointer_bytes != sizeof(void *) ||
      profile.wchar_bytes != sizeof(wchar_t) ||
      profile.any_bytes != sizeof(std::any) ||
      profile.any_alignment != alignof(std::any) ||
      profile.shared_bytes != sizeof(std::shared_ptr<void>) ||
      profile.shared_alignment != alignof(std::shared_ptr<void>) ||
      std::string_view(profile.archive_sha256, 64) !=
          TRACER_CONTEXT_ARCHIVE_SHA256)
    return invalid("private runtime/native C++ compilation profiles differ");
  // Explicit handles keep every native unwind/TLS callee loaded. Symbol
  // membership checks reject preemption through an application's definitions.
  impl->unwind_image = ::dlopen("libgcc_s.so.1", RTLD_NOW | RTLD_LOCAL);
  impl->exception_image = ::dlopen("libstdc++.so.6", RTLD_NOW | RTLD_LOCAL);
  impl->memory_image = ::dlopen("libc.so.6", RTLD_NOW | RTLD_LOCAL);
  if (!impl->unwind_image || !impl->exception_image || !impl->memory_image)
    return invalid("native unwind/TLS runtime is unavailable");
  // Native fixed-buffer copies can also be introduced by the host compiler.
  // Shared-state guards also depend on the native mutex entrypoints. Reject
  // application interposition before publishing either memory or lock access.
  for (const auto *name :
       {"memcpy", "memmove", "memset", "memcmp", "strlen", "pthread_mutex_lock",
        "pthread_mutex_trylock", "pthread_mutex_unlock"}) {
    auto *address = ::dlsym(impl->memory_image, name);
    if (!address || ::dlsym(RTLD_DEFAULT, name) != address ||
        !::dladdr(address, &information) || !information.dli_fname ||
        std::string_view(information.dli_fname).find("libc.so") ==
            std::string_view::npos)
      return invalid("native memory/lock primitive is interposed: " +
                     std::string(name));
  }
  constexpr const char *names[]{"_Unwind_RaiseException",
                                "_Unwind_DeleteException",
                                "_Unwind_GetDataRelBase",
                                "_Unwind_GetIPInfo",
                                "_Unwind_GetLanguageSpecificData",
                                "_Unwind_GetRegionStart",
                                "_Unwind_GetTextRelBase",
                                "_Unwind_Resume",
                                "_Unwind_Resume_or_Rethrow",
                                "_Unwind_SetGR",
                                "_Unwind_SetIP",
                                "_Unwind_GetGR",
                                "_Unwind_GetIP",
                                "_Unwind_GetCFA"};
  for (unsigned i = 0; i < context_support::unwind_entries; ++i) {
    auto *address = ::dlsym(impl->unwind_image, names[i]);
    if (!address || ::dlsym(RTLD_DEFAULT, names[i]) != address ||
        !::dladdr(address, &information) ||
        std::string_view(information.dli_fname).find("libgcc_s.so") ==
            std::string_view::npos)
      return invalid("native unwind entry is missing or interposed");
    impl->kernel.unwind[i] = reinterpret_cast<std::uintptr_t>(address);
  }
  impl->find_fde = reinterpret_cast<std::uintptr_t>(
      ::dlsym(impl->unwind_image, "_Unwind_Find_FDE"));
  impl->forced_unwind = reinterpret_cast<std::uintptr_t>(
      ::dlsym(impl->unwind_image, "_Unwind_ForcedUnwind"));
  impl->get_globals = reinterpret_cast<void *(*)()>(
      ::dlsym(impl->exception_image, "__cxa_get_globals"));
  if (!impl->find_fde || !impl->forced_unwind || !impl->get_globals)
    return invalid("required native unwind proof endpoints are absent");
  for (const auto *name :
       {"__cxa_get_globals", "__cxa_init_primary_exception"}) {
    auto *address = ::dlsym(impl->exception_image, name);
    if (!address || ::dlsym(RTLD_DEFAULT, name) != address ||
        !::dladdr(address, &information) || !information.dli_fname ||
        std::string_view(information.dli_fname).find("libstdc++.so") ==
            std::string_view::npos)
      return invalid("native exception ABI/TLS entry is interposed");
  }
  impl->storage = context_engine::StorageDomain::create();
  auto reserve = impl->storage->allocate(sizeof(Lane) * kLanes, alignof(Lane));
  if (!reserve.pointer)
    return invalid("cannot reserve bounded support exception lanes");
  impl->lanes = static_cast<Lane *>(reserve.pointer);
  for (unsigned i = 0; i < kLanes; ++i)
    std::construct_at(impl->lanes + i);
  impl->kernel.domain = impl.get();
  impl->kernel.allocate = &Impl::allocate;
  impl->kernel.release = &Impl::release;
  impl->kernel.allocation_size = &Impl::allocation_size;
  impl->kernel.exception_allocate = &Impl::allocate_exception;
  impl->kernel.exception_release = &Impl::release_exception;
  impl->kernel.exception_globals = &Impl::exception_globals;
  impl->kernel.error_location = &Impl::error_location;
  impl->kernel.panic = &Impl::panic;
  impl->kernel.owns_exception = &Impl::owns_exception;
  impl->kernel.private_unwind = &Impl::is_private_unwind;
  impl->kernel.set_private_unwind = &Impl::set_private_unwind;
  impl->kernel.find_fde = &Impl::lookup_fde;
  if (!install(&impl->kernel))
    return invalid("private runtime rejected its native kernel");
  impl->finalize =
      reinterpret_cast<Impl::Finalize>(::dlsym(impl->image, "__cxa_finalize"));
  auto image = std::shared_ptr<const ContextStdlibImage>(
      new ContextStdlibImage(std::move(impl)));
  std::weak_ptr<const ContextStdlibImage> publish(image);
  {
    std::lock_guard lock(cache.gate);
    cache.image.swap(publish);
  }
  return image;
#else
  return invalid("no packaged standard-library profile for this host");
#endif
}

llvm::Expected<std::uintptr_t>
ContextStdlibImage::symbol(const std::string &name) const {
#if defined(__linux__) && defined(__x86_64__)
  if (name.starts_with("clangtracer_stdlib_") ||
      std::find(impl_->exports.begin(), impl_->exports.end(), name) ==
          impl_->exports.end())
    return invalid("symbol is outside the sealed support catalog: " + name);
  auto *address = ::dlsym(impl_->image, name.c_str());
  Dl_info info{};
  if (!address || !::dladdr(address, &info) || info.dli_fbase != impl_->base)
    return invalid("support symbol is not bound inside its retained image: " +
                   name);
  return reinterpret_cast<std::uintptr_t>(address);
#else
  (void)name;
  return invalid("no private runtime image");
#endif
}

bool ContextStdlibImage::contains(std::string_view name) const noexcept {
  return std::find(impl_->exports.begin(), impl_->exports.end(), name) !=
         impl_->exports.end();
}

bool ContextStdlibImage::validate_runtime() const noexcept {
#if defined(__linux__) && defined(__x86_64__)
  auto image = shared_from_this();
  Frame frame(image);
  if (!frame.entered())
    return false;
  using Initialize = void *(*)(void *, std::type_info *, void (*)(void *));
  auto private_init = reinterpret_cast<Initialize>(
      ::dlsym(impl_->image, "__cxa_init_primary_exception"));
  auto native_init = reinterpret_cast<Initialize>(
      ::dlsym(impl_->exception_image, "__cxa_init_primary_exception"));
  alignas(16) std::byte private_bytes[512]{}, native_bytes[512]{};
  if (!private_init || !native_init ||
      private_init(private_bytes + 256,
                   const_cast<std::type_info *>(&typeid(int)),
                   nullptr) != private_bytes + 128 ||
      native_init(native_bytes + 256,
                  const_cast<std::type_info *>(&typeid(int)),
                  nullptr) != native_bytes + 128)
    return false;
  using Probe = unsigned (*)(std::any *, unsigned *) noexcept;
  auto probe = reinterpret_cast<Probe>(
      ::dlsym(impl_->image, "clangtracer_stdlib_probe_v1"));
  if (!probe)
    return false;
  const auto before = outstanding_allocations();
  std::any erased;
  unsigned destroyed = 0;
  if (probe(&erased, &destroyed) || !erased.has_value() || destroyed)
    return false;
  erased.reset();
  return destroyed == 1 && outstanding_allocations() == before;
#else
  return false;
#endif
}

bool ContextStdlibImage::warm_unwind(std::uintptr_t address) const noexcept {
#if defined(__linux__) && defined(__x86_64__)
  struct Bases {
    void *text = nullptr, *data = nullptr, *function = nullptr;
  } bases;
  using Find = const void *(*)(const void *, Bases *);
  return reinterpret_cast<Find>(impl_->find_fde)(
             reinterpret_cast<const void *>(address), &bases) != nullptr;
#else
  (void)address;
  return false;
#endif
}

ContextStdlibImage::Frame::Frame(
    std::shared_ptr<const ContextStdlibImage> image, const void *guest,
    UnwindLookup lookup, Purpose purpose) noexcept
    : image_(std::move(image)) {
  if (!image_)
    return;
  if (active_lane.image) {
    if (active_lane.image == image_->impl_.get()) {
      previous_guest_ = active_lane.guest;
      previous_lookup_ = active_lane.lookup;
      if (guest) {
        active_lane.guest = guest;
        active_lane.lookup = lookup;
      }
      nested_ = true;
      entered_ = true;
    }
    return;
  }
  if (purpose == Purpose::cleanup) {
    // Validated finalizers cannot allocate or throw. They must still release
    // payloads when unrelated invocations occupy every exception lane.
    lane_ = kLanes;
    private_unwinding = false;
    active_lane = {image_->impl_.get(), kLanes, guest, lookup};
    entered_ = true;
    return;
  }
  // Warm native TLS before entering guarded/user code, never through a support
  // allocator or application-preemptible symbol lookup in a hook.
  if (!image_->impl_->get_globals || !image_->impl_->get_globals())
    return;
  for (unsigned i = 0; i < kLanes; ++i) {
    bool expected = false;
    if (!image_->impl_->lanes[i].occupied.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
      continue;
    lane_ = i;
    private_unwinding = false;
    active_lane = {image_->impl_.get(), i, guest, lookup};
    entered_ = true;
    return;
  }
}
ContextStdlibImage::Frame::~Frame() noexcept {
  if (!entered_)
    return;
  if (nested_) {
    active_lane.guest = previous_guest_;
    active_lane.lookup = previous_lookup_;
    return;
  }
  active_lane = {};
  private_unwinding = false;
  if (lane_ == kLanes)
    return;
  auto &lane = image_->impl_->lanes[lane_];
  if (lane.exceptions.load(std::memory_order_acquire)) {
    // A violated nonescaping-exception contract quarantines one bounded lane
    // and pins its code; freeing an escaped cleanup callback would be unsafe.
    lane.quarantine = std::move(image_);
    return;
  }
  lane.occupied.store(false, std::memory_order_release);
}

[[gnu::noinline]] bool
ContextStdlibImage::invoke(void (*function)(void *),
                           void *argument) const noexcept {
#if defined(__linux__) && defined(__x86_64__)
  UnwindBoundary boundary;
  boundary.cfa = reinterpret_cast<std::uintptr_t>(__builtin_dwarf_cfa());
  boundary.previous = active_boundary;
  boundary.kernel = &impl_->kernel;
  if (sigsetjmp(boundary.jump, 0)) {
    active_boundary = boundary.previous;
    return false;
  }
  active_boundary = &boundary;
  try {
    function(argument);
  } catch (...) {
    active_boundary = boundary.previous;
    return false;
  }
  active_boundary = boundary.previous;
  return true;
#else
  (void)function;
  (void)argument;
  return false;
#endif
}
} // namespace tracer
