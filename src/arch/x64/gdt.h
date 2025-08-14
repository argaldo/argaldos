/* Header file for GDT initialisation as part of the SpecOS project.
 * This is for ../gdt.c and only requires defining one function.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repo for more information.
 */

#include <kernel/kernel.h>

#ifndef GDT_H
#define GDT_H


struct GDTEntry {
    uint16_t limit1;
    uint16_t base1;
    uint8_t base2;
    uint8_t accessByte;
    uint8_t limit2 : 4;
    uint8_t flags : 4;
    uint8_t base3;
} __attribute__((packed));

void initGDT();

#endif
