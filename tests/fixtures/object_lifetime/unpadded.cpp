#include "unpadded.hpp"

namespace life {
Text::Text(int initial) : value(initial), label(initial == 7 ? "requires heap allocation for its buffer" : "small") {}
Text::~Text() {}

Plain::Plain(int initial) : value(initial), label(initial == 7 ? "requires heap allocation for its buffer" : "small") {}
Plain::~Plain() {}

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
void delete_text(life::Text *object) { delete object; }
void destroy_text(life::Text *object) { object->~Text(); }

void write_virtual(life::Virtual &object, int value) { object.value = value; }
void write_inline(life::Inline &object, int value) { object.value = value; }
void write_delegating(life::Delegating &object, int value) { object.value = value; }
void write_text(life::Text &object, int value) { object.value = value; }
void write_plain(life::Plain &object, int value) { object.value = value; }
void write_callback(life::Callback &object, int value) {
  object.value = value;
  object.callback(value);
}
