#include <any>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <variant>
#include <unistd.h>

namespace complex_test {
struct Payload { long id; int value; char reserve[32]; };
struct Throwing {
  explicit Throwing(int) { throw 7; }
  Throwing(Throwing &&) noexcept(false) {}
};
__attribute__((noinline)) void delegated_chars(char *data) {
  data[1] = 'D';
  asm volatile("" ::: "memory");
  data[1] = 'h';
}
class Cell {
  std::string text_;
  std::variant<int, double, std::string, Throwing> variant_;
  std::any extension_;
  static void done() { asm volatile("" ::: "memory"); }
public:
  Cell() : text_("short"), variant_{1}, extension_{4} {}
  __attribute__((noinline)) void short_text() {
    auto *p = text_.data();
    p[1] = 'X';
    done();
    p[1] = 'h';
    done();
  }
  __attribute__((noinline)) void long_text() {
    auto *p = text_.data();
    p[33] = 'Y';
    done();
    p[33] = 'L';
    done();
  }
  __attribute__((noinline)) void delegated_text() {
    delegated_chars(text_.data());
    done();
  }
  __attribute__((noinline)) void variant_value() {
    if (auto *p = std::get_if<int>(&variant_)) {
      *p = 41;
      done();
      *p = 1;
    }
    done();
  }
  __attribute__((noinline)) void variant_text() {
    if (auto *p = std::get_if<std::string>(&variant_)) {
      p->data()[33] = 'V';
      done();
      p->data()[33] = 'L';
    }
    done();
  }
  __attribute__((noinline)) void variant_change() {
    variant_.emplace<double>(3.5);
    done();
  }
  __attribute__((noinline)) void variant_valueless() {
    variant_.emplace<double>(3.5);
    done();
  }
  __attribute__((noinline)) void any_text() {
    if (auto *p = std::any_cast<std::string>(&extension_)) {
      p->data()[33] = 'A';
      done();
      p->data()[33] = 'L';
    }
    done();
  }
  __attribute__((noinline)) void any_inline() {
    if (auto *p = std::any_cast<int>(&extension_)) {
      *p = 42;
      done();
      *p = 4;
    }
    done();
  }
  __attribute__((noinline)) void any_heap() {
    if (auto *p = std::any_cast<Payload>(&extension_)) {
      p->value = 72;
      done();
      p->value = 7;
    }
    done();
  }
  void set_up(const std::string &scenario) {
    if (scenario == "long_text") text_.assign(81, 'L');
    if (scenario == "variant_text") variant_.emplace<std::string>(81, 'L');
    if (scenario == "any_heap") extension_.emplace<Payload>(Payload{15, 7, {}});
    if (scenario == "any_text") extension_.emplace<std::string>(81, 'L');
    if (scenario == "variant_valueless") {
      try {
        variant_.emplace<Throwing>(0);
      } catch (int) {
        if (variant_.valueless_by_exception())
          return;
      }
      std::abort();
    }
  }
  bool check(const std::string &scenario) {
    return scenario == "short_text" || scenario == "delegated_text" ? text_ == "short" :
        scenario == "long_text" ? text_[33] == 'L' :
        scenario == "variant_value" ? std::get<int>(variant_) == 1 :
        scenario == "variant_text" ? std::get<std::string>(variant_)[33] == 'L' :
        scenario == "variant_change" || scenario == "variant_valueless" ?
            std::get<double>(variant_) == 3.5 :
        scenario == "any_text" ? std::any_cast<std::string&>(extension_)[33] == 'L' :
        scenario == "any_inline" ? std::any_cast<int>(extension_) == 4 :
        std::any_cast<Payload&>(extension_).value == 7;
  }
};
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  complex_test::Cell cell;
  const std::string test = argv[1];
  cell.set_up(test);
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  while (::access("go", F_OK))
    ::usleep(1000);
  if (test == "short_text") cell.short_text();
  else if (test == "long_text") cell.long_text();
  else if (test == "variant_value") cell.variant_value();
  else if (test == "variant_text") cell.variant_text();
  else if (test == "variant_change") cell.variant_change();
  else if (test == "variant_valueless") cell.variant_valueless();
  else if (test == "any_text") cell.any_text();
  else if (test == "delegated_text") {
    cell.delegated_text();
    if (std::puts("DELEGATED") == EOF)
      return 3;
  } else if (test == "any_inline") cell.any_inline();
  else if (test == "any_heap") cell.any_heap();
  else return 2;
  if (!cell.check(test))
    return 3;
  std::puts("DONE");
  while (::access("stop", F_OK))
    ::usleep(1000);
}
