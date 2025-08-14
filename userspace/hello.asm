[BITS 64]

section .text
   global _start




section .data
filename: db 'HELLO',0

section .text
   global _start

_start:
   mov rax, 1        ; syscall id 1 (example: print, exit, etc.)
   int 0x80
   mov rax, 2        ; syscall id 2 (open)
   lea rdi, [rel filename] ; pointer to filename
   int 0x80
   ret
