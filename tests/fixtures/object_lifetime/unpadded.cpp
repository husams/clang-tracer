#include "unpadded.hpp"

namespace life {
Virtual::Virtual(int initial) : value(initial) {}
Virtual::~Virtual() {}

namespace {
volatile int observed;
void consume(int value) { observed = value; }
}

void Inline::consume(int value) { life::consume(value); }

Delegating::Delegating() : Delegating(7) {}
Delegating::Delegating(int initial) : value(initial) {}
Delegating::~Delegating() {}

Callback::Callback(int initial) : value(initial), callback(consume) {}
Callback::~Callback() {}
} // namespace life

void delete_virtual(life::Virtual *object) { delete object; }
void destroy_virtual(life::Virtual *object) { object->~Virtual(); }

void write_virtual(life::Virtual &object, int value) { object.value = value; }
void write_inline(life::Inline &object, int value) { object.value = value; }
void write_delegating(life::Delegating &object, int value) { object.value = value; }
void write_callback(life::Callback &object, int value) {
  object.value = value;
  object.callback(value);
}
