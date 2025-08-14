#include <stdint.h>

#ifndef PCI_H
#define PCI_H

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_DATA_ADDRESS 0xCFC


// PCI configuration space: https://en.wikipedia.org/wiki/PCI_configuration_space
#define PCI_CONFIG_VENDOR_ID_OFFSET 0x00
#define PCI_CONFIG_DEVICE_ID_OFFSET 0x02
#define PCI_CONFIG_DEVICE_CLASS_OFFSET 0x0A
#define PCI_CONFIG_HEADER_TYPE_OFFSET 0x0E
#define PCI_CONFIG_BAR0_OFFSET 0x10
#define PCI_CONFIG_BAR1_OFFSET 0x14
#define PCI_CONFIG_BAR2_OFFSET 0x18
#define PCI_CONFIG_BAR3_OFFSET 0x1C
#define PCI_CONFIG_BAR4_OFFSET 0x20
#define PCI_CONFIG_BAR5_OFFSET 0x24
#define PCI_CONFIG_SUBSYSTEM_VENDOR_ID_OFFSET 0x2C
#define PCI_CONFIG_SUBSYSTEM_ID_OFFSET 0x2E

typedef struct pci_device_t pci_device;

// pci device
struct pci_device_t {
   uint8_t bus;
   uint8_t slot;
   uint16_t vendor_id;
   uint16_t device_id;
   uint16_t class;
   uint16_t subclass;
   uint16_t subsystem_vendor_id;
   uint16_t subsystem_id;
   uint16_t progIF;
   uint32_t bar0;
   uint32_t bar1;
   uint32_t bar2;
   uint32_t bar3;
   uint32_t bar4;
   uint32_t bar5;
};


void pci_init();
void find_pci_device(pci_device *device, uint16_t class, uint16_t subclass      , uint16_t function);
void print_pci_device_info(pci_device *device);

#endif
