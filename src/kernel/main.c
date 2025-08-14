#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib/string.h>
#include <limine.h>
#include <drivers/terminal/terminal.h>
#include <drivers/pci/pci.h>
#include <drivers/usb/host/uhci.h>
#include <drivers/serial.h>
#include <arch/x64/idt.h>
#include <arch/x64/gdt.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/pmm.h>
#include <kernel/timer.h>
#include <kernel/mem.h>
#include <fs/fat/fat32.h>



// Set the base revision to 2, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_kernel_file_request kernelElfRequest = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmapRequest = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdmRequest = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_kernel_address_request kernelAddressRequest = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 2
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
    printk("Halting the CPU\n");
    asm ("cli");
    for (;;) {
        asm ("hlt");
    }
}

Kernel kernel = {0};

void init_kernel_data() {
    // bootloader information
    kernel.hhdm = (hhdmRequest.response)->offset;
    struct limine_memmap_response memmapResponse = *memmapRequest.response;
    kernel.memmapEntryCount = memmapResponse.entry_count;
    kernel.memmapEntries = memmapResponse.entries;
    kernel.kernelFile = *kernelElfRequest.response;
    kernel.kernelAddress = *kernelAddressRequest.response;
    // other info
    kernel.schedulerTurn = 0;
    kernel.serial_output = false;
    kernel.tick = 0;
    #ifdef ARGALDOS_KERNEL_DEBUG
      kernel.debug = true;
    #else
      kernel.debug = false;
    #endif
}


void print_banner() {
   printk("\n");
   printk("                       _     _  ___  ____  \n");
   printk("  __ _ _ __ __ _  __ _| | __| |/ _ \\/ ___| \n");
   printk(" / _` | '__/ _` |/ _` | |/ _` | | | \\___ \\ \n");
   printk("| (_| | | | (_| | (_| | | (_| | |_| |___) |\n");
   printk(" \\__,_|_|  \\__, |\\__,_|_|\\__,_|\\___/|____/ \n");
   printk("           |___/                           \n");
   printk("                                  %s\n\n",KERNEL_VERSION);
}


// The following will be our kernel's entry point.
// If renaming _start() to something else, make sure to change the
// linker script accordingly.
void kernel_start(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    setup_terminal(framebuffer);
    //printk("Welcome to argaldOS (%s)\n",VERSION);
    init_kernel_data();
    init_serial();
    print_banner();
    printk("[argaldOS:kernel:COR] argaldOS kernel is bootstrapping\n");
    initPMM();
    init_paging(); // Enable paging before GDT/IDT/other subsystems
    initGDT();
    initIDT();
    setup_timer();
    printk("[argaldOS:kernel:COR] Enabling interrupts\n");
    asm("sti");
    //pci_init();
    uhci_init(&kernel.usb_device);
    //fat32_init();


    printk("\nargaldOS has completely booted up. The kernel is idle now.\n\n");
    printk("Press F1 should you want to open a pseudo-terminal running in kernel space\n\n");

    while(1);

    // We should never get here :(

    printk("[argaldOS:kernel:COR] Preparing to hang\n");

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    /*for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }*/

    // We're done, just hang...
    hcf();
}
