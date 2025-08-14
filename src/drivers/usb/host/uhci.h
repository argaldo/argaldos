#include <stdint.h>

#ifndef UHCI_H
#define UHCI_H

#define UHCI_DEVICE_CLASS    0x0C
#define UHCI_DEVICE_SUBCLASS 0x03
#define UHCI_DEVICE_FUNCTION 0x00

#define UHCI_IO_USBCMD_OFFSET    0x00
#define UHCI_IO_USBSTS_OFFSET    0x02
#define UHCI_IO_USBINTR_OFFSET   0x04
#define UHCI_IO_FRNUM_OFFSET     0x06
#define UHCI_IO_FRBASEADD_OFFSET 0x08
#define UHCI_IO_SOFMOD_OFFSET    0x0C
#define UHCI_IO_PORTSC1_OFFSET   0x10
#define UHCI_IO_PORTSC2_OFFSET   0x12

#define TOKEN_IN 0x69
#define TOKEN_OUT 0xE1
#define TOKEN_SETUP 0x2D

#define QUEUE_HEAD_Q 0x00000002

typedef struct uhci_device_io_t uhci_device_io;

struct uhci_device_io_t {
   uint16_t USBCMD;    // USB command
   uint16_t USBSTS;    // USB status
   uint16_t USBINTR;   // USB interrupt enable
   uint16_t FRNUM;     // Frame number
   uint32_t FRBASEADD; // Frame list base address
   uint8_t  SOFMOD;    // Start of frame modify
   uint16_t PORTSC1;   // Port 1 status/control
   uint16_t PORTSC2;   // Port 2 status/control
};

struct UHCI_TRANSFER_DESCRIPTOR {
  uint32_t   link_ptr;
  uint32_t   reply;
  uint32_t   info;
  uint32_t   buff_ptr;
  uint32_t   resv0[4];          // the last 4 dwords are reserved for software use.
};

struct UHCI_QUEUE_HEAD {
  uint32_t   horz_ptr;
  uint32_t   vert_ptr;
  uint32_t   resv0[2];   // to make it 16 bytes in length
};

struct DEVICE_DESC {
  uint8_t  len;
  uint8_t  type;
  uint16_t usb_ver;
  uint8_t  _class;
  uint8_t  subclass;
  uint8_t  protocol;
  uint8_t  max_packet_size;
  uint16_t vendorid;
  uint16_t productid;
  uint16_t device_rel;
  uint8_t  manuf_indx;   // index value
  uint8_t  prod_indx;    // index value
  uint8_t  serial_indx;  // index value
  uint8_t  configs;      // Number of configurations
};

void uhci_init();
void uhci_reset();
void uhci_print_registers(uhci_device_io *usb_device, uint32_t uhci_io_base_address);

#endif
