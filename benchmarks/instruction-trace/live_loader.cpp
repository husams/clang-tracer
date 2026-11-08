// Source for the fixture's temporary in-target Clang interceptor.
#include <cstdio>
#include <cerrno>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace {
int restore_fixture_ptracer() {
#ifdef PR_SET_PTRACER
  // Gum temporarily authorizes its own helper. The fixture's original
  // authorization is known because it explicitly selected its parent.
  if (prctl(PR_SET_PTRACER, static_cast<unsigned long>(getppid()), 0UL, 0UL, 0UL) != 0 &&
      errno != EINVAL)
    return -17;
#endif
  return 0;
}

struct Backend {
  void *handle = nullptr;
  PilotGumLiveStart start = nullptr;
  PilotGumLiveStop stop = nullptr;
  void (*shutdown)() = nullptr;

  int open() {
    if (handle)
      return -10;
    handle = dlopen(PILOT_PLUGIN_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      std::fprintf(stderr, "Gum dlopen: %s\n", dlerror());
      return -11;
    }
    const auto init = reinterpret_cast<int (*)()>(dlsym(handle, "pilot_gum_init"));
    shutdown = reinterpret_cast<void (*)()>(dlsym(handle, "pilot_gum_shutdown"));
    start = reinterpret_cast<PilotGumLiveStart>(dlsym(handle, "pilot_gum_live_start"));
    stop = reinterpret_cast<PilotGumLiveStop>(dlsym(handle, "pilot_gum_live_stop"));
    if (!init || !shutdown || !start || !stop) {
      dlclose(handle);
      handle = nullptr;
      return -12;
    }
    if (init() != 0) {
      close();
      return -13;
    }
    return 0;
  }

  int close() {
    shutdown();
    const auto result = dlclose(handle);
    handle = nullptr;
    return result;
  }

  int collect() {
    if (!handle)
      return -14;
    PilotResult result{};
    uint32_t offsets[PILOT_MAX_RECORDS];
    const auto status = stop(&result, offsets, PILOT_MAX_RECORDS);
    if (const auto auth = restore_fixture_ptracer())
      return auth;
    if (status != 0)
      return status; // Keep a possibly executing backend mapped.
    if (close() != 0)
      return -15;
    auto *file = std::fopen(PILOT_RESULT_PATH, "wb");
    if (!file)
      return -16;
    std::fprintf(file, "{\"truncated\":%u,\"offsets\":[", result.truncated);
    for (unsigned i = 0; i < result.count; ++i)
      std::fprintf(file, "%s%u", i ? "," : "", offsets[i]);
    std::fputs("]}\n", file);
    return std::fclose(file);
  }
};
} // namespace

struct Interceptor {
  void pre(const Call &call) {
    // The fixture's leader is the only caller of pilot_control().
    static Backend backend;
    const auto command = call.arg<int>(0);
    int status;
    if (command == 'e') {
      status = backend.collect();
    } else {
      status = backend.open();
      if (status == 0) {
        status = backend.start(call.arg<int>(1), call.arg<unsigned long>(2),
                               call.arg<unsigned long>(3), call.arg<int>(4));
        if (status != 0 && status != -2)
          backend.close();
        else if (status == 0)
          status = restore_fixture_ptracer();
      }
    }
    call.field("gum_status", status);
  }
};
