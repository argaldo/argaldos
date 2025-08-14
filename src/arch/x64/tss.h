/* Header for ../tss.c, as part of the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repo for more information.
 */

#include <kernel/kernel.h>

#ifndef TSS_H
#define TSS_H

#define KERNEL_STACK_PTR 0xFFFFFFFFFFFFF000LL

void initTSS();

#endif
