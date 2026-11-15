#pragma once

#include <string>

namespace life {
struct Text {
  int value;
  std::string label;
  explicit Text(int initial);
  virtual ~Text();
};

struct Plain {
  int value;
  std::string label;
  explicit Plain(int initial);
  ~Plain();
};

struct Virtual {
  int value;
  explicit Virtual(int initial);
  virtual ~Virtual();
};

struct Inline {
  int value;
  explicit Inline(int initial) : value(initial) {}
  ~Inline() { consume(value); }
  static void consume(int value);
};

struct Delegating {
  int value;
  Delegating();
  explicit Delegating(int initial);
  ~Delegating();
};

struct Callback {
  int value;
  void (*callback)(int);
  explicit Callback(int initial);
  ~Callback();
};

struct TailCtor {
  int value, spare;
  explicit TailCtor(int initial);
  ~TailCtor();
};

struct TailDtor {
  int value, spare;
  explicit TailDtor(int initial);
  ~TailDtor();
};
} // namespace life

extern "C" void delete_virtual(life::Virtual *);
extern "C" void destroy_virtual(life::Virtual *);
extern "C" void write_virtual(life::Virtual &, int);
extern "C" void write_inline(life::Inline &, int);
extern "C" void write_delegating(life::Delegating &, int);
extern "C" void write_callback(life::Callback &, int);
extern "C" void write_text(life::Text &, int);
extern "C" void write_plain(life::Plain &, int);
extern "C" void delete_text(life::Text *);
extern "C" void destroy_text(life::Text *);
extern "C" void make_tail_ctor(life::TailCtor *, int);
extern "C" void destroy_tail_dtor(life::TailDtor *);
extern "C" void write_tail_ctor(life::TailCtor &, int);
extern "C" void write_tail_dtor(life::TailDtor &, int);
extern "C" void hidden_writer_call(life::Callback &, int);
extern "C" void write_hidden(life::Callback &, int);
