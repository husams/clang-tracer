#pragma once

namespace writer_entry {
struct Cell {
  int value;
  explicit Cell(int initial);
  ~Cell();
};
void poke(Cell &cell, int value);
using Writer = void (*)(Cell &, int);
Writer chosen();
void apply(Cell &cell, int value, Writer writer);

struct Knob {
  int value;
  explicit Knob(int initial);
  virtual ~Knob();
  virtual void set(int value);
};
void turn(Knob &knob, int value);
} // namespace writer_entry
