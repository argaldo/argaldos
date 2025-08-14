// Map memory at a specific address (if not already used). Returns pointer or NULL.
void* map_at_addr(uint64_t addr, uint64_t size) {
    // For a simple flat model, just check if the address is in usable RAM and return it.
    // (No real protection, but enough for a hobby OS.)
    // You may want to add checks for overlap, but for now, just return the address + hhdm.
    // WARNING: This is not safe for production OSes!
    return (void*)(addr + kernel.hhdm);
}
/* Allocator for the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 * This uses a simple bitmap allocator for 4096 byte size page frames, but I may switch to a buddy allocator in the future.
 */

#include <stdint.h>
#include <stdbool.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <stdlib/binop.h>
#include <stdlib/string.h>

struct pmemBitmap {
    uint8_t bitmap[0];
};

struct pmemData {
    uint8_t data[0];
};

void initPMM() {
    printk("[argaldOS:kernel:PMM] Starting Physical Memory Manager (PMM)...\n");
    // get the memmap;
    uint64_t memmapEntriesCount = kernel.memmapEntryCount;
    struct limine_memmap_entry **memmapEntries = kernel.memmapEntries;
    int maxBegin = 0;
    int maxLength = 0;
    // Find the largest avaliable entry to use for allocation
    for (uint64_t i = 0; i < memmapEntriesCount; i++) {
        if (memmapEntries[i]->type == LIMINE_MEMMAP_USABLE &&
            memmapEntries[i]->length > maxLength) {
            // set it as this one
            maxBegin = memmapEntries[i]->base;
            maxLength = memmapEntries[i]->length;
        }   
    }
    kernel.largestSect.maxBegin = maxBegin;
    kernel.largestSect.maxLength = maxLength;
    // WARNING: Heavily commented section, because I was trying to get my brain to understand it
    // now that it's gotten the largest avaliable/reclaimable segment, allocate the first bit of space for the bitmap
    // and the rest for the actual data.
    // first of all, find the amount needed to allocate for the bitmap
    int bitmapReserved;
    int n = 1;
    while (1) { // keeps adding another page frame for the bitmap until it's enough
        if (n * 4096 * 8 > // bitmap size (in bits)
            (maxLength / 4096) - n) { // number of page frame to allocate
            // it's enough space: set this to be the correct number of page frames to reserve for the bitmap (in bytes)
            bitmapReserved = n * 4096;
            break;
        }
        n++;
    }
    kernel.largestSect.bitmapReserved = bitmapReserved;
    // initialise that point in memory
    // set the bitmap to a proper array, set all to 0
    struct pmemBitmap memBuffBitmap;
    for (int i = 0; i < bitmapReserved; i++)
        memBuffBitmap.bitmap[i] = 0;
    // set all of the data to 0
    struct pmemData memBuffData;
    for (int i = 0; i < maxBegin - bitmapReserved; i++)
        memBuffData.data[i] = 0;
    // put it at the right point in memory
    *((struct pmemBitmap*) maxBegin + kernel.hhdm) = memBuffBitmap;
    *((struct pmemData*) (maxBegin + bitmapReserved + kernel.hhdm)) = memBuffData;
    // display stuff for debugging
    char b1[9];
    char b2[9];
    char b3[9];
    uint64_to_hex_string(maxBegin, b1);
    uint64_to_hex_string(maxLength, b2);
    uint64_to_hex_string(bitmapReserved, b3);
    printk("[argaldOS:kernel:PMM] Successfully initialised physical memory allocator.\n");
}

// just a basic utility
static uint8_t setBit(uint8_t byte, uint8_t bitPosition, bool setTo) {
    if (bitPosition < 8) {
        if (setTo)
            return byte |= (1 << bitPosition);
        else 
            return byte &= ~(1 << bitPosition);
    } else {
        return byte;
    }
}

// Ooh, fancy! Dynamic memory management, kmalloc and kfree!
// something I gotta remember sometimes is that, unlike userspace heap malloc,
// this doesn't take a size. It will always allocate 1024 bytes
#include <stdint.h>
void* kmalloc() {
    // go to the start of the largest part of memory, and look thru it.
    for (int b = 0; b < kernel.largestSect.bitmapReserved; b++) {
        // look through each bit checking if it's avaliable. If it is, return the matching memory address.
        for (int y = 0; y < 8; y++) {
            if (!getBit(*((uint8_t*)(kernel.largestSect.maxBegin + b + kernel.hhdm)), y)) {
                // avaliable frame found!
                // set it to be used
                *((uint8_t*)(kernel.largestSect.maxBegin + b + kernel.hhdm)) = setBit(*((uint8_t*)(kernel.largestSect.maxBegin + b + kernel.hhdm)), y, 1);
                // the actual frame index is just `byte + bit`
                return (void*)((kernel.largestSect.maxBegin + (((b * 8) + y) * 4096)) + kernel.largestSect.bitmapReserved);
            }
        }
    }
    // if it got to this point, no memory address is avaliable.
    // print an error message and halt the computer
    printk("KERNEL ERROR: Not enough physical memory space to allocate.\nHalting device.");
    asm("cli; hlt");
    // and make the compiler happy by returning an arbitrary value
    return (void*) 0x00;
}

void kfree(void* location) {
    // get the memory address to change
    // pageFrameNumber = (location - (kernel.largestSect.maxBegin + kernel.largestSect.bitmapReserved)) / 1024
    // bitmapMemAddress = (pageFrameNumber >> 3) + kernel.largestSect.maxBegin + kernel.hhdm
    uint32_t pfNum = (((uint64_t)location) - (kernel.largestSect.maxBegin + kernel.largestSect.bitmapReserved)) / 4096;
    uint64_t bitmapMemAddr = (pfNum >> 3) + kernel.largestSect.maxBegin + kernel.hhdm;
    // now get the thing at that address, and set the right thingy to 0
    uint8_t bitmapByte = *((uint8_t*)bitmapMemAddr);
    // get the bit that needs to be changed
    uint8_t bitToChange = pfNum % 8;
    // and change it, putting the new version at the correct address
    uint8_t newByte = setBit(bitmapByte, bitToChange, 0);
    *((uint8_t*)bitmapMemAddr) = newByte;
    // and it should be free'd now :D
}
