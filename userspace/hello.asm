[BITS 64]

section .text
   global _start


_start:
   mov rax, 1        ; syscall id 1 (example: print, exit, etc.)
   int 0x80
   ret
