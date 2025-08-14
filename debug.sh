objcopy --only-keep-debug bin/argaldos kernel.sym
gdb --command=gdbinit
