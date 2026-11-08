#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <unistd.h>

#include "admission.hpp"

// Isolate metadata work from ptrace, RPC transport and callback compilation.
int main(int argc, char **argv) {
  std::vector<void *> handles;
  try {
    for (int i = 1; i < argc; ++i) {
      void *handle = ::dlopen(argv[i], RTLD_NOW | RTLD_LOCAL);
      if (!handle) {
        throw std::runtime_error(::dlerror());
      }
      handles.push_back(handle);
    }
    std::cout << "phase,sample,modules,milliseconds\n";
    for (int sample = 0; sample < 5; ++sample) {
      const auto begin = std::chrono::steady_clock::now();
      const auto modules =
          tracer::injector::discover_application_modules(::getpid());
      const auto discovered = std::chrono::steady_clock::now();
      tracer::injector::validate_application_modules(::getpid(), modules);
      const auto validated = std::chrono::steady_clock::now();
      const auto milliseconds = [](auto duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
      };
      std::cout << "discover," << sample << ',' << modules.size() << ','
                << milliseconds(discovered - begin) << '\n';
      std::cout << "validate," << sample << ',' << modules.size() << ','
                << milliseconds(validated - discovered) << std::endl;
    }
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    for (void *handle : handles) {
      ::dlclose(handle);
    }
    return 1;
  }
  for (void *handle : handles) {
    ::dlclose(handle);
  }
}
