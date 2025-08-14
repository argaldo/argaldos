/* TSS setup for the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repo for more information.
 */

#include <arch/x64/tss.h>
#include <kernel/kernel.h>

void initTSS() {
    kernel.tss.rsp0 = KERNEL_STACK_PTR;
}
