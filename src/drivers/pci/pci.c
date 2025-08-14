#include <drivers/terminal/terminal.h>
#include <drivers/pci/pci.h>
#include <kernel/io.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>

/*
 * PCI device classes: https://pcisig.com/sites/default/files/files/PCI_Code-ID_r_1_11__v24_Jan_2019.pdf
 */
char* get_pci_device_class(uint16_t class, uint16_t subclass, uint16_t progIF) {
        switch(class) {
                case 0x0001:
                        switch(subclass) {
                                case 0x0001:
                                        return "IDE controller";
                                case 0x0006:
                                        switch(progIF) {
                                                case 0x0000:
                                                        return "Serial ATA controller";
                                                case 0x0001:
                                                        return "Serial ATA controller - AHCI";
                                                default:
                                                        return "Unknown";
                                        }
                                default:
                                        return "Unknown";
                        }
                case 0x0002:
                        switch(subclass) {
                                case 0x0000:
                                        return "Ethernet controller";
                                default:
                                        return "Unknown";
                        }
                case 0x0003:
                        switch(subclass) {
                                case 0x0000:
                                        return "VGA-compatible controller";
                                case 0x0002:
                                        return "3D Controller";
                                default:
                                        return "Unknown"; 
                        }
                case 0x0006: 
                        switch(subclass) {
                                case 0x0000:
                                        return "Host bridge";
                                case 0x0001:
                                        return "ISA bridge";
                                case 0x0080:
                                        return "Other PCI bridge";
                                default:
                                        return "Unknown";
                        }
                case 0x000c:
                        switch(subclass) {
                                case 0x0003:
                                        switch(progIF) {
                                                case 0x0000:
                                                        return "USB controller - UHCI";
                                                case 0x0020:
                                                        return "USB2 controller - EHCI";
                                                default:
                                                        return "Unknown";
                                        }
                                default:
                                        return "Unknown";
                        }
                default:
                        return "Unknown";
       }
}

uint8_t  pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
	uint32_t address = (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000;
   io_write32(PCI_CONFIG_ADDRESS, address);
   return (inl(PCI_DATA_ADDRESS) >> ((offset & 2) * 8)) & 0xffff;
}

uint16_t  pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
	uint32_t address = (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000;
   io_write32(PCI_CONFIG_ADDRESS, address);
   return (inl(PCI_DATA_ADDRESS) >> ((offset & 2) * 8)) & 0xffff;
}


uint32_t  pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset) {
	uint32_t address = (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000;
   io_write32(PCI_CONFIG_ADDRESS, address);
   return (inl(PCI_DATA_ADDRESS) >> ((offset & 2) * 8)) & 0xffff;
}

void find_pci_device(pci_device *device, uint16_t class, uint16_t subclass, uint16_t function) {
        // enumerating PCI bus trying to find the device
        for(int bus=0;bus<256;bus++) {
                for(int slot=0;slot<32;slot++){
                        for(int _function=0;_function<8;_function++) {
                           uint16_t vendor_id = pci_config_read16(bus, slot, _function, PCI_CONFIG_VENDOR_ID_OFFSET);
                           if (vendor_id != 0xffff) { // there's a device there
                              uint16_t header_type = pci_config_read16(bus,slot,_function, PCI_CONFIG_HEADER_TYPE_OFFSET);
                              device->device_id = pci_config_read16(bus, slot, _function, PCI_CONFIG_DEVICE_ID_OFFSET);
                              uint16_t device_class_subclass = pci_config_read16(bus, slot, _function, PCI_CONFIG_DEVICE_CLASS_OFFSET);
                              uint16_t device_function = pci_config_read16(bus,slot,_function,0x08) >> 8;
                              uint16_t device_class = device_class_subclass >> 8;
                              uint16_t device_subclass = device_class_subclass & 0xff;
                              if (device_class == class && device_subclass == subclass && device_function == function ) {
                                     // device found!
                                 device->bus = bus;
                                 device->slot = slot;
                                 device->vendor_id = vendor_id;
                                 device->class = device_class;
                                 device->subclass = device_subclass;
                                 device->subsystem_vendor_id = pci_config_read16(bus, slot, _function, PCI_CONFIG_SUBSYSTEM_VENDOR_ID_OFFSET);
                                 device->subsystem_id = pci_config_read16(bus, slot, _function, PCI_CONFIG_SUBSYSTEM_ID_OFFSET);
                                 device->progIF = _function;
                                 device->bar0 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR0_OFFSET);
                                 device->bar1 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR1_OFFSET);
                                 device->bar2 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR2_OFFSET);
                                 device->bar3 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR3_OFFSET);
                                 device->bar4 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR4_OFFSET) & 0xFFFFFFC;   // PCI BAR4 for USB represents an IO mapped register, hence less-significative bit is 1
                                 device->bar5 = pci_config_read32(bus,slot,_function,PCI_CONFIG_BAR5_OFFSET);
                                 return;
                              }
                  }
                }
              } 
      }
}

