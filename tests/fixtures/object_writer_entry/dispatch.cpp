#include "writer.hpp"

namespace writer_entry {
Writer chosen() { return poke; }
void apply(Cell &cell, int value, Writer writer) { writer(cell, value); }
void turn(Knob &knob, int value) { knob.set(value); }
} // namespace writer_entry
