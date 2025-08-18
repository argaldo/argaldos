#include "paging.h"
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <string.h>
#include <kernel/util/utils.h>

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

    // We no longer try to map tables here to avoid recursion
    return table;
}

static void map_range(volatile uint64_t* hhdm_pml4_ptr, uint64_t start, uint64_t end, uint64_t flags) {
    for (uint64_t addr = start & ~0xFFFULL; addr < end; addr += 0x1000) {
        int pml4_idx = (addr >> 39) & 0x1FF;
        int pdpt_idx = (addr >> 30) & 0x1FF;
        int pd_idx = (addr >> 21) & 0x1FF;
        int pt_idx = (addr >> 12) & 0x1FF;

        volatile uint64_t* pdpt;
        if (!(hhdm_pml4_ptr[pml4_idx] & PAGE_PRESENT)) {
            uint64_t* new_pdpt = alloc_table(0);
            hhdm_pml4_ptr[pml4_idx] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_RW;
        }
        pdpt = (uint64_t*)((hhdm_pml4_ptr[pml4_idx] & ~0xFFFULL) + kernel.hhdm);

        volatile uint64_t* pd;
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            uint64_t* new_pd = alloc_table(0);
            pdpt[pdpt_idx] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_RW;
        }
        pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);

        volatile uint64_t* pt;
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            uint64_t* new_pt = alloc_table(0);
            pd[pd_idx] = ((uint64_t)new_pt) | PAGE_PRESENT | PAGE_RW;
        }
        pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);

        pt[pt_idx] = addr | flags | PAGE_PRESENT;
    }
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
    
    volatile uint64_t* hhdm_pml4_ptr = (uint64_t*)((uint64_t)pml4 + kernel.hhdm);
    printk("[paging] init_paging: pml4 allocated at phys=%p (HHDM=%p)\n", pml4, (void*)hhdm_pml4_ptr);

    // 1. Identity map first 2MB (crucial bootloader and kernel areas)
    printk("[paging] Mapping first 2MB identity...\n");
    map_range(hhdm_pml4_ptr, 0, 0x200000, PAGE_PRESENT | PAGE_RW);

    // 2. Get and map the current code region
    uint64_t code_start = (uint64_t)&init_paging & ~0xFFFULL;
    printk("[paging] Mapping code region at %p\n", (void*)code_start);
    map_range(hhdm_pml4_ptr, code_start & ~0x1FFFFF, (code_start + 0x200000) & ~0xFFF, PAGE_PRESENT | PAGE_RW);

    // 3. Get and map the current stack region
    uint64_t current_rsp;
    asm volatile ("mov %%rsp, %0" : "=r"(current_rsp));
    uint64_t stack_start = current_rsp & ~0x1FFFFF; // Round down to 2MB
    printk("[paging] Mapping stack region at %p\n", (void*)stack_start);
    map_range(hhdm_pml4_ptr, stack_start, stack_start + 0x200000, PAGE_PRESENT | PAGE_RW);

    // 4. Map the same regions in HHDM
    printk("[paging] Mapping HHDM regions...\n");
    map_range(hhdm_pml4_ptr, kernel.hhdm, kernel.hhdm + 0x200000, PAGE_PRESENT | PAGE_RW); // First 2MB
    map_range(hhdm_pml4_ptr, kernel.hhdm + (code_start & ~0x1FFFFF), 
             kernel.hhdm + ((code_start + 0x200000) & ~0xFFF), PAGE_PRESENT | PAGE_RW);
    map_range(hhdm_pml4_ptr, kernel.hhdm + stack_start, 
             kernel.hhdm + stack_start + 0x200000, PAGE_PRESENT | PAGE_RW);
    
    // First set up essential kernel mappings
    printk("[paging] Setting up essential mappings...\n");
    
    // Identity map current code page (containing init_paging function)
    uint64_t code_page = ((uint64_t)&init_paging) & ~0xFFFULL;
    
    // First map one page directly through page table manipulation
    int pml4_idx = (code_page >> 39) & 0x1FF;
    int pdpt_idx = (code_page >> 30) & 0x1FF;
    int pd_idx = (code_page >> 21) & 0x1FF;
    int pt_idx = (code_page >> 12) & 0x1FF;
    
    printk("[paging] Mapping code page at %p\n", (void*)code_page);
    
    // Create page tables manually
    volatile uint64_t* hhdm_pml4 = (uint64_t*)((uint64_t)pml4 + kernel.hhdm);
    
    // Allocate PDPT if needed
    if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t* pdpt = alloc_table(0);
        hhdm_pml4[pml4_idx] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_RW;
    }
    volatile uint64_t* pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
    
    // Allocate PD if needed
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t* pd = alloc_table(0);
        pdpt[pdpt_idx] = ((uint64_t)pd) | PAGE_PRESENT | PAGE_RW;
    }
    volatile uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
    
    // Allocate PT if needed
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t* pt = alloc_table(0);
        pd[pd_idx] = ((uint64_t)pt) | PAGE_PRESENT | PAGE_RW;
    }
    volatile uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
    
    // Map the page
    pt[pt_idx] = (code_page) | PAGE_PRESENT | PAGE_RW;
    
    printk("[paging] Mapped initial code page successfully\n");
    
    // Now map HHDM region for the code page
    uint64_t hhdm_addr = code_page + kernel.hhdm;
    pml4_idx = (hhdm_addr >> 39) & 0x1FF;
    pdpt_idx = (hhdm_addr >> 30) & 0x1FF;
    pd_idx = (hhdm_addr >> 21) & 0x1FF;
    pt_idx = (hhdm_addr >> 12) & 0x1FF;
    
    // Repeat the same process for HHDM mapping
    if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t* pdpt = alloc_table(0);
        hhdm_pml4[pml4_idx] = ((uint64_t)pdpt) | PAGE_PRESENT | PAGE_RW;
    }
    pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
    
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t* pd = alloc_table(0);
        pdpt[pdpt_idx] = ((uint64_t)pd) | PAGE_PRESENT | PAGE_RW;
    }
    pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
    
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t* pt = alloc_table(0);
        pd[pd_idx] = ((uint64_t)pt) | PAGE_PRESENT | PAGE_RW;
    }
    pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
    
    // Map the HHDM page
    pt[pt_idx] = (code_page) | PAGE_PRESENT | PAGE_RW;
    
    printk("[paging] Mapped HHDM code page successfully\n");

    // Get current stack pointer to ensure stack is mapped
    uint64_t stack_ptr;
    asm volatile ("mov %%rsp, %0" : "=r"(stack_ptr));
    uint64_t stack_page = stack_ptr & ~0xFFFULL;
    
    // Map a few pages around the stack pointer to ensure we have enough stack space
    for (uint64_t addr = stack_page - 0x4000; addr <= stack_page + 0x4000; addr += 0x1000) {
        printk("[paging] Mapping stack page at %p\n", (void*)addr);
        
        // Map both identity and HHDM for stack
        // Identity mapping
        pml4_idx = (addr >> 39) & 0x1FF;
        pdpt_idx = (addr >> 30) & 0x1FF;
        pd_idx = (addr >> 21) & 0x1FF;
        pt_idx = (addr >> 12) & 0x1FF;
        
        if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
            uint64_t* new_pdpt = alloc_table(0);
            hhdm_pml4[pml4_idx] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_RW;
        }
        pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            uint64_t* new_pd = alloc_table(0);
            pdpt[pdpt_idx] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_RW;
        }
        pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            uint64_t* new_pt = alloc_table(0);
            pd[pd_idx] = ((uint64_t)new_pt) | PAGE_PRESENT | PAGE_RW;
        }
        pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
        
        pt[pt_idx] = addr | PAGE_PRESENT | PAGE_RW;
        
        // HHDM mapping for stack
        uint64_t hhdm_addr = addr + kernel.hhdm;
        pml4_idx = (hhdm_addr >> 39) & 0x1FF;
        pdpt_idx = (hhdm_addr >> 30) & 0x1FF;
        pd_idx = (hhdm_addr >> 21) & 0x1FF;
        pt_idx = (hhdm_addr >> 12) & 0x1FF;
        
        if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
            uint64_t* new_pdpt = alloc_table(0);
            hhdm_pml4[pml4_idx] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_RW;
        }
        pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            uint64_t* new_pd = alloc_table(0);
            pdpt[pdpt_idx] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_RW;
        }
        pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            uint64_t* new_pt = alloc_table(0);
            pd[pd_idx] = ((uint64_t)new_pt) | PAGE_PRESENT | PAGE_RW;
        }
        pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
        
        pt[pt_idx] = addr | PAGE_PRESENT | PAGE_RW;
    }
    
    printk("[paging] Mapped stack pages successfully\n");

    // Map the page table pages themselves in both identity and HHDM space
    for (int i = 0; i < num_early_page_tables; i++) {
        uint64_t pt_addr = early_page_tables[i];
        uint64_t hhdm_pt_addr = pt_addr + kernel.hhdm;
        
        // Identity map
        pml4_idx = (pt_addr >> 39) & 0x1FF;
        pdpt_idx = (pt_addr >> 30) & 0x1FF;
        pd_idx = (pt_addr >> 21) & 0x1FF;
        pt_idx = (pt_addr >> 12) & 0x1FF;
        
        if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
            uint64_t* new_pdpt = alloc_table(0);
            hhdm_pml4[pml4_idx] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_RW;
        }
        pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            uint64_t* new_pd = alloc_table(0);
            pdpt[pdpt_idx] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_RW;
        }
        pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            uint64_t* new_pt = alloc_table(0);
            pd[pd_idx] = ((uint64_t)new_pt) | PAGE_PRESENT | PAGE_RW;
        }
        pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
        
        pt[pt_idx] = pt_addr | PAGE_PRESENT | PAGE_RW;
        
        // HHDM map
        pml4_idx = (hhdm_pt_addr >> 39) & 0x1FF;
        pdpt_idx = (hhdm_pt_addr >> 30) & 0x1FF;
        pd_idx = (hhdm_pt_addr >> 21) & 0x1FF;
        pt_idx = (hhdm_pt_addr >> 12) & 0x1FF;
        
        if (!(hhdm_pml4[pml4_idx] & PAGE_PRESENT)) {
            uint64_t* new_pdpt = alloc_table(0);
            hhdm_pml4[pml4_idx] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_RW;
        }
        pdpt = (uint64_t*)((hhdm_pml4[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            uint64_t* new_pd = alloc_table(0);
            pdpt[pdpt_idx] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_RW;
        }
        pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
        
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            uint64_t* new_pt = alloc_table(0);
            pd[pd_idx] = ((uint64_t)new_pt) | PAGE_PRESENT | PAGE_RW;
        }
        pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
        
        pt[pt_idx] = pt_addr | PAGE_PRESENT | PAGE_RW;
    }
    
    printk("[paging] Mapped all page tables successfully\n");
    // Load PML4 into CR3
    // 5. Map all page tables both identity and HHDM
    printk("[paging] Mapping page tables...\n");
    for (int i = 0; i < num_early_page_tables; i++) {
        uint64_t pt_addr = early_page_tables[i] & ~0xFFF;
        // Identity map
        map_range(hhdm_pml4, pt_addr, pt_addr + 0x1000, PAGE_PRESENT | PAGE_RW);
        // HHDM map
        map_range(hhdm_pml4, kernel.hhdm + pt_addr, kernel.hhdm + pt_addr + 0x1000, PAGE_PRESENT | PAGE_RW);
    }

    // Verify critical page table entries before switching
    printk("[paging] Verifying page table entries:\n");
    
    // Get current instruction pointer to verify code mapping
    uint64_t rip;
    asm volatile ("lea (%%rip), %0" : "=r"(rip));
    uint64_t code_page = rip & ~0xFFFULL;
    
    // Check code page mapping
    int pml4_idx = (code_page >> 39) & 0x1FF;
    int pdpt_idx = (code_page >> 30) & 0x1FF;
    int pd_idx   = (code_page >> 21) & 0x1FF;
    int pt_idx   = (code_page >> 12) & 0x1FF;
    
    volatile uint64_t* hhdm_pml4_check = (uint64_t*)((uint64_t)pml4 + kernel.hhdm);
    printk("[paging] Code page %p mapping:\n", (void*)code_page);
    printk("  PML4[%d] = %p\n", pml4_idx, (void*)hhdm_pml4_check[pml4_idx]);
    
    if (hhdm_pml4_check[pml4_idx] & PAGE_PRESENT) {
        volatile uint64_t* pdpt = (uint64_t*)((hhdm_pml4_check[pml4_idx] & ~0xFFFULL) + kernel.hhdm);
        printk("  PDPT[%d] = %p\n", pdpt_idx, (void*)pdpt[pdpt_idx]);
        
        if (pdpt[pdpt_idx] & PAGE_PRESENT) {
            volatile uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + kernel.hhdm);
            printk("  PD[%d] = %p\n", pd_idx, (void*)pd[pd_idx]);
            
            if (pd[pd_idx] & PAGE_PRESENT) {
                volatile uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + kernel.hhdm);
                printk("  PT[%d] = %p\n", pt_idx, (void*)pt[pt_idx]);
            }
        }
    }
    
    // Flush TLB before paging change
    asm volatile ("invlpg (%0)" :: "r"(pml4) : "memory");
    
    // First load CR3 with physical address of PML4
    printk("[paging] Loading CR3 with %p\n", pml4);
    asm volatile ("mov %0, %%cr3" :: "r"(pml4) : "memory");
    
    // Quick test that we can still execute
    asm volatile ("nop");
    printk("[paging] CR3 loaded successfully\n");
    
    // Now update CR0
    uint64_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x10000; // Set WP bit
    printk("[paging] Updating CR0 to %p\n", (void*)cr0);
    asm volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");
    
    // Final verification
    asm volatile ("nop");
    printk("[paging] Paging enabled successfully\n");
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
