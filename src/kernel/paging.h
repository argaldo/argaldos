#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <stddef.h>

// Page size for x86_64
#define PAGE_SIZE 4096

// Paging structures
#define PML4_ENTRIES 512
#define PDPTE_ENTRIES 512
#define PDE_ENTRIES 512
#define PTE_ENTRIES 512

// Page table entry flags
#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4

// Paging API
void init_paging();
void map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
void unmap_page(uint64_t virt_addr);

#endif
