#include "paging.h"
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <string.h>

// Track page tables we create for later mapping
#define MAX_EARLY_PAGE_TABLES 256
static uint64_t early_page_tables[MAX_EARLY_PAGE_TABLES];
static int num_early_page_tables = 0;

// Function declarations
static void add_early_page_table(uint64_t addr);
static uint64_t* alloc_table(int do_map);
static uint64_t* get_next_table(uint64_t* table, int index, int create);

// Simple page table structures for x86_64
static uint64_t* pml4 = 0;

// Allocate a new page-aligned page table
static uint64_t* alloc_table(int do_map) {
    printk("[paging] alloc_table: starting allocation\n");
    uint64_t* table = (uint64_t*)kmalloc();
    if (!table) {
        printk("[paging] alloc_table: kmalloc failed!\n");
        return NULL;
    }
    printk("[paging] alloc_table: kmalloc returned %p (HHDM %p)\n", table, (void*)((uint64_t)table + kernel.hhdm));
    printk("[paging] hhdm value: %p\n", (void*)kernel.hhdm);
    printk("[paging] attempting HHDM write test...\n");
    volatile uint64_t *test = (uint64_t*)((uint64_t)table + kernel.hhdm);
    printk("[paging] HHDM pointer created: %p\n", test);
    *test = 0xdeadbeef;
    printk("[paging] wrote 0xdeadbeef to hhdm-mapped page table\n");
    // Zero the table through HHDM one word at a time
    printk("[paging] alloc_table: zeroing table through HHDM at %p\n", test);
    volatile uint64_t* ptr = test;
    for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
        ptr[i] = 0;
    }
    printk("[paging] alloc_table: finished zeroing table\n");

    if (do_map && pml4 != 0) {
        // Only try to map if we're not setting up the initial PML4
        map_page((uint64_t)table, (uint64_t)table, PAGE_PRESENT | PAGE_RW);
        printk("[paging] alloc_table: mapped %p\n", table);
    }
    return table;
}

