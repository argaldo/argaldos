#!/bin/bash
./create_hd.sh
./compile_userspace_test.sh
./create_data_disk.sh
#qemu-system-x86_64 -drive file=argaldos.hdd,format=raw,index=0,media=disk -no-reboot -no-shutdown
#qemu-system-x86_64 -drive file=argaldos.hdd,format=raw,index=0,media=disk -no-reboot -no-shutdown -serial unix:/tmp/serial.socket,server,nowait
#qemu-system-x86_64 -s -m 128m -drive file=bin/argaldos.hdd,format=raw,if=ide,index=0,media=disk -drive file=bin/disk.vfat,index=1,if=ide,format=raw -no-shutdown
#qemu-system-x86_64 -s -m 128m -drive file=bin/argaldos.hdd,format=raw,if=ide,index=0,media=disk -drive file=bin/disk.vfat,index=1,if=ide,format=raw -usb -device ich9-usb-uhci1,id=uhci -device usb-kbd -no-shutdown
#qemu-system-x86_64 -s -m 128m -drive file=bin/argaldos.hdd,format=raw,if=ide,index=0,media=disk -drive file=bin/disk.vfat,index=1,if=ide,format=raw -usb -device usb-tablet,bus=usb-bus.0,port=1 -no-shutdown
qemu-system-x86_64 -s -m 128m -drive file=bin/argaldos.hdd,format=raw,if=ide,index=0,media=disk -drive file=bin/disk.vfat,index=1,if=ide,format=raw -usb -device usb-mouse,bus=usb-bus.0,port=1,id=argaldomouse -device usb-tablet,bus=usb-bus.0,port=2,id=argaldotablet -no-shutdown
#qemu-system-x86_64 -drive file=argaldos.hdd,format=raw,index=0,media=disk -no-reboot -nographic
