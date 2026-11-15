#include "writer.hpp"

namespace writer_entry {
Cell::Cell(int initial) : value(initial) {}
Cell::~Cell() {}
[[gnu::aligned(16)]] void poke(Cell &cell, int value) { cell.value = value; }

Knob::Knob(int initial) : value(initial) {}
Knob::~Knob() {}
[[gnu::aligned(16)]] void Knob::set(int next) { value = next; }
} // namespace writer_entry