void init_paging() {
    printk("[paging] init_paging: start (HHDM offset: %p)\n", (void*)kernel.hhdm);
    
    // Reset early page table tracking
    num_early_page_tables = 0;
    
    // Initialize PML4
    pml4 = alloc_table(0); // Do not map PML4 itself
    if (!pml4) {
        printk("[paging] init_paging: FATAL - Failed to allocate PML4\n");
        return;
    }
    add_early_page_table((uint64_t)pml4);
    printk("[paging] init_paging: pml4 allocated at phys=%p (HHDM=%p)\n", pml4, (void*)((uint64_t)pml4 + kernel.hhdm));
    
    // First set up minimal page tables for kernel space
    printk("[paging] Setting up initial kernel mappings...\n");
    // Map first 1MB for kernel (reduced range for testing)
    printk("[paging] Starting initial 1MB identity mapping...\n");
    for (uint64_t addr = 0x100000; addr < 0x200000; addr += PAGE_SIZE) {
        printk("[paging] Attempting to map: virt=%p phys=%p\n", (void*)addr, (void*)addr);
        // Get indexes for debugging
        int pml4_idx = (addr >> 39) & 0x1FF;
        int pdpt_idx = (addr >> 30) & 0x1FF;
        int pd_idx = (addr >> 21) & 0x1FF;
        int pt_idx = (addr >> 12) & 0x1FF;
        printk("[paging] Map indexes: PML4=%d PDPT=%d PD=%d PT=%d\n", 
               pml4_idx, pdpt_idx, pd_idx, pt_idx);
        
        // Access PML4 through HHDM for debug
        volatile uint64_t* hhdm_pml4 = (uint64_t*)((uint64_t)pml4 + kernel.hhdm);
        printk("[paging] Current PML4[%d] = %p\n", pml4_idx, (void*)hhdm_pml4[pml4_idx]);
        
        map_page(addr, addr, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        printk("[paging] Mapped address %p successfully\n", (void*)addr);
    }
    printk("[paging] Completed initial 1MB identity mapping\n");
    
    // Now map all page tables we created
    printk("[paging] Starting to map %d page tables...\n", num_early_page_tables);
    for (int i = 0; i < num_early_page_tables; i++) {
        uint64_t pt_addr = early_page_tables[i];
        printk("[paging] Mapping page table %d: phys=%p\n", i, (void*)pt_addr);
        // Map each page table to itself
        map_page(pt_addr, pt_addr, PAGE_PRESENT | PAGE_RW);
        printk("[paging] Successfully mapped page table %d\n", i);
    }
    printk("[paging] Completed mapping all page tables\n");
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
static void add_early_page_table(uint64_t addr) {
    if (num_early_page_tables < MAX_EARLY_PAGE_TABLES) {
        early_page_tables[num_early_page_tables++] = addr;
    }
}

static uint64_t* get_next_table(uint64_t* table, int index, int create) {
    printk("[paging] --- Get Next Table ---\n");
    printk("[paging] Table physical=%p index=%d create=%d\n", 
           table, index, create);
    
    // Access table through HHDM
    volatile uint64_t* hhdm_table = (uint64_t*)((uint64_t)table + kernel.hhdm);
    printk("[paging] Table HHDM=%p\n", (void*)hhdm_table);
    
    // Read current entry
    uint64_t current = hhdm_table[index];
    printk("[paging] Current entry at index %d = %p\n", index, (void*)current);
    
    if (!(current & PAGE_PRESENT)) {
        printk("[paging] Entry not present\n");
        if (!create) {
            printk("[paging] Not creating new table\n");
            return 0;
        }
        
        // Allocate new table
        printk("[paging] Allocating new table...\n");
        uint64_t* next = alloc_table(0); // Don't map yet
        if (!next) {
            printk("[paging] FATAL: Failed to allocate new table\n");
            return 0;
        }
        printk("[paging] Allocated new table at %p\n", next);
        
        // Add to tracking list
        add_early_page_table((uint64_t)next);
        printk("[paging] Added to early page tables list (count=%d)\n", num_early_page_tables);
        
        // Set up entry
        uint64_t entry = ((uint64_t)next) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
        printk("[paging] Writing entry %p to index %d\n", (void*)entry, index);
        hhdm_table[index] = entry;
        
        return next;
    }
    
    uint64_t next_table = current & ~0xFFFULL;
    printk("[paging] Returning existing table %p\n", (void*)next_table);
    return (uint64_t*)next_table;
}

void map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    printk("[paging] ============ Mapping Page ============\n");
    printk("[paging] Virtual: %p Physical: %p Flags: %p\n", 
           (void*)virt_addr, (void*)phys_addr, (void*)flags);

    // Calculate indices
    int pml4_idx = (virt_addr >> 39) & 0x1FF;
    int pdpt_idx = (virt_addr >> 30) & 0x1FF;
    int pd_idx   = (virt_addr >> 21) & 0x1FF;
    int pt_idx   = (virt_addr >> 12) & 0x1FF;
    
    printk("[paging] Table indices: PML4=%d PDPT=%d PD=%d PT=%d\n", 
           pml4_idx, pdpt_idx, pd_idx, pt_idx);
    
    // Get PDPT
    printk("[paging] Getting PDPT from PML4[%d]...\n", pml4_idx);
    uint64_t* pdpt = get_next_table(pml4, pml4_idx, 1);
    if (!pdpt) {
        printk("[paging] FATAL: Failed to get/create PDPT\n");
        return;
    }
    printk("[paging] Got PDPT at %p\n", pdpt);
    
    // Get PD
    printk("[paging] Getting PD from PDPT[%d]...\n", pdpt_idx);
    uint64_t* pd = get_next_table(pdpt, pdpt_idx, 1);
    if (!pd) {
        printk("[paging] FATAL: Failed to get/create PD\n");
        return;
    }
    printk("[paging] Got PD at %p\n", pd);
    
    // Get PT
    printk("[paging] Getting PT from PD[%d]...\n", pd_idx);
    uint64_t* pt = get_next_table(pd, pd_idx, 1);
    if (!pt) {
        printk("[paging] FATAL: Failed to get/create PT\n");
        return;
    }
    printk("[paging] Got PT at %p\n", pt);

    // Set the page table entry
    uint64_t entry = (phys_addr & ~0xFFFULL) | (flags & 0xFFF) | PAGE_PRESENT;
    printk("[paging] Setting PT[%d] = %p\n", pt_idx, (void*)entry);
    
    // Access through HHDM
    volatile uint64_t* hhdm_pt = (uint64_t*)((uint64_t)pt + kernel.hhdm);
    hhdm_pt[pt_idx] = entry;
    
    printk("[paging] Successfully mapped page\n");
    printk("[paging] ====================================\n");
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
    pt[pt_idx] = 0;
}
