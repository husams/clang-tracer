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

extern "C" [[gnu::noinline]] void lifetime_side_effect(int value) { observed = value; }

void Inline::consume(int value) { life::consume(value); }

Delegating::Delegating() : Delegating(7) {}
Delegating::Delegating(int initial) : value(initial) {}
Delegating::~Delegating() {}

Callback::Callback(int initial) : value(initial), callback(consume) {}
Callback::~Callback() {}
TailCtor::~TailCtor() { asm volatile("" : : "r"(this) : "memory"); }
TailDtor::TailDtor(int initial) : value(initial), spare(0) {}
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
void write_tail_ctor(life::TailCtor &object, int value) { object.value = value; }
void write_tail_dtor(life::TailDtor &object, int value) { object.value = value; }

// Model the optimized early return after a conditional branch. Neither the
// one-byte return nor its predecessor can host an independent near redirect.
// Spell out the wrappers so both compilers emit the same incoming tail jumps.
asm(R"(
  .text
  .globl _ZN4life8TailCtorC1Ei
  .type _ZN4life8TailCtorC1Ei,@function
_ZN4life8TailCtorC1Ei:
  movl %esi,(%rdi)
  movl %esi,4(%rdi)
  testl $1,%esi
  jne .Lctor_slow
  ret
.Lctor_slow:
  movl %esi,%edi
  pushq %rax
  call lifetime_side_effect
  popq %rax
  ret
  .size _ZN4life8TailCtorC1Ei,.-_ZN4life8TailCtorC1Ei

  .globl _ZN4life8TailDtorD1Ev
  .type _ZN4life8TailDtorD1Ev,@function
_ZN4life8TailDtorD1Ev:
  movl $0,4(%rdi)
  movq %rdi,%rax
  testb $1,(%rdi)
  jne .Ldtor_slow
  ret
.Ldtor_slow:
  movl (%rdi),%edi
  pushq %rax
  call lifetime_side_effect
  popq %rax
  ret
  .size _ZN4life8TailDtorD1Ev,.-_ZN4life8TailDtorD1Ev

  .globl make_tail_ctor
  .type make_tail_ctor,@function
make_tail_ctor:
  jmp _ZN4life8TailCtorC1Ei
  .size make_tail_ctor,.-make_tail_ctor

  .globl destroy_tail_dtor
  .type destroy_tail_dtor,@function
destroy_tail_dtor:
  jmp _ZN4life8TailDtorD1Ev
  .size destroy_tail_dtor,.-destroy_tail_dtor

  .globl _ZN4life8TailDtorD2Ev
  .type _ZN4life8TailDtorD2Ev,@function
  .set _ZN4life8TailDtorD2Ev,_ZN4life8TailDtorD1Ev
)");
