#!/bin/bash
./create_iso.sh
./compile_userspace_test.sh
./create_data_disk.sh

qemu-system-x86_64 -fda floppy -usb -device usb-ehci,id=ehci -machine pc -drive file=argaldos.iso,format=raw,index=0,media=disk -drive file=bin/disk.vfat,index=1,if=ide,format=raw -no-shutdown 
