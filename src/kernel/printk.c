#include <stdlib/printf.h>
#include <kernel/kernel.h>
#include <drivers/serial.h>
#include <kernel/util/hexdump.h>

int printk(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int ret = vprintf(format,args);
        va_end(args);
        if (kernel.serial_output) {
            char buffer[1024];
            snprintf(buffer,sizeof(buffer),format,args);
            writeserial(buffer);
        } 
        return ret;
}

int kdebug(const char* format, ...) {
    if (kernel.debug) {
        va_list args;
        va_start(args, format);
        int ret = vprintf(format,args);
        va_end(args);
        if (kernel.serial_output) {
            char buffer[1024];
            snprintf(buffer,sizeof(buffer),format,args);
            writeserial(buffer);
        }
        return ret;
   } else {
        return 0;
   }
}
