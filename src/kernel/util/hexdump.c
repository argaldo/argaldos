#include <kernel/util/hexdump.h>
#include <kernel/printk.h>

void hexdump(uint8_t* data, int size) {
        for(int i=0;i<size;i++) {
            if ((i+1)%16 != 0) {
               kdebug("%02X ",data[i]);
               if ((i+1)%8 == 0) {
                       kdebug(" ");
               }
            } else {
               kdebug("%02X  |",data[i]);
               for(int j=0;j<16;j++) {
                       char c = (char)data[i-16+j];
                       char to_print = (c>=0x20)?c:0x2E;
                       kdebug("%c",to_print);
               }
               kdebug("|\n");
            }
        }
}


