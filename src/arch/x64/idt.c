/* 64 bit IDT implementation for the SpecOS kernel project.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "idt.h"
#include <kernel/printk.h>
#include <drivers/ports.h>
#include <drivers/keyboard.h>
#include <kernel/kernel.h>
#include <kernel/pmm.h>
#include <kernel/panic.h>

// and the thingies to make it do stuff

// User/kernel separation: safely copy string from user space
#define USER_SPACE_TOP 0x00007FFFFFFFFFFFULL
#define MAX_USER_STRING 256

// Returns 0 on success, -1 on error
int copy_from_user(char *dest, const char *user_src, size_t maxlen) {
    // Check user_src is in user space
    uintptr_t addr = (uintptr_t)user_src;
    if (addr == 0 || addr > USER_SPACE_TOP) return -1;
    size_t i = 0;
    for (; i < maxlen - 1; ++i) {
        // Volatile to avoid compiler optimizing out
        volatile char c = user_src[i];
        if (addr + i > USER_SPACE_TOP) return -1;
        dest[i] = c;
        if (c == '\0') break;
    }
    dest[i < maxlen ? i : maxlen - 1] = '\0';
    return 0;
}



void initIRQ();

// takes: IDT vector number (eg. 0x01 for divide by 0 exception), a pointer to an ISR (aka the function it calls), & the flags
void idtSetDescriptor(uint8_t vect, void* isrThingy, uint8_t gateType, uint8_t dpl, struct IDTEntry *IDTAddr) {
    //printk("idtSetDescriptor\n");
    struct IDTEntry* thisEntry = &IDTAddr[vect];
    // set the thingies
    // isr offset
    thisEntry->offset1 = (uint64_t)isrThingy & 0xFFFF; // first 16 bits
    thisEntry->offset2 = ((uint64_t)isrThingy >> 16) & 0xFFFF; // next 16 bits (I think? not sure if this is wrong)
    thisEntry->offset3 = ((uint64_t)isrThingy >> 32) & 0xFFFFFFFF; // next 32 bits (again, not sure if i did that right.)
    // gdt segment
    thisEntry->segmentSelector = 0x08; // addresses kernel mode code segment in gdt
    // some "flags". idk why the wiki calls these flags tbh.
    thisEntry->ist = 0; // idk what this should be but apparently it can just be 0
    thisEntry->gateType = gateType; // Trap or interrupt gate?
    thisEntry->dpl = dpl;
    thisEntry->present = 1;
}

// define more stuff for PIC (hardware, yay!)

void remapPIC() {
    printk("[argaldOS:kernel:IDT] Remapping Programmable Interrupt Controller (PIC)\n");
    // ICW1: Start initialization of PIC
    port_byte_out(0x20, 0x11); // Master PIC
    port_byte_out(0xA0, 0x11); // Slave PIC

    // ICW2: Set interrupt vector offsets
    port_byte_out(0x21, 0x20); // Master PIC vector offset
    port_byte_out(0xA1, 0x28); // Slave PIC vector offset

    // ICW3: Tell Master PIC there is a slave PIC at IRQ2 (0000 0100)
    port_byte_out(0x21, 0x04);
    // Tell Slave PIC its cascade identity (0000 0010)
    port_byte_out(0xA1, 0x02);

    // ICW4: Set PIC to x86 mode
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);

    // Mask interrupts on both PICs
    port_byte_out(0x21, 0xFF);
    port_byte_out(0xA1, 0xFF);
}


extern void divideException();
extern void debugException();
extern void breakpointException();
extern void overflowException();
extern void boundRangeExceededException();
extern void invalidOpcodeException();
extern void deviceNotAvaliableException();
extern void doubleFaultException();
extern void coprocessorSegmentOverrunException();
extern void invalidTSSException();
extern void segmentNotPresentException();
extern void stackSegmentFaultException();
extern void generalProtectionFaultException();
extern void pageFaultException();
extern void floatingPointException();
extern void alignmentCheckException(); 
extern void machineCheckException();
extern void simdFloatingPointException();
extern void virtualisationException();

// FIXME    This function only unmasks the said IRQ, masking the rest
void unmaskIRQ(int IRQ) {
    //printk("unmask IRQ %d\n",IRQ);
    if (IRQ < 8)
        port_byte_out(0x21, ~(1 << (IRQ % 8)));
    else
        port_byte_out(0xA1, ~(1 << (IRQ % 8)));
}


// FIXME    This function only unmasks the said IRQ, masking the rest
void maskIRQ(int IRQ) {
    //printk("mask IRQ %d\n",IRQ);
    if (IRQ < 8)
        port_byte_out(0x21, (1 << (IRQ % 8)));
    else
        port_byte_out(0xA1, (1 << (IRQ % 8)));
}

// FIXME this is a hack to unmask IRQ0 (timer) and IRQ1 (keyboard) at once
void unmask() {
        port_byte_out(0x21,0xFC);
}


// Minimal interrupt frame for x86_64 (GCC/Clang ABI)
struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};


// Example syscall handlers
void sys_print() {
    printk("[SYSCALL] sys_print called!\n");
}
void sys_exit() {
    printk("[SYSCALL] sys_exit called!\n");
}

// POSIX-like open syscall: rdi = filename pointer
void sys_open(const char* user_filename) {
    char kfilename[MAX_USER_STRING];
    if (copy_from_user(kfilename, user_filename, MAX_USER_STRING) == 0) {
        printk("[SYSCALL] sys_open called for file: %s\n", kfilename);
        // TODO: implement real file open logic, return fd
    } else {
        printk("[SYSCALL] sys_open: invalid user pointer!\n");
    }
}


__attribute__((interrupt))
void syscallISR(struct interrupt_frame* frame) {
    uint64_t syscall_id, filename_ptr;
    asm volatile ("mov %%rax, %0" : "=r"(syscall_id));
    asm volatile ("mov %%rdi, %0" : "=r"(filename_ptr));
    printk("[argaldOS:kernel:IDT] IRQ 0x80 [syscall] received, id=%llu\n", (unsigned long long)syscall_id);
    switch (syscall_id) {
        case 1:
            sys_print();
            break;
        case 2:
            sys_open((const char*)filename_ptr);
            break;
        default:
            printk("[SYSCALL] Unknown syscall id: %llu\n", syscall_id);
            break;
    }
    port_byte_out(0x20, 0x20); // Acknowledge interrupt from master PIC
    asm("sti");
}

__attribute__((interrupt))
void pepe(void*) {
   printk("[argaldOS:kernel:IDT] IRQ 0x81 [TEST] has been received\n");
        port_byte_out(0x20, 0x20); // Acknowledge interrupt from master PIC
        asm("sti");
}

__attribute__((interrupt))
void mouseMoveISR(void*) {
    printk("Mouse moved.\n");
}


__attribute__((interrupt))
void taskSwitchISR(void*) {
        kernel.tick = kernel.tick + 1;
        port_byte_out(0x20, 0x20); // Acknowledge interrupt from master PIC
        asm("sti");
}

void initIRQ(struct IDTEntry *IDTAddr) {
    printk("[argaldOS:kernel:IDT] Populating Interrupt Service Requests on IDT table...\n");
    remapPIC();
    // map some stuff
    idtSetDescriptor(0x20, &taskSwitchISR, 14, 0, IDTAddr);
    idtSetDescriptor(0x21, &isr_keyboard, 14, 0, IDTAddr);
    idtSetDescriptor(0x80, &syscallISR, 14, 0, IDTAddr);
    idtSetDescriptor(0x81, &pepe, 14, 0, IDTAddr);
    printk("[argaldOS:kernel:IDT] Unmasking keyboard's IRQ (0x01)\n");
    //unmaskIRQ(1); // keyboard
    //unmaskIRQ(0); // pit
    unmask();
    printk("[argaldOS:kernel:IDT] Unmasking syscall IRQ (0x80)\n");
    unmaskIRQ(0x80);
    unmaskIRQ(0x81);
    // all the exceptions
    idtSetDescriptor(0, &divideException, 15, 0, IDTAddr);
    idtSetDescriptor(1, &debugException, 15, 0, IDTAddr);
    idtSetDescriptor(3, &breakpointException, 15, 0, IDTAddr);
    idtSetDescriptor(4, &overflowException, 15, 0, IDTAddr);
    idtSetDescriptor(5, &boundRangeExceededException, 15, 0, IDTAddr);
    idtSetDescriptor(6, &invalidOpcodeException, 15, 0, IDTAddr);
    idtSetDescriptor(7, &deviceNotAvaliableException, 15, 0, IDTAddr);
    idtSetDescriptor(8, &doubleFaultException, 15, 0, IDTAddr);
    idtSetDescriptor(9, &coprocessorSegmentOverrunException, 15, 0, IDTAddr);
    idtSetDescriptor(10, &invalidTSSException, 15, 0, IDTAddr);
    idtSetDescriptor(11, &segmentNotPresentException, 15, 0, IDTAddr);
    idtSetDescriptor(12, &stackSegmentFaultException, 15, 0, IDTAddr);
    idtSetDescriptor(13, &generalProtectionFaultException, 15, 0, IDTAddr);
    idtSetDescriptor(14, &pageFaultException, 15, 0, IDTAddr);
    idtSetDescriptor(16, &floatingPointException, 15, 0, IDTAddr);
    idtSetDescriptor(17, &alignmentCheckException, 15, 0, IDTAddr);
    idtSetDescriptor(18, &machineCheckException, 15, 0, IDTAddr);
    idtSetDescriptor(19, &simdFloatingPointException, 15, 0, IDTAddr);
    idtSetDescriptor(20, &virtualisationException, 15, 0, IDTAddr);
}

void initIDT() {
    printk("[argaldOS:kernel:IDT] Trying to initialise IDT & IRQs...\n");
    struct IDTEntry *IDTAddr = (struct IDTEntry*) (kmalloc() + kernel.hhdm);
    kernel.IDTPtr.offset = (uintptr_t)IDTAddr;
    kernel.IDTPtr.size = ((uint16_t)sizeof(struct IDTEntry) *  256) - 1;
    printk("[argaldOS:kernel:IDT] Loading empty IDT table...\n");
    asm volatile("lidt %0" : : "m"(kernel.IDTPtr));
    //printk("after lidt\n");
    initIRQ(IDTAddr);
    //printk("initIDT done\n");
}
