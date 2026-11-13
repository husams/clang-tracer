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

void exercise_hidden_writer() {
  life::Callback object(7);
  write_hidden(object, 11);
  hidden_writer_call(object, 13);
}

[[gnu::noinline]] void exercise_text() {
  auto *heap = new life::Text(7);
  write_text(*heap, 13);
  delete_text(heap);
  alignas(life::Text) unsigned char storage[sizeof(life::Text)];
  auto *placed = new (storage) life::Text(17);
  write_text(*placed, 23);
  destroy_text(placed);
}

[[gnu::noinline]] void exercise_plain() {
  for (int initial : {7, 17}) {
    life::Plain object(initial);
    write_plain(object, initial + 6);
  }
}

void exercise_tail_ctor() {
  life::TailCtor direct(7);
  write_tail_ctor(direct, 13);
  alignas(life::TailCtor) unsigned char storage[sizeof(life::TailCtor)];
  auto *object = reinterpret_cast<life::TailCtor *>(storage);
  make_tail_ctor(object, 17);
  write_tail_ctor(*object, 23);
  object->~TailCtor();
}

void exercise_tail_dtor() {
  life::TailDtor direct(7);
  write_tail_dtor(direct, 13);
  alignas(life::TailDtor) unsigned char storage[sizeof(life::TailDtor)];
  auto *object = new (storage) life::TailDtor(17);
  write_tail_dtor(*object, 23);
  destroy_tail_dtor(object);
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
    case 'h':
      exercise_hidden_writer();
      break;
    case 't':
      exercise_text();
      break;
    case 'p':
      exercise_plain();
      break;
    case 'x':
      exercise_tail_ctor();
      break;
    case 'y':
      exercise_tail_dtor();
      break;
    case 'q':
      return 0;
    default:
      continue;
    }
    std::puts("DONE");
  }
}
