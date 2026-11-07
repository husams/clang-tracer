.text

.macro pc name
.globl pilot_pc_\name
pilot_pc_\name:
.endm

.globl pilot_work
.type pilot_work,@function
pilot_work:
  .cfi_startproc
  pc save
  pushq %rbx
  .cfi_adjust_cfa_offset 8
  .cfi_offset %rbx, -16
  pc clear
  xorl %ebx, %ebx
  pc rounds
  movl %edi, %r8d
  pc test_signal
  testl %ecx, %ecx
  pc skip_signal
  je .Ltarget
  pc number
  movl $234, %eax                   # x86-64 tgkill
  pc pid
  movl %esi, %edi
  pc tid
  movl %edx, %esi
  pc sigusr1
  movl $10, %edx
  pc send_signal
  syscall
.Ltarget:
  pc target
  leaq .Ladd(%rip), %r9
.Lloop:
  pc parity
  testl $1, %r8d
  pc branch
  jz .Leven
  pc indirect
  call *%r9
  pc skip_direct
  jmp .Lnext
.Leven:
  pc direct
  call .Ladd
.Lnext:
  pc decrement
  decl %r8d
  pc repeat
  jnz .Lloop
  pc value
  movl %ebx, %eax
  pc restore
  popq %rbx
  .cfi_adjust_cfa_offset -8
  .cfi_restore %rbx
  pc finish
  ret
.Ladd:
  pc add
  addl $3, %ebx
  pc return
  ret
  .cfi_endproc
.size pilot_work,.-pilot_work
.globl pilot_work_end
pilot_work_end:

.globl pilot_native
.type pilot_native,@function
pilot_native:
  subq $8, %rsp
.globl pilot_native_trap
pilot_native_trap:
  int3
  call pilot_work
.globl pilot_native_done
pilot_native_done:
  addq $8, %rsp
  ret
.size pilot_native,.-pilot_native

.section .note.GNU-stack,"",@progbits
