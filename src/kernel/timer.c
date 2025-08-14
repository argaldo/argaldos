#include <stdbool.h>
#include <stdint.h>
#include <kernel/io.h>
#include <kernel/printk.h>


///////////////////////////////////////////////////////////////////////////////////////////////
// This is the timer delay code.
//
uint64_t cpu_hz;   // clock ticks per second

// read the Time Stamp Count
uint64_t rdtsc() {
  uint64_t val; 
  __asm__ __volatile__ ("rdtsc" : "=A" (val)); 
  return val; 
}

// Check to see if the processor has the RDTSC instruction.
// This assumes the processor has the CPUID instruction.
bool setup_timer() {
  
  uint32_t eax, edx;
  uint64_t start, end;
  
  __asm__ __volatile__ (
    "movl  $0x000000001,%%eax\n"
    "cpuid\n"
    "movl  %%edx,%0"
    : "=g" (edx)
    :
    : "eax", "ebx", "ecx", "edx"
  );
  
  if (edx & 0x10) {
    
    // Set up the PPC port - disable the speaker, enable the T2 gate.
    io_write8(0x61, (io_read8(0x61) & ~0x02) | 0x01);
    
    // Set the PIT to Mode 0, counter 2, word access.
    io_write8(0x43, 0xB0);
    
    // Load the counter with 0xFFFF
    io_write8(0x42, 0xFF);
    io_write8(0x42, 0xFF);
    
    // Read the number of ticks during the period.
    start = rdtsc();
    while (!(io_read8(0x61) & 0x20))
      ;
    end = rdtsc();
    
    cpu_hz = (uint64_t) ((end - start) * (uint64_t) 1193180 / (uint64_t) 0xFFFF);
    printk("[argaldOS:kernel:COR:TMR] Detected CPU clock speed %zu Hz\n",cpu_hz);
    return true;
  } else
    return false;
}

// n = amount of rdtsc timer ticks to delay
void _delay(const uint64_t n) {
  volatile const uint64_t timeout = (rdtsc() + n);
  //printk("timeout %zu\n", timeout);
  while (true) {
          uint64_t r = rdtsc();
          //printk("%zu\n",r);
          if (r < timeout) {
                  break;
          }
  }
}

// delay for a specified number of nanoseconds. 
//  n = number of nanoseconds to delay.
void ndelay(const uint32_t n) {
  _delay(((uint64_t) n * cpu_hz) / (uint64_t) 1000000000); 
} 
 
// delay for a specified number of microseconds. 
//  n = number of microseconds to delay. 
void udelay(const uint32_t u) {
  _delay(((uint64_t) u * cpu_hz) / (uint64_t) 1000000); 
}

// delay for a specified number of milliseconds. 
//  m = number of milliseconds to delay. 
void mdelay(const uint32_t m) {
  _delay(((uint64_t) m * cpu_hz) / (uint64_t) 1000);
}

// delay for a specified number of seconds. 
//  s = number of seconds to delay. 
void delay(const uint32_t s) {
  _delay((uint64_t) s * cpu_hz); 
}
