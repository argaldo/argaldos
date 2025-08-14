symbol-file kernel.sym
target remote localhost:1234
break uhci.c:60
continue
