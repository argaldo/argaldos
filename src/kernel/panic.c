/* Panic screen for the SpecOS kernel project.
 *
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repo for details.
 * This is a loose parody of the blue screen of death on Windows.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib/string.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <kernel/kernel.h>
#include <limine.h>

struct elfSectionHeader {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addressAlign;
    uint64_t entrySize;
} __attribute__ ((packed));

struct symtabEntry {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
} __attribute__((packed));

void assert(bool condition) {
    if (condition) {
        printk("\nASSERT: True\n");
    } else {
        printk("\nFailed assert.\n");
        asm("cli; hlt");
    }
}

void getFunctionName(uint64_t address) {
    struct limine_kernel_file_response kernelElfResponse = kernel.kernelFile;
    struct limine_file kernelFile = *kernelElfResponse.kernel_file;
    char* kernelFileStart = (char*) kernelFile.address;
    //uint64_t kernelFileLength = kernelFile.size;
    if (kernelFileStart[0] == 0x7F && kernelFileStart[1] == 'E' && kernelFileStart[2] == 'L' && kernelFileStart[3] == 'F' &&
        kernelFileStart[4] == 2) {
        // it's a valid 64 bit elf file!
        // parse the section header table
        struct elfSectionHeader *sectionHeaderOffset = (struct elfSectionHeader*)(*(uint64_t*)((kernelFileStart + 40)) + ((uint64_t)kernelFileStart));
        uint16_t sectionHeaderNumEntries = *(uint16_t*)(kernelFileStart + 60);
        // parse the section header string table
        uint16_t sectionHeaderStringTableIndex = *(uint16_t*)(kernelFileStart + 62);
        char* sectionHeaderStringTableOffset = (char*)(sectionHeaderOffset[sectionHeaderStringTableIndex].offset + ((uint64_t)kernelFileStart));
        // now look through each section header entry, comparing the name, and find the offset of .strtab and .symtab
        struct symtabEntry *symtabOffset = 0;
        char* strtabOffset = 0;
        uint64_t symtabIndex = 0;
        // this iterates through the section headers in the ELF
        for (int i = 0; i < sectionHeaderNumEntries; i++) {
            // check if it's .symtab or .strtab by comparing it's string in the section header string table
            if (strcmp(&sectionHeaderStringTableOffset[sectionHeaderOffset[i].name], ".strtab")) {
                //printk("found strtab\n");
                strtabOffset = (char*)(sectionHeaderOffset[i].offset + (uint64_t)kernelFileStart);
            }
            if (strcmp(&sectionHeaderStringTableOffset[sectionHeaderOffset[i].name], ".symtab")) {
                //printk("found symtab\n");
                symtabOffset = (struct symtabEntry*)(sectionHeaderOffset[i].offset + (uint64_t)kernelFileStart);
                symtabIndex = i;
            }
        }
        // now go through .symtab, looking for an entry who's address is equal to `address`
        for (long unsigned int i = 0; i < sectionHeaderOffset[symtabIndex].size / sizeof(struct symtabEntry); i++) {
            if (address >= symtabOffset[i].value && address < symtabOffset[i].value + symtabOffset[i].size) {
                // found! parse the string table to find the actual thingy
                printk("   %s\n", (&strtabOffset[symtabOffset[i].name]));
                return;
            }
        }
        // if it got to this point, it's invalid.
        printk("  <INVALID>\n");
    } else {
        printk("  <INVALID>");
    }
} 

struct stackFrame {
    struct stackFrame* rbp;
    uint64_t rip;
};

void exceptionHandler(struct IDTEFrame registers) {
    printk("[argaldOS:kernel:COR] KERNEL PANIC!\n");
    printk("\nException occurred. RIP: 0x%x\n", registers.rip);
    char labelDesignate[30];
    char* label = (char*)labelDesignate;
    if (registers.type == 14)
        label = "Page Fault\0";
    else if (registers.type == 13)
        label = "General Protection Fault.\0";
    else
        uint16_to_string(registers.type, label);
    labelDesignate[29] = 0; // just in case
    kpanic(label, registers);
    asm("cli; hlt");
}

void stackTrace(uint64_t rbp, uint64_t rip) {
    printk("\n\n==== Stack Trace: ====\n\n");
    // first the faulting address
    printk(" 0x%x", rip);
    if (rip)
        getFunctionName(rip);
    else {
        printk("\n");
        asm("cli;hlt");
    }
      
    // and now for the rest of it
    struct stackFrame *stack = (struct stackFrame*)rbp;
    while (stack) {
        printk(" 0x%x", stack->rip);
        if (stack->rip)
            getFunctionName(stack->rip);
        else
            printk("\n");
        stack = stack->rbp;
    }
}

void kpanic(char* exception, struct IDTEFrame registers) {
    printk("\n============= DEBUG ============="); // 24
    printk("\n\n Fault:      %s\n", exception);
    printk(" Error code: 0b%b", registers.ss);
    char bufferGDTR[19];
    char bufferIDTR[19];
    char bufferCR3[19];
    char bufferCR2[19];
    uint64_t gdtrVal;
    uint64_t idtrVal;
    uint64_t cr3Val;
    uint64_t cr2Val;
    asm("sgdt %0" : "=m"(gdtrVal));
    asm("sidt %0" : "=m"(idtrVal));
    asm("mov %%cr3, %0" : "=r"(cr3Val));
    asm("mov %%cr2, %0" : "=r"(cr2Val));
    uint64_to_hex_string(gdtrVal, bufferGDTR);
    bufferGDTR[18] = '\0';
    uint64_to_hex_string(idtrVal, bufferIDTR);
    bufferIDTR[18] = '\0';
    uint64_to_hex_string(cr3Val, bufferCR3);
    bufferCR3[18] = '\0';
    uint64_to_hex_string(cr2Val, bufferCR2);
    printk("\n\n=== Register Dump: ===\n\n GDTR: 0x"); // 22
    printk(bufferGDTR);
    printk("\n IDTR: 0x");
    printk(bufferIDTR);
    printk("\n CR3: 0x");
    printk(bufferCR3);
    printk("\n CR2: 0x");
    printk(bufferCR2);
    //stackTrace(10, registers.rbp, registers.rip);/*
    stackTrace(registers.rbp, registers.rip);/*
    printk("\n\n== Last 10 stdio outputs: ==\n\n");
    for (int i = 0; i < 10; i++)
        printk(kernel.last10[i]);
    */
    printk("\n[argaldOS:kernel:COR] kpanic: disabling interrupts and halting the cpu\n");
    asm("cli; hlt");
}
