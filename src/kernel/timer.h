#include <stdbool.h>
#include <stdint.h>


bool setup_timer();

void ndelay(const uint32_t n);
void udelay(const uint32_t u);
void mdelay(const uint32_t m);
void delay(const uint32_t s);
