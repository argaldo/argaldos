#include <kernel/printk.h>

void hcf(void) {
    printk("Halting the CPU\n");
    asm ("cli");
    for (;;) {
        asm ("hlt");
    }
}
