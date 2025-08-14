/* Header file for ../idt.c, which is a 64 bit IDT implementation for the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 */

#include <kernel/kernel.h>

#ifndef IDT_H
#define IDT_H

// User/kernel separation helper
int copy_from_user(char *dest, const char *user_src, size_t maxlen);

struct IDTEntry {
    uint16_t offset1;
    uint16_t segmentSelector;
    uint8_t ist : 3;
    uint8_t reserved0 : 5;
    uint8_t gateType : 4;
    uint8_t empty : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset2;
    uint32_t offset3;
    uint32_t reserved;
} __attribute__((packed));

struct IDTEntry;

void initIDT();

void unmaskIRQ(int IRQ);

void maskIRQ(int IRQ);

#endif
