/* Allocator for the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 * This uses a simple bitmap allocator for 4096 byte size page frames, but I may switch to a buddy allocator in the future.
 */

#include <stdint.h>
#include <stdbool.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/mem.h>
#include <stdlib/binop.h>
#include <stdlib/string.h>

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

// Map memory at a specific address (if not already used). Returns pointer or NULL.
void* map_at_addr(uint64_t addr, uint64_t size) {
    // For a simple flat model, just check if the address is in usable RAM and return it.
    // (No real protection, but enough for a hobby OS.)
    // You may want to add checks for overlap, but for now, just return the address + hhdm.
    // WARNING: This is not safe for production OSes!
    return (void*)(addr + kernel.hhdm);
}

struct pmemBitmap {
    uint8_t bitmap[0];
};

struct pmemData {
    uint8_t data[0];
};

void initPMM() {
    printk("[argaldOS:kernel:PMM] Starting Physical Memory Manager (PMM)...\n");
    // get the memmap
    uint64_t memmapEntriesCount = kernel.memmapEntryCount;
    struct limine_memmap_entry **memmapEntries = kernel.memmapEntries;
    uint64_t maxBegin = 0;
    uint64_t maxLength = 0;
    
    // Find the largest available entry to use for allocation
    for (uint64_t i = 0; i < memmapEntriesCount; i++) {
        printk("[PMM] Memory region %d: base=%p len=%p type=%d\n", 
               i, (void*)memmapEntries[i]->base, 
               (void*)memmapEntries[i]->length, 
               memmapEntries[i]->type);
        if (memmapEntries[i]->type == LIMINE_MEMMAP_USABLE &&
            memmapEntries[i]->length > maxLength &&
            memmapEntries[i]->base >= 0x100000) { // Start after 1MB
            maxBegin = memmapEntries[i]->base;
            maxLength = memmapEntries[i]->length;
        }   
    }
    printk("[PMM] Selected memory region: base=%p len=%p\n", (void*)maxBegin, (void*)maxLength);
    kernel.largestSect.maxBegin = maxBegin;
    kernel.largestSect.maxLength = maxLength;

    // Calculate bitmap size needed
    uint64_t total_pages = maxLength / 4096;
    uint64_t bitmap_bytes = (total_pages + 7) / 8; // Round up to nearest byte
    uint64_t bitmap_pages = (bitmap_bytes + 4095) / 4096; // Round up to nearest page
    uint64_t bitmapReserved = bitmap_pages * 4096;
    
    printk("[PMM] Bitmap info: total_pages=%d bitmap_bytes=%d bitmap_pages=%d reserved=%d\n",
           total_pages, bitmap_bytes, bitmap_pages, bitmapReserved);
    
    kernel.largestSect.bitmapReserved = bitmapReserved;
    
    // Zero the entire bitmap region
    volatile uint8_t* bitmap = (uint8_t*)(maxBegin + kernel.hhdm);
    memset((void*)bitmap, 0, bitmapReserved);
    // Mark bitmap region as used in the bitmap itself
    // bitmap_pages was already calculated above
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        uint64_t byte_index = i / 8;
        uint64_t bit_index = i % 8;
        bitmap[byte_index] = setBit(bitmap[byte_index], bit_index, 1);
    }
    
    printk("[PMM] Reserved %d pages for bitmap at %p\n", bitmap_pages, (void*)maxBegin);
    printk("[PMM] First allocatable page at %p\n", (void*)(maxBegin + bitmapReserved));
    printk("[argaldOS:kernel:PMM] Successfully initialized physical memory allocator.\n");
}

// Ooh, fancy! Dynamic memory management, kmalloc and kfree!
// something I gotta remember sometimes is that, unlike userspace heap malloc,
// this doesn't take a size. It will always allocate 1024 bytes
void* kmalloc() {
    printk("[PMM] kmalloc: maxBegin=%p maxLength=%x bitmapReserved=%x hhdm=%p\n", 
           (void*)kernel.largestSect.maxBegin, kernel.largestSect.maxLength, 
           kernel.largestSect.bitmapReserved, (void*)kernel.hhdm);
    
    // Calculate bitmap base address correctly
    uint8_t* bitmap_base = (uint8_t*)(kernel.largestSect.maxBegin + kernel.hhdm);
    
    // Calculate start of allocatable memory (after bitmap)
    uint64_t data_start = kernel.largestSect.maxBegin + kernel.largestSect.bitmapReserved;
    
    // Search for a free page
    for (int b = 0; b < kernel.largestSect.bitmapReserved; b++) {
        uint8_t* bitmap_ptr = bitmap_base + b;
        for (int y = 0; y < 8; y++) {
            if (!getBit(*bitmap_ptr, y)) {
                // Mark page as used
                *bitmap_ptr = setBit(*bitmap_ptr, y, 1);
                
                // Calculate physical address after bitmap region
                uint64_t frame_index = (b * 8) + y;
                void* addr = (void*)(data_start + (frame_index * 4096));
                
                printk("[PMM] kmalloc: allocated addr=%p (frame=%d)\n", addr, frame_index);
                return addr;
            }
        }
    }
    // if it got to this point, no memory address is avaliable.
    printk("[PMM] kmalloc: FAILED to allocate!\n");
    printk("KERNEL ERROR: Not enough physical memory space to allocate.\nHalting device.\n");
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
