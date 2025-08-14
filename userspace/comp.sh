gcc -s -O3 -Os -ffreestanding -fdata-sections -static -ffunction-sections hello.c -o hello -Wl,--gc-sections -Wl,--strip-all 
file hello
readelf -h hello
echo ""
nm hello | nl | tail -1
ldd hello
ls -lh hello
