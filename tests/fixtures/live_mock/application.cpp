#include "application.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>

#include <sys/prctl.h>

namespace {
int buffers = 0;
int destroyed = 0;
int nested_originals = 0;
bool fail_allocation = false;
} // namespace

std::optional<Product> ProductStore::load(int id) {
  ++queries_;
  return Product{id, "Database", 500};
}

std::optional<Product> ProductStore::find_cached(int id) {
  if (cache_.id == id) {
    return cache_;
  }
  return std::nullopt;
}

bool Pricer::is_test_product(int product) const {
  return product == test_id_;
}

Quote* Pricer::make_quote(int product) {
  ++original_calls_;
  return quote_create(product, 200);
}

int Quote::amount() const {
  return price;
}

Quote::~Quote() {
  ++destroyed;
}

__attribute__((visibility("hidden"))) Quote* quote_create(int product, int price) {
  return std::make_unique<Quote>(product, price).release();
}

void quote_destroy(Quote* quote) {
  delete quote;
}

void* target_allocate(std::size_t size) {
  if (fail_allocation) {
    return nullptr;
  }
  auto* storage = std::malloc(size);
  if (storage) {
    ++buffers;
  }
  return storage;
}

void target_release(void* storage) {
  if (storage) {
    --buffers;
    std::free(storage);
  }
}

char* status_message() {
  constexpr char text[] = "original";
  auto* message = static_cast<char*>(target_allocate(sizeof(text)));
  if (message) {
    std::memcpy(message, text, sizeof(text));
  }
  return message;
}

int nesting(int depth) {
  ++nested_originals;
  return depth == 0 ? 1 : 1 + nesting(depth - 1);
}

// Separate out-of-line callers force the actual C++ virtual route even in
// compilers that devirtualize a receiver whose allocation they can see.
[[gnu::noinline]] static Quote* make_virtual(Pricer& pricer, int id) {
  return pricer.make_quote(id);
}

[[gnu::noinline]] static std::optional<Product> load_virtual(ProductStore& store, int id) {
  return store.load(id);
}

int main() {
  if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
    return 2;
  }
  static ProductStore store;
  static Pricer pricer(42);
  // These objects and the saved native entry all predate installation.
  char* message = nullptr;
  Quote* quote = nullptr;
  char* (*volatile saved_status)() = status_message;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("ready");
  char line[64];
  while (std::fgets(line, sizeof(line), stdin)) {
    int id = 0;
    switch (line[0]) {
    case 'l': {
      if (std::sscanf(line + 1, "%d", &id) != 1) {
        return 3;
      }
      const auto result = load_virtual(store, id);
      const auto cached = store.find_cached(42);
      std::printf("load=%d price=%d name=%s queries=%d stored=%d\n", id,
                  result ? result->price_cents : -1, result ? result->name.c_str() : "none",
                  store.queries(), cached->price_cents);
      break;
    }
    case 'q':
      if (std::sscanf(line + 1, "%d", &id) != 1 || quote) {
        return 3;
      }
      quote = make_virtual(pricer, id);
      std::printf("quote=%d price=%d calls=%d\n", quote->product, quote->amount(),
                  pricer.original_calls());
      break;
    case 'd':
      std::printf("destroying=%d price=%d\n", quote->product, quote->amount());
      quote_destroy(quote);
      quote = nullptr;
      std::printf("destroyed=%d\n", destroyed);
      break;
    case 's':
      if (message) {
        return 3;
      }
      message = saved_status();
      std::printf("message=%s buffers=%d\n", message ? message : "null", buffers);
      break;
    case 'f':
      std::printf("releasing=%s\n", message);
      target_release(message);
      message = nullptr;
      std::printf("buffers=%d\n", buffers);
      break;
    case 'x':
      fail_allocation = !fail_allocation;
      std::printf("fail=%d\n", fail_allocation);
      break;
    case 'n': {
      if (std::sscanf(line + 1, "%d", &id) != 1) {
        return 3;
      }
      nested_originals = 0;
      try {
        const auto result = nesting(id);
        std::printf("nesting=%d originals=%d\n", result, nested_originals);
      } catch (const std::runtime_error& error) {
        std::printf("caught=%s originals=%d\n", error.what(), nested_originals);
      }
      break;
    }
    default:
      return 3;
    }
  }
  return buffers || quote ? 4 : 0;
}
