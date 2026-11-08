#include <unistd.h>

// Give the tiny functions a patchable instruction sequence inside their DWARF
// ranges, including with GCC 8.5, which excludes patchable-entry padding.
__attribute__((always_inline)) inline void keep_patch_window() {
  asm volatile("mov %%eax, %%eax\n\tmov %%eax, %%eax" ::: "eax");
}

template <class T> struct Configuration {
  T value;

  __attribute__((noinline)) const T &get() const {
    keep_patch_window();
    return value;
  }
};

template <class T, int Offset> struct AdjustedConfiguration {
  struct Converter {
    T value;

    template <class U> __attribute__((noinline)) U convert() const {
      keep_patch_window();
      return static_cast<U>(value) + Offset;
    }
  };
};

template <class T> __attribute__((noinline)) T echo(T value) {
  keep_patch_window();
  return value;
}

volatile long result;

int main() {
  Configuration<int> conf{42};
  AdjustedConfiguration<int, 5>::Converter adjusted{42};
  while (true) {
    result = conf.get();
    result = adjusted.convert<long>();
    result = echo<int>(43);
    usleep(50000);
  }
}
