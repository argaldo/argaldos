/*
 *   UHCI documentation https://ftp.netbsd.org/pub/NetBSD/misc/blymn/uhci11d.pdf
 */

#include <drivers/usb/host/uhci.h>
#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/io.h>
#include <kernel/pmm.h>
#include <kernel/timer.h>
#include <stdlib/string.h>

void get_uhci_device_io_registers(uhci_device_io *usb_device, uint32_t uhci_io_base_address) {
      usb_device->USBCMD = io_read16(uhci_io_base_address + UHCI_IO_USBCMD_OFFSET);
      usb_device->USBSTS = io_read16(uhci_io_base_address + UHCI_IO_USBSTS_OFFSET);
      usb_device->USBINTR = io_read16(uhci_io_base_address + UHCI_IO_USBINTR_OFFSET);
      usb_device->FRNUM = io_read16(uhci_io_base_address + UHCI_IO_FRNUM_OFFSET);
      usb_device->FRBASEADD = io_read32(uhci_io_base_address + UHCI_IO_FRBASEADD_OFFSET);
      usb_device->SOFMOD = io_read8(uhci_io_base_address + UHCI_IO_SOFMOD_OFFSET);
      usb_device->PORTSC1 = io_read16(uhci_io_base_address + UHCI_IO_PORTSC1_OFFSET);
      usb_device->PORTSC2 = io_read16(uhci_io_base_address + UHCI_IO_PORTSC2_OFFSET);
}

void uhci_print_registers(uhci_device_io *usb_device, uint32_t uhci_io_base_address) {
        get_uhci_device_io_registers(usb_device, uhci_io_base_address);
        printk("USBCMD 0x%04X\n",usb_device->USBCMD);
        printk("USBSTS 0x%04X\n",usb_device->USBSTS);
        printk("USBINTR 0x%04X\n",usb_device->USBINTR);
        printk("FRNUM 0x%04X\n",usb_device->FRNUM);
        printk("FRBASEADD 0x%08X\n",usb_device->FRBASEADD);
        printk("SOFMOD 0x%02X\n",usb_device->SOFMOD);
        printk("PORTSC1 0x%04X\n",usb_device->PORTSC1);
        printk("PORTSC2 0x%04X\n",usb_device->PORTSC2);
}

void wait() {
       //uint64_t before = kernel.tick;
       while(1){
          if (kernel.tick % 18  == 0) {
              break;
          }
       }
}

void uhci_reset() {
        printk("[argaldOS:kernel:DRV:USB] Waiting for host controller to reset\n");
        for(int i=0;i<5;i++) {
            io_write16(kernel.uhci_io_base_address + UHCI_IO_USBCMD_OFFSET, 0x0004); // - GRESET Host Controller Reset
            wait();
            io_write16(kernel.uhci_io_base_address + UHCI_IO_USBCMD_OFFSET, 0x0000); // - GRESET Host Controller Reset
        }
}


// See if there is a valid UHCI port at address base+port
// Bit 7 set = Always set.
// See if we can clear it.
bool uhci_port_present(uint16_t base, uint8_t port) {
  // if bit 7 is 0, not a port
  if ((io_read16(base+port) & 0x0080) == 0) return false;
  // try to clear it
  io_write16(base+port, io_read16(base+port) & ~0x0080);
  //if (((base+port) & 0x0080) == 0) return false;   // FIXME disabling this check as it doesn't stand Qemu's USB emulation
  // try to write/clear it
  io_write16(base+port, io_read16(base+port) | 0x0080);
  if ((io_read16(base+port) & 0x0080) == 0) return false;
  // let's see if we write a 1 to bits 3:1, if they come back as zero
  io_write16(base+port, io_read16(base+port) | 0x000A);
  if ((io_read16(base+port) & 0x000A) != 0) return false;
  // we should be able to assume this is a valid port if we get here
  return true;
}

