#include <kernel/elf.h>
//#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <stdlib/binop.h>
#include <kernel/pmm.h>
#include <stdlib/string.h>
#include <kernel/mem.h>

bool is_valid_elf(char magic[4]) {
        if (strcmp("\x7F" "ELF", magic)){
                kdebug("Wrong ELF magic number: Invalid ELF file.\n");
                return false;
        }
        return true;
}

int parse_elf_header(uint8_t* elf, struct ELF_FILE_HEADER_T* elf_header) {
        kdebug("Parsing ELF\n");
        for(int i=0;i<4;i++) { elf_header->magic[i] = elf[i]; } elf_header->magic[4] = 0x00;
        elf_header->elf_format = elf[0x04];
        elf_header->endianness = elf[0x05];
        elf_header->version = elf[0x06];
        elf_header->abi = elf[0x07];
        elf_header->abi_version = elf[0x08];
        //TODO padding bytes
        elf_header->type = (uint16_t)(elf[0x11] << 8 | elf[0x10]);
        elf_header->machine_type = (uint16_t)(elf[0x13] << 8 | elf[0x12]);
        elf_header->version = elf[0x14];
        elf_header->entry_point_address = combine64bit(elf[0x1F],elf[0x1E],elf[0x1D],elf[0x1C],elf[0x1B],elf[0x1A],elf[0x19],elf[0x18]);
        elf_header->section_header_offset = combine64bit(elf[0x2F],elf[0x2E],elf[0x2D],elf[0x2C],elf[0x2B],elf[0x2A],elf[0x29],elf[0x28]);
        // TODO rest of fields in header
        elf_header->section_header_entry_size = (uint16_t)(elf[0x3B] << 8 | elf[0x3A]);
        elf_header->section_header_entry_count = (uint16_t)(elf[0x3D] << 8 | elf[0x3C]);
        // TODO rest of fields in header
        return 1;
}

void print_elf_header(struct ELF_FILE_HEADER_T elf_header) {
           kdebug("magic: %s\n",elf_header.magic);
           kdebug("elf_format: [%s] 0x%02X\n",(elf_header.elf_format == 0x01)?"32 bit":"64 bit",elf_header.elf_format);
           kdebug("endianness: [%s] 0x%02X\n",(elf_header.endianness==0x01)?"little endian":"big endian", elf_header.endianness);
           kdebug("ELF version: %d\n",elf_header.version);
           char* abi_str;
           switch(elf_header.abi) {
               case 0x00: abi_str = "System V";break;
               // TODO rest of ABI types
               default: abi_str = "Unknown";
           }

           kdebug("ABI: [%s] 0x%02X\n",abi_str,elf_header.abi);
           kdebug("ABI version: %d\n",elf_header.abi_version);

           char* type_str;
           switch(elf_header.type) {
               case 0x00: type_str = "Unknown";break;
               case 0x01: type_str = "Relocatable file";break;
               case 0x02: type_str = "Executable file";break;
               case 0x03: type_str = "Shared object";break;
               case 0x04: type_str = "Core file";break;
               case 0xFE00: type_str = "Reserved. LOOS. Operating system specific";break;
               case 0xFEFF: type_str = "Reserved. HIOS. Operating system specific";break;
               case 0xFF00: type_str = "Reserved. LOPROC. Processor specific";break;
               case 0xFFFF: type_str = "Reserved. HIPROC. Processor specific";break;
               default: type_str = "Unknown";
           }
           kdebug("type: [%s] 0x%02X\n", type_str, elf_header.type);
           char* machine_type_str;
           switch(elf_header.machine_type) {
               case 0x3E: machine_type_str = "AMD x86-64";break;
               // TODO rest of machine types
               default: machine_type_str = "Unknown";
           }
           kdebug("machine type: [%s] 0x%02X\n", machine_type_str, elf_header.machine_type);
           kdebug("version: %d\n",elf_header.version);
           kdebug("entry point address: %zu\n",(uint32_t)elf_header.entry_point_address);
           kdebug("section_header_offset: %hu\n",elf_header.section_header_offset);
           kdebug("section_header_entry_size: %hu\n",elf_header.section_header_entry_size);
           kdebug("section_header_entry_count: %hu\n",elf_header.section_header_entry_count);
}