void print_pci_device_info(pci_device *device) {
      printk("[argaldOS:kernel:DRV:PCI] PCI Device %02X:%02X.%02X\n", device->bus, device->slot, device->progIF);
      printk("[argaldOS:kernel:DRV:PCI] Vendor id: 0x%04X\n", device->vendor_id);
      printk("[argaldOS:kernel:DRV:PCI] Device id: 0x%04X\n", device->device_id);
      printk("[argaldOS:kernel:DRV:PCI] Device class: 0x%04X\n", device->class);
      printk("[argaldOS:kernel:DRV:PCI] Device subClass: 0x%04X\n",device->subclass);
      printk("[argaldOS:kernel:DRV:PCI] Device type: %s\n", get_pci_device_class(device->class, device->subclass,device->progIF));
      printk("[argaldOS:kernel:DRV:PCI] Device subSystemVendorId: 0x%04X\n",device->subsystem_vendor_id);
      printk("[argaldOS:kernel:DRV:PCI] Device subSystemId: 0x%04X\n",device->subsystem_id);
      printk("[argaldOS:kernel:DRV:PCI] BAR0: 0x%08X\n",device->bar0);
      printk("[argaldOS:kernel:DRV:PCI] BAR1: 0x%08X\n",device->bar1);
      printk("[argaldOS:kernel:DRV:PCI] BAR2: 0x%08X\n",device->bar2);
      printk("[argaldOS:kernel:DRV:PCI] BAR3: 0x%08X\n",device->bar3);
      printk("[argaldOS:kernel:DRV:PCI] BAR4: 0x%08X\n",device->bar4);
      printk("[argaldOS:kernel:DRV:PCI] BAR5: 0x%08X\n",device->bar5);
      //printk("[argaldOS:kernel:DRV:PCI] %s[%02x:%02x.%02x] %s device {class:subclass %04x:%04x} {vendor:device %04x:%04x} {subsystem: %04x:%04x} progIF: %02x\n", parent ? "":"   ", bus, slot, function, get_pci_device_class(device_class, device_subclass, device_progIF), device_class, device_subclass, vendor_id, device_id, device_subsystem_vendor_id, device_subsystem_id, device_progIF);
}