bool uhci_port_reset(uint16_t base, uint8_t port) {
  uint16_t val;

  // reset the port, holding the bit set at least 50 ms for a root hub
  val = io_read16(base + port);
  io_write16(base + port, val | (1<<9));
  mdelay(50);    // USB_TDRSTR
  // clear the reset bit, do not clear the CSC bit while
  //  we clear the reset.  The controller needs to have
  //  it cleared (written to) while *not* in reset
  // also, write a zero to the enable bit
  val = io_read16(base + port);
  io_write16(base + port, val & 0xFCB1);
  udelay(300);  // note that this is *not* the USB specification delay
  // if we wait the recommended USB_TRSTRCY time after clearing the reset,
  //  the device will not enable when we set the enable bit below.
  // the CSC bit must be clear *before* we set the Enable bit
  //  so clear the CSC bit (Write Clear), then set the Enable bit
  val = io_read16(base + port);
  io_write16(base + port, val | 0x0003);
  io_write16(base + port, val | 0x0005);
  // wait for it to be enabled
  udelay(50);
  // now clear the PEDC bit, and CSC if it still
  //  happens to be set, while making sure to keep the
  //  Enable bit and CCS bits set
  val = io_read16(base + port);
  io_write16(base + port, val | 0x000F);
  // short delay before we start sending packets
  mdelay(50);
  val = io_read16(base + port);
  return (val & 0x0004) > 0;
}


bool uhci_set_address(const uint16_t io_base, const uint32_t base, const int selector, const int dev_address, const bool ls_device) {

  // our setup packet (with the third byte replaced below)
  static uint8_t setup_packet[8] = { 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  int i, timeout;

  // one queue and two TD's
  struct UHCI_QUEUE_HEAD queue;
  struct UHCI_TRANSFER_DESCRIPTOR td[2];

  setup_packet[2] = (uint8_t) dev_address;

  queue.horz_ptr = 0x00000001;
  queue.vert_ptr = (base + 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD));  // 128 to skip past buffers

  td[0].link_ptr = ((queue.vert_ptr & ~0xF) + sizeof(struct UHCI_TRANSFER_DESCRIPTOR));
  td[0].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
  td[0].info = (7<<21) | (0<<8) | TOKEN_SETUP;
  td[0].buff_ptr = base + 4096;

  td[1].link_ptr = 0x00000001;
  td[1].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (1<<24) | (0x80 << 16);
  td[1].info = (0x7FF<<21) | (1<<19) | (0<<8) | TOKEN_IN;
  td[1].buff_ptr = 0x00000000;

  // make sure status:int bit is clear
  io_write16(kernel.uhci_io_base_address + UHCI_IO_USBSTS_OFFSET, 1);

  // now move our queue into the physicall memory allocated
  //movedata(_my_ds(), (uint32_t) setup_packet, selector, 4096, 8);
  //movedata(_my_ds(), (uint32_t) &queue, selector, 4096 + 128, sizeof(struct UHCI_QUEUE_HEAD));
  //movedata(_my_ds(), (uint32_t) &td[0], selector, 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD), (sizeof(struct UHCI_TRANSFER_DESCRIPTOR) * 2));

  // mark the first stack frame pointer
  //_farpokel(selector, 0, (base + 4096 + 128) | QUEUE_HEAD_Q);

  // wait for the IOC to happen
  timeout = 10000; // 10 seconds
  while (!(io_read16(kernel.uhci_io_base_address + UHCI_IO_USBSTS_OFFSET) & 1) && (timeout > 0)) {
    timeout--;
    mdelay(1);
  }
  if (timeout == 0) {
    printk(" uhci_set_address:UHCI timed out...\n");
    //_farpokel(selector, 0, 1);  // mark the first stack frame pointer invalid
    return false;
  }
  io_write32(kernel.uhci_io_base_address + UHCI_IO_USBSTS_OFFSET, 1);  // acknowledge the interrupt

  //_farpokel(selector, 0, 1);  // mark the first stack frame pointer invalid

  // copy the stack frame back to our local buffer
  //movedata(selector, 4096 + 128, _my_ds(), (uint32_t) &queue, sizeof(struct UHCI_QUEUE_HEAD));
  //movedata(selector, 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD), _my_ds(), (uint32_t) &td[0], (sizeof(struct UHCI_TRANSFER_DESCRIPTOR) * 2));

  // check the TD's for error
  for (i=0; i<2; i++) {
    if ((td[i].reply & (0xFF<<16)) != 0) {
      printk(" Found Error in TD #%i: 0x%08X\n", i, td[i].reply);
      return false;
    }
  }

  return true;
}

