#include <stdint.h>

void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);

void io_write8(uint16_t address, uint8_t value);
void io_write16(uint16_t address, uint16_t value);
void io_write32(uint16_t address, uint32_t value);
uint8_t io_read8(uint16_t address);
uint16_t io_read16(uint16_t address);
uint32_t io_read32(uint16_t address);