void get_pci_device_config(uint8_t bus, uint8_t slot, uint8_t function, bool parent) {
   uint16_t vendor_id = pci_config_read16(bus, slot, function, PCI_CONFIG_VENDOR_ID_OFFSET);
   if (vendor_id != 0xffff) {
      uint16_t device_id = pci_config_read16(bus, slot, function, PCI_CONFIG_DEVICE_ID_OFFSET);
      uint16_t device_class_subclass = pci_config_read16(bus, slot, function, PCI_CONFIG_DEVICE_CLASS_OFFSET);
      uint16_t device_progIF = pci_config_read16(bus,slot,function,0x08) >> 8;
      uint16_t device_class = device_class_subclass >> 8;
      uint16_t device_subclass = device_class_subclass & 0xff;
      uint16_t device_subsystem_vendor_id = pci_config_read16(bus, slot, function, PCI_CONFIG_SUBSYSTEM_VENDOR_ID_OFFSET);
      uint16_t device_subsystem_id = pci_config_read16(bus, slot, function, PCI_CONFIG_SUBSYSTEM_ID_OFFSET);
      uint16_t device_header_type = pci_config_read16(bus,slot,function, PCI_CONFIG_HEADER_TYPE_OFFSET);
      uint32_t bar0 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR0_OFFSET);
      uint32_t bar1 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR1_OFFSET);
      uint32_t bar2 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR2_OFFSET);
      uint32_t bar3 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR3_OFFSET);
      uint32_t bar4 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR4_OFFSET);
      uint32_t bar5 = pci_config_read32(bus,slot,function,PCI_CONFIG_BAR5_OFFSET);
      kdebug("\n[argaldOS:kernel:DRV:PCI] Detected PCI Device %02X:%02X%02X\n", bus, slot, function);
      kdebug("[argaldOS:kernel:DRV:PCI] Vendor id: 0x%04X\n", vendor_id);
      kdebug("[argaldOS:kernel:DRV:PCI] Device id: 0x%04X\n", device_id);
      kdebug("[argaldOS:kernel:DRV:PCI] Device class: 0x%04X\n", device_class);
      kdebug("[argaldOS:kernel:DRV:PCI] Device subClass: 0x%04X\n",device_subclass);
      kdebug("[argaldOS:kernel:DRV:PCI] Device type: %s\n", get_pci_device_class(device_class, device_subclass,device_progIF));
      kdebug("[argaldOS:kernel:DRV:PCI] Device subSystemVendorId: 0x%04X\n",device_subsystem_vendor_id);
      kdebug("[argaldOS:kernel:DRV:PCI] Device subSystemId: 0x%04X\n",device_subsystem_id);
      kdebug("[argaldOS:kernel:DRV:PCI] Device header type: 0x%04X\n",device_header_type);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR0: 0x%08X\n",bar0);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR1: 0x%08X\n",bar1);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR2: 0x%08X\n",bar2);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR3: 0x%08X\n",bar3);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR4: 0x%08X\n",bar4);
      kdebug("[argaldOS:kernel:DRV:PCI] BAR5: 0x%08X\n",bar5);
      printk("[argaldOS:kernel:DRV:PCI] %s[%02X:%02X.%02X] %s device {class:subclass %04X:%04X} {vendor:device %04X:%04X} {subsystem: %04X:%04X} progIF: %02X\n", parent ? "":"   ", bus, slot, function, get_pci_device_class(device_class, device_subclass, device_progIF), device_class, device_subclass, vendor_id, device_id, device_subsystem_vendor_id, device_subsystem_id, device_progIF);

      if (device_header_type == 0x0080) {
              // is multifunction device
              for (int i=1;i<8;i++) {
                      get_pci_device_config(bus,slot,i,false);
              }
      }
   }
}

void enumerate_pci_slot(uint8_t bus, uint8_t slot) {
    //printk("[argaldOS:kernel:DRV:PCI] Enumerating PCI bus %d slot %d\n", bus, slot); 
    return get_pci_device_config(bus,slot,0, true);
}

void enumerate_pci_bus(uint8_t bus) {
    //printk("[argaldOS:kernel:DRV:PCI] Enumerating PCI bus %d\n", bus); 
    for (int i=0;i<32;i++) {
            enumerate_pci_slot(bus,i);
    }
}

void pci_enumeration() {
    printk("[argaldOS:kernel:DRV:PCI] Starting PCI enumeration\n"); 
    for (int i=0;i<256;i++){
       enumerate_pci_bus(i);
    }
}

void pci_init() {
    printk("[argaldOS:kernel:DRV:PCI] PCI Initialization\n");
    io_write32(PCI_CONFIG_ADDRESS, 0x80000000);
    if (inl(PCI_CONFIG_ADDRESS) != 0x80000000) {
            printk("[argaldOS:kernel:DRV:PCI] PCI Not Found!\n");
    } else {
            pci_enumeration();
    }
}

