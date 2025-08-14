#include "paging.h"
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <string.h>

// Simple page table structures for x86_64
static uint64_t* pml4 = 0;

// Allocate a new page-aligned page table
static uint64_t* alloc_table(int do_map) {
    uint64_t* table = (uint64_t*)kmalloc();
    printk("[paging] alloc_table: kmalloc returned %p\n", table);
    if (do_map) {
        map_page((uint64_t)table, (uint64_t)table, PAGE_PRESENT | PAGE_RW);
        printk("[paging] alloc_table: mapped %p\n", table);
    }
    memset((void*)((uint64_t)table + kernel.hhdm), 0, PAGE_SIZE);
    return table;
}

void init_paging() {
    printk("[paging] init_paging: start\n");
    pml4 = alloc_table(0); // Do not map PML4 itself
    printk("[paging] init_paging: pml4 at %p\n", pml4);
    // Identity map first 16MB for kernel and user (for demo)
    for (uint64_t addr = 0; addr < 0x1000000; addr += PAGE_SIZE) {
        map_page(addr, addr, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }
    printk("[paging] init_paging: identity-mapped 16MB\n");
    // Map kernel higher half (example: 0xFFFF800000000000)
    for (uint64_t addr = 0; addr < 0x1000000; addr += PAGE_SIZE) {
        map_page(0xFFFF800000000000ULL + addr, addr, PAGE_PRESENT | PAGE_RW);
    }
    printk("[paging] init_paging: higher-half mapped\n");
    // Load PML4 into CR3
    printk("[paging] init_paging: loading CR3 with %p\n", pml4);
    asm volatile ("mov %0, %%cr3" : : "r"(pml4));
    printk("[paging] Paging enabled.\n");
}

// Helper to get/create next level table
static uint64_t* get_next_table(uint64_t* table, int index, int create) {
    uint64_t* hhdm_table = (uint64_t*)((uint64_t)table + kernel.hhdm);
    if (!(hhdm_table[index] & PAGE_PRESENT)) {
        if (!create) return 0;
        uint64_t* next = alloc_table(1); // Map lower-level tables
        hhdm_table[index] = ((uint64_t)next) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }
    return (uint64_t*)(hhdm_table[index] & ~0xFFFULL);
}

void map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    int pml4_idx = (virt_addr >> 39) & 0x1FF;
    int pdpt_idx = (virt_addr >> 30) & 0x1FF;
    int pd_idx   = (virt_addr >> 21) & 0x1FF;
    int pt_idx   = (virt_addr >> 12) & 0x1FF;
    uint64_t* pdpt = get_next_table(pml4, pml4_idx, 1);
    uint64_t* pd   = get_next_table(pdpt, pdpt_idx, 1);
    uint64_t* pt   = get_next_table(pd, pd_idx, 1);
    uint64_t* hhdm_pt = (uint64_t*)((uint64_t)pt + kernel.hhdm);
    hhdm_pt[pt_idx] = (phys_addr & ~0xFFFULL) | (flags & 0xFFF) | PAGE_PRESENT;
}

void unmap_page(uint64_t virt_addr) {
    int pml4_idx = (virt_addr >> 39) & 0x1FF;
    int pdpt_idx = (virt_addr >> 30) & 0x1FF;
    int pd_idx   = (virt_addr >> 21) & 0x1FF;
    int pt_idx   = (virt_addr >> 12) & 0x1FF;
    uint64_t* pdpt = get_next_table(pml4, pml4_idx, 0);
    if (!pdpt) return;
    uint64_t* pd   = get_next_table(pdpt, pdpt_idx, 0);
    if (!pd) return;
    uint64_t* pt   = get_next_table(pd, pd_idx, 0);
    if (!pt) return;
    uint64_t* hhdm_pt = (uint64_t*)((uint64_t)pt + kernel.hhdm);
    hhdm_pt[pt_idx] = 0;
}
