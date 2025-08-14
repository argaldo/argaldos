#!/bin/bash
echo "CARALLO" > bin/test
dd if=/dev/zero of=bin/disk.vfat count=1024 bs=1024
sudo mkfs.vfat --mbr=y -F 32 bin/disk.vfat
mkdir tmp
sudo mount -t vfat bin/disk.vfat tmp
sudo cp bin/hello tmp
sudo cp bin/test tmp
sudo mkdir -p tmp/dir
sudo cp bin/hello tmp/dir
sudo umount tmp
rm -rf tmp
