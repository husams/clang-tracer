#include "unpadded.hpp"

#include <cerrno>
#include <cstdio>
#include <initializer_list>
#include <new>

#include <fcntl.h>
#include <unistd.h>

namespace {
void exercise_virtual() {
  auto *heap = new life::Virtual(7);
  write_virtual(*heap, 13);
  delete_virtual(heap);
  alignas(life::Virtual) unsigned char storage[sizeof(life::Virtual)];
  for (int initial : {7, 17}) {
    auto *placed = new (storage) life::Virtual(initial);
    write_virtual(*placed, initial + 6);
    destroy_virtual(placed);
  }
}

void exercise_inline() {
  life::Inline object(7);
  write_inline(object, 13);
}

void exercise_delegating() {
  life::Delegating object;
  write_delegating(object, 13);
}

void exercise_callback() {
  life::Callback object(7);
  write_callback(object, 13);
}
} // namespace

int main() {
  if (::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK))
    return 2;
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::puts("READY");
  for (;;) {
    char command;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (!count)
      return 0;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 3;
      ::usleep(1000);
      continue;
    }
    switch (command) {
    case 'v':
      exercise_virtual();
      break;
    case 'i':
      exercise_inline();
      break;
    case 'd':
      exercise_delegating();
      break;
    case 'c':
      exercise_callback();
      break;
    case 'q':
      return 0;
    default:
      continue;
    }
    std::puts("DONE");
  }
}
