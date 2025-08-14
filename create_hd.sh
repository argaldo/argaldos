make clean
make
dd if=/dev/zero bs=1M count=0 seek=64 of=argaldos.hdd
sudo sgdisk argaldos.hdd -n 1:2048 -t 1:ef00
./limine/limine bios-install argaldos.hdd
mformat -i argaldos.hdd@@1M
mmd -i argaldos.hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
mcopy -i argaldos.hdd@@1M bin/argaldos ::/boot
mcopy -i argaldos.hdd@@1M limine.cfg limine/limine-bios.sys ::/boot/limine
mcopy -i argaldos.hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
mcopy -i argaldos.hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT
mv argaldos.hdd bin