// set up a queue, and enough TD's to get 'size' bytes
bool uhci_get_descriptor(const uint16_t io_base, const uint32_t base, const int selector, struct DEVICE_DESC *dev_desc, const bool ls_device, const int dev_address, const int packet_size, const int size) {
  static uint8_t setup_packet[8] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
  uint8_t our_buff[120];
  int i = 1, t, sz = size, timeout;

  struct UHCI_QUEUE_HEAD queue;
  struct UHCI_TRANSFER_DESCRIPTOR td[10];

  // set the size of the packet to return
  * ((uint16_t *) &setup_packet[6]) = (uint16_t) size;

  //memset(our_buff, 0, 120);

  queue.horz_ptr = 0x00000001;
  queue.vert_ptr = (base + 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD));  // 128 to skip past buffers

  td[0].link_ptr = ((queue.vert_ptr & ~0xF) + sizeof(struct UHCI_TRANSFER_DESCRIPTOR));
  td[0].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
  td[0].info = (7<<21) | ((dev_address & 0x7F)<<8) | TOKEN_SETUP;
  td[0].buff_ptr = base + 4096;

  while ((sz > 0) && (i<9)) {
    td[i].link_ptr = ((td[i-1].link_ptr & ~0xF) + sizeof(struct UHCI_TRANSFER_DESCRIPTOR));
    td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
    t = ((sz <= packet_size) ? sz : packet_size);
    td[i].info = ((t-1)<<21) | ((i & 1) ? (1<<19) : 0) | ((dev_address & 0x7F)<<8) | TOKEN_IN;
    td[i].buff_ptr = base + 4096 + (8 * i);
    sz -= t;
    i++;
  }

  td[i].link_ptr = 0x00000001;
  td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (1<<24) | (0x80 << 16);
  td[i].info = (0x7FF<<21) | (1<<19) | ((dev_address & 0x7F)<<8) | TOKEN_OUT;
  td[i].buff_ptr = 0x00000000;
  i++; // for a total count

  // make sure status:int bit is clear
  io_write16(io_base + UHCI_IO_USBSTS_OFFSET, 1);

  // now move our queue into the physical memory allocated
  //movedata(_my_ds(), (uint32_t) setup_packet, selector, 4096 + 0, 8);
  //movedata(_my_ds(), (uint32_t) our_buff, selector, 4096 + 8, 120);
  //movedata(_my_ds(), (uint32_t) &queue, selector, 4096 + 128, sizeof(struct UHCI_QUEUE_HEAD));
  //movedata(_my_ds(), (uint32_t) &td[0], selector, 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD), sizeof(struct UHCI_TRANSFER_DESCRIPTOR) * 10);

  // mark the first stack frame pointer
  //_farpokel(selector, 0, (base + 4096 + 128) | QUEUE_HEAD_Q);

  // wait for the IOC to happen
  timeout = 10000; // 10 seconds
  while (!(io_read16(io_base + UHCI_IO_USBSTS_OFFSET) & 1) && (timeout > 0)) {
    timeout--;
    mdelay(1);
  }
  if (timeout == 0) {
    printk(" uhci_get_descriptor:UHCI timed out...\n");
    //_farpokel(selector, 0, 1);  // mark the first stack frame pointer invalid
    return false;
  }
  io_write16(io_base + UHCI_IO_USBSTS_OFFSET, 1);  // acknowledge the interrupt

  //_farpokel(selector, 0, 1);  // mark the first stack frame pointer invalid

  // copy the stack frame back to our local buffer
  //movedata(selector, 4096+8, _my_ds(), (uint32_t) our_buff, 120);
  //movedata(selector, 4096 + 128, _my_ds(), (uint32_t) &queue, sizeof(struct UHCI_QUEUE_HEAD));
  //movedata(selector, 4096 + 128 + sizeof(struct UHCI_QUEUE_HEAD), _my_ds(), (uint32_t) &td[0], (sizeof(struct UHCI_TRANSFER_DESCRIPTOR) * 10));

  // check the TD's for error
  for (t=0; t<i; t++) {
    if (((td[t].reply & (0xFF<<16)) != 0)) {
      printk(" Found Error in TD #%i: 0x%08X\n", t, td[t].reply);
      return false;
    }
  }

  // copy the descriptor to the passed memory block
