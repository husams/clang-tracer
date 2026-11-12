#pragma once

template <class T>
__attribute__((noinline)) T echo(T value) {
  volatile T saved = value;
  return saved;
}
