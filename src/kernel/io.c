#include <kernel/io.h>
#include <stdint.h>

void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
   volatile uint32_t ret;
   __asm__ volatile("inl %1, %0" : "=a"(ret): "Nd"(port));
   return ret;
}

void io_write8(uint16_t address, uint8_t value) {
   outl(address,value);
}

void io_write16(uint16_t address, uint16_t value) {
   outl(address,value);
}

void io_write32(uint16_t address, uint32_t value) {
   outl(address,value);
}

uint8_t io_read8(uint16_t address) {
   return (inl(address));
}

uint16_t io_read16(uint16_t address) {
   return (inl(address));
}

uint32_t io_read32(uint16_t address) {
   return (inl(address));
}

