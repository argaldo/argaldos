#include <stdbool.h>
#include <kernel/shell.h>
#include <stdlib/string.h>
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/mem.h>
#include <limine.h>
#include <fs/fat/fat32.h>
#include <stdlib/string.h>
#include <kernel/util/hexdump.h>
#include <kernel/elf.h>
#include <drivers/disk.h>
#include <drivers/pci/pci.h>
#include <drivers/usb/host/uhci.h>

char* getCPU() {
    uint32_t ebx, ecx, edx;
    asm volatile("cpuid"
        : "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a"(0));
    static char toReturn[14];
    toReturn[13] = 0;
    *(uint32_t*)(toReturn    ) = ebx;
    *(uint32_t*)(toReturn + 4) = edx;
    *(uint32_t*)(toReturn + 8) = ecx;
    return toReturn;
}


void show_info() {
    // get total memory
    struct limine_memmap_entry **memmapEntries = kernel.memmapEntries;
    uint64_t memmapEntryCount = kernel.memmapEntryCount;
    uint64_t memSize = memmapEntries[memmapEntryCount - 2]->base + memmapEntries[memmapEntryCount - 2]->length;
    printk("\n"); 
    // draw text
    int memSizeMB = memSize / 10000000; // seems like too many zeros? for some reason it needs this amount.
    printk("Memory  %imb\n", memSizeMB);
    printk("Kernel  argaldOS %s\n", KERNEL_VERSION);
    printk("CPU     %s\n", getCPU());
    printk("\n");
}

bool process_command(char *input) {
       // printk("command %s",input);
        if (strcmp(input,"panic")){
                asm("int $0x03"); // debug isr
        } else if (strcmp(input,"fat")) {
            print_fat32_ebpb();
        } else if (strcmp(input,"run")) {
            uint8_t* ptr = (uint8_t*) kmalloc();
            uint8_t code[] = {0xcd,0x80,0xc3};
            memcpy(ptr + kernel.hhdm,code,3);
            int (*elf_entry_point)(void) = (int(*)(void))(ptr+kernel.hhdm);
            elf_entry_point();
        } else if (strcmp(input,"serial")) {
                if (kernel.serial_output) {
                        kernel.serial_output = false;
                        printk("[argaldOS:kernel:COR] Kernel serial output is OFF\n");
                } else {
                        kernel.serial_output = true;
                        printk("[argaldOS:kernel:COR] Kernel serial oputput is ON\n");
                }
        } else if (strcmp(input,"debug")) {
                if (kernel.debug) {
                        kernel.debug = false;
                        printk("[argaldOS:kernel:COR] Kernel debug traces are OFF\n");
                } else {
                        kernel.debug = true;
                        printk("[argaldOS:kernel:COR] Kernel debug traces are ON\n");
                }
        } else if (strcmp(input,"lspci")) {
                pci_init();
        } else if (strcmp(input,"exec")) {
                //uint8_t* buffer[4608] = {0};
                uint8_t buffer[4608] = {0};
                printk("Reading executable from disk\n");
                read_file("HELLO", buffer,4608);
                hexdump(buffer,4608);
                read_elf(buffer, true);
        } else if (strcmp(input,"reboot")) {
            // zeroing IDT and calling a non-defined interrupt
            uint64_t zero = 0;
            asm("lidt %0" : : "m"(zero)); // get rid of the IDT
            asm("int $0x90");
        } else if(strcmp(input,"usb")) {
            uhci_print_registers(&kernel.usb_device,kernel.uhci_io_base_address);
        } else if (strcmp(input,"usb reset")) {
            uhci_reset();
        } else if (strcmp(input,"kmalloc")) {
            uint8_t* ptr;
            ptr = (uint8_t*) kmalloc();
            char buf[9];
            uint64_to_hex_string((uint64_t) (ptr + kernel.hhdm), buf);
            printk("\n8192 byte block dynamically allocated by the kernel: address 0x%s\n", buf);
        } else if (strcmp(input,"help")) {
                printk("\nCommands available:\n");
                printk(" - help       Shows this help menu\n");
                printk(" - panic      Force a kernel panic\n");
                printk(" - info       Shows some system info\n");
                printk(" - kmalloc    Tests kmalloc kernel function\n");
                printk(" - fat        Prints FAT32 EBPB from IDE2\n");
                printk(" - reboot     Reboot machine\n");
                printk(" - exec       Exec ELF executable reading from IDE2 FAT32\n");
                printk(" - debug      Toggles Kernel debug status {ON|OFF}\n");
                printk(" - lspci      Triggers PCI enumeration and prints the results\n");
                printk(" - serial     Toggles Kernel serial output {ON|OFF}\n");
                printk(" - usb        Prints USB PCI IO registers\n");
                printk(" - usb reset  USB bus global reset\n");
                printk(" - exit/quit  Exit pseudo-shell\n");
                printk("\n");
        } else if (strcmp(input,"exit") || strcmp(input,"quit")) {
                return true;
        } else if (strcmp(input,"info")) {
                show_info();
                return false;
        } else {
                printk("ERROR: command not found\n");
        }
        return false;
}
