/* Serial driver for SpecOS (re-written for 64 bit).
 * Copyright (C) 2024 Jake Steport_byte_inurger under the MIT license. See the GitHub repo for more information.
 */

#include <stdint.h>
#include <stdbool.h>
#include <drivers/serial.h>
#include <drivers/ports.h>
#include <stdlib/string.h>
#include <kernel/printk.h>

#define PORT 0x3f8          // COM1

int init_serial() {
   printk("[argaldOS:kernel:DRV:SER] Starting serial console port 0x3F8\n");
   port_byte_out(PORT + 1, 0x00);    // Disable all interrupts
   port_byte_out(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   port_byte_out(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   port_byte_out(PORT + 1, 0x00);    //                  (hi byte)
   port_byte_out(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   port_byte_out(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   port_byte_out(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
   port_byte_out(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
   port_byte_out(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

   // Check if serial is faulty (i.e: not same byte as sent)
   if(port_byte_in(PORT + 0) != 0xAE) {
      return 1;
   }

   // If serial is not faulty set it in normal operation mode
   // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
   port_byte_out(PORT + 4, 0x0F);
   return 0;
}

int is_transmit_empty() {
   return port_byte_in(PORT + 5) & 0x20;
}

void outCharSerial(char ch) {
    while (!is_transmit_empty());
    port_byte_out(0x3f8, ch);
}

void writeserial(char* str) {
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == 0x00) break;
        if (str[i] == 0x0A){
          outCharSerial(0x0D);
        }
        outCharSerial(str[i]);
    }
}
