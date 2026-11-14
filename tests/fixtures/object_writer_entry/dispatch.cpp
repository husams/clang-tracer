#include "writer.hpp"

namespace writer_entry {
#if defined(TRACE_STORED_WRITER)
void select_writer(Writer &writer) { writer = poke; }
#else
Writer chosen() { return poke; }
#endif
void apply(Cell &cell, int value, Writer writer) { writer(cell, value); }
void turn(Knob &knob, int value) { knob.set(value); }
} // namespace writer_entry