//memcpy(dev_desc, our_buff, size);

  return true;
}

void uhci_init() {
        printk("[argaldOS:kernel:DRV:USB] UCHI USB Initialization\n");
        pci_device _pci_device = {0};
        struct DEVICE_DESC dev_desc;
        int dev_address = 1;
        find_pci_device(&_pci_device, UHCI_DEVICE_CLASS, UHCI_DEVICE_SUBCLASS, UHCI_DEVICE_FUNCTION);
        if (_pci_device.vendor_id != 0x00) {  // device was found
               printk("[argaldOS:kernel:DRV:USB] USB Host Controller ( UHCI ) found\n");
               kernel.uhci_io_base_address = _pci_device.bar4;
               get_uhci_device_io_registers(&kernel.usb_device, kernel.uhci_io_base_address);
               //uhci_print_registers(&kernel.usb_device,kernel.uhci_io_base_address);
               uhci_reset();
               get_uhci_device_io_registers(&kernel.usb_device, kernel.uhci_io_base_address);
               uhci_print_registers(&kernel.usb_device,kernel.uhci_io_base_address);
               if (kernel.usb_device.USBCMD != 0x0000 || kernel.usb_device.USBSTS != 0x0000 || kernel.usb_device.SOFMOD != 0x40) {
                  printk("[argaldOS:kernel:DRV:USB] ERROR: The USB host controller didn't reset properly :(\n");
                  return;
               }
               io_write16(kernel.uhci_io_base_address + UHCI_IO_USBSTS_OFFSET, 0x00FF); // clear USBSTS
               io_write16(kernel.uhci_io_base_address + UHCI_IO_USBCMD_OFFSET, 0x0002); // HCRESET Host Controller reset
               wait();
               // if it gets here, means that UHCI controller is working OK
               printk("[argaldOS:kernel:DRV:USB] USB Host Controller reset and in working condition\n");
               
               uint8_t* ptr = (uint8_t*) kmalloc();  // reserving a 8K memory page
               uint64_t address = (uint64_t)( ptr + kernel.hhdm );
               char buf[20];
               uint64_to_hex_string(address,buf);
               printk("Mem 0x%s\n",buf);

               printk("[argaldOS:kernel:DRV:USB] USB Host Controller configuration\n");
               // Setting the host controller schedule configuration
               io_write32(kernel.uhci_io_base_address + UHCI_IO_FRBASEADD_OFFSET, address); // physical address
               io_write16(kernel.uhci_io_base_address + UHCI_IO_FRNUM_OFFSET, 0x00);        // start af frame 0
               io_write8(kernel.uhci_io_base_address + UHCI_IO_SOFMOD_OFFSET, 0x40);        // start of frame to default
               io_write16(kernel.uhci_io_base_address + UHCI_IO_USBINTR_OFFSET, 0x0000);    // disallow error interrupts
               io_write16(kernel.uhci_io_base_address + UHCI_IO_USBSTS_OFFSET, 0x40);       // clear status bits
               io_write16(kernel.uhci_io_base_address + UHCI_IO_USBCMD_OFFSET, (1<<7) | (1<<6) | (1<<0));  // start of frame to default
               
               // Now attempting to read device info on first USB port at offset 0x10
               if (uhci_port_present(kernel.uhci_io_base_address,0x10)) {
                     printk("An USB device is present in port 0x10\n");
                     uhci_port_reset(kernel.uhci_io_base_address,0x10);
                     printk("After resetting the port\n");
                     if (uhci_set_address(kernel.uhci_io_base_address,address,1,dev_address,true)) {
                        printk("Set address\n");
                     }
                     if (uhci_get_descriptor(kernel.uhci_io_base_address,address,1,&dev_desc,true,dev_address, dev_desc.max_packet_size, dev_desc.len)) {
                        printk("get_descriptor\n");
                     }

                        

               } else {
                     printk("An USB device is NOT present in port 0x10\n");
               }

               printk("[argaldOS:kernel:DRV:USB] Done\n");

        } else {
                printk("[argaldOS:kernel:DRV:USB] UHCI USB controller not found\n");
        }
}
