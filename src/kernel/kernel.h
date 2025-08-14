#include <limine.h>
#include <stdbool.h>
#include <drivers/pci/pci.h>
#include <drivers/usb/host/uhci.h>

#ifndef KERNEL_H
#define KERNEL_H

#define KERNEL_VERSION "v.0.0.1"

struct largestSection {
    int maxBegin;
    int maxLength;
    int bitmapReserved; // in bytes
};

struct idtr {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct idtr;

struct GDTPtr {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct GDTPtr;

struct TSS {
    uint32_t rsvd0;
    uint64_t rsp0;
    uint32_t rsvd1[23]; // I *think* that should be 23 elements?
} __attribute__((packed));




typedef struct {
      uint64_t hhdm; // limine higher half direct mapping
      uint64_t memmapEntryCount;
      struct largestSection largestSect; // info about location of the pmm's bitmap
      struct limine_memmap_entry **memmapEntries;
      struct limine_kernel_file_response kernelFile;
      struct limine_kernel_address_response kernelAddress;
      struct limine_module_response moduleFiles;
      uint16_t schedulerTurn;
      struct idtr IDTPtr;
      struct GDTPtr GDTR; // the pointer thingy to the GDT
      struct TSS tss;
      bool serial_output;
      bool debug;
      uint32_t uhci_io_base_address;
      uhci_device_io usb_device;
      volatile uint64_t tick;

} Kernel;

extern Kernel kernel;

#endif
