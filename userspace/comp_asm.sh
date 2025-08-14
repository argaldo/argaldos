nasm -f elf64 hello.asm
ld -s -o ../bin/hello hello.o
readelf -h hello
file hello
nm hello
ldd hello
ls -l hello