void print_section_header(struct ELF_SECTION_HEADER_T section_header) {
        kdebug("SECTION HEADER\n");
        //kdebug("type: [%s] 0x%04X\n",type_str,section_header.name_offset);
        kdebug("name offset: 0x%04X\n",section_header.name_offset);
        char* type_str = {0};
        switch(section_header.type) {
                case 0x00: type_str = "Unused entry (SHT_NULL)";break;
                case 0x01: type_str = "Program data (SHT_PROGBITS)";break;
                case 0x02: type_str = "Symbol table (SHT_SYNTAB)";break;
                case 0x03: type_str = "String table (SHT_STRTAB)";break;
                case 0x04: type_str = "Relocation entries with addends (SHT_RELA)";break;
                case 0x05: type_str = "Symbol hash table (SHT_HASH)";break;
                case 0x06: type_str = "Unused entry";break;
                           // TODO fill rest of header types
                case 0x07: type_str = "Unused entry";break;
                case 0x08: type_str = "Unused entry";break;
                case 0x09: type_str = "Unused entry";break;
                case 0x0A: type_str = "Unused entry";break;
                case 0x0B: type_str = "Unused entry";break;
                case 0x0E: type_str = "Unused entry";break;
                case 0x0F: type_str = "Unused entry";break;
                case 0x10: type_str = "Unused entry";break;
                case 0x11: type_str = "Unused entry";break;
                case 0x12: type_str = "Unused entry";break;
                case 0x13: type_str = "Unused entry";break;
                case 0x60000000: type_str = "Unused entry";break;
                
        } 
        kdebug("type: [%s] 0x%02X\n",type_str,section_header.type);
        kdebug("offset: 0x%08X\n",section_header.offset);
        kdebug("size: 0x%08X\n",section_header.size);
}

int parse_section_header(uint8_t* elf, struct ELF_SECTION_HEADER_T* section_header, uint64_t offset, int index, uint16_t header_size) {
        uint64_t base_offset = offset + (index * header_size);
        section_header->name_offset = combine32bit(elf[base_offset+0x03],elf[base_offset+0x02],elf[base_offset+0x01],elf[base_offset + 0x00]);
        section_header->type = combine32bit(elf[base_offset+0x07],elf[base_offset+0x06],elf[base_offset+0x05],elf[base_offset + 0x04]);
        // TODO rest of sectino header fields
        section_header->offset = combine64bit(elf[base_offset+0x1F],elf[base_offset+0x1E],elf[base_offset+0x1D],elf[base_offset+0x1C],elf[base_offset+0x1B],elf[base_offset+0x1A],elf[base_offset+0x19],elf[base_offset+0x18]);
        section_header->size = combine64bit(elf[base_offset+0x27],elf[base_offset+0x26],elf[base_offset+0x25],elf[base_offset+0x24],elf[base_offset+0x23],elf[base_offset+0x22],elf[base_offset+0x21],elf[base_offset+0x20]);
        return 0;
}


int read_elf(uint8_t* elf, bool run) {
        struct ELF_FILE_HEADER_T elf_header;
        if (parse_elf_header(elf,&elf_header)) {
            print_elf_header(elf_header);
            for(int i=0;i<elf_header.section_header_entry_count;i++) {
               struct ELF_SECTION_HEADER_T section_header;
               parse_section_header(elf,&section_header,elf_header.section_header_offset,i,elf_header.section_header_entry_size);
               print_section_header(section_header);
               if (section_header.type == 0x01) {
                     kdebug("Found the code in the .text section of the ELF file\n");
                       // this is the actual code to be run
                     uint8_t code[section_header.size];
                     for(long unsigned int j=0;j<section_header.size;j++) {
                             code[j] = elf[section_header.offset + j];
                     }
                     if (run) {
                        printk("Allocating memory to store the code\n");
                        uint8_t* ptr = (uint8_t*) kmalloc();   // alocate memory for code
                        printk("Copying the code to kernel memory\n");
                        memcpy(ptr + kernel.hhdm,code,3);      // copy code
                        int (*elf_entry_point)(void) = (int(*)(void))(ptr+kernel.hhdm); // pointer to an anonymous function to execute the code
                        printk("Running the code\n");
                        elf_entry_point();  // running the code from ELF
                       // break;
                     }
               }
            }
        } 
        return 0;
}
