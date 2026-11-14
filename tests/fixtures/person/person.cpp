#include "person.hpp"

namespace demo {
Person::Person(int initial) : age(initial) {}
Person::~Person() = default;

void update(Person &person) {
  person.age = 1; // STORE_LINE
}

// Keep IDIV within the five bytes replaced by the writer entry redirect.
asm(R"(
  .text
  .globl _ZN4demo9remainderERNS_6PersonEi
  .type _ZN4demo9remainderERNS_6PersonEi,@function
_ZN4demo9remainderERNS_6PersonEi:
  .cfi_startproc
  movl (%rdi), %eax
  cltd
  idivl %esi
  movl %edx, (%rdi)
  ret
  .cfi_endproc
  .size _ZN4demo9remainderERNS_6PersonEi, .-_ZN4demo9remainderERNS_6PersonEi
)");
}
