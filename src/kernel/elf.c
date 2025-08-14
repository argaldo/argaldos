
#include <kernel/elf.h>
#include <stdbool.h>
#include <stdint.h>
#include <kernel/printk.h>
#include <kernel/kernel.h>
#include <stdlib/binop.h>
#include <kernel/pmm.h>
#include <stdlib/string.h>
#include <kernel/mem.h>
#include <stddef.h>


bool is_valid_elf(const uint8_t *magic) {
    // Use memcmp for binary comparison, not strcmp
    if (memcmp(magic, "\x7F" "ELF", 4) != 0) {
        kdebug("Wrong ELF magic number: Invalid ELF file.\n");
        return false;
    }
    return true;
}


int parse_elf_header(const uint8_t* elf, struct ELF_FILE_HEADER_T* elf_header) {
    if (!elf || !elf_header) return 0;
    kdebug("Parsing ELF\n");
    for (int i = 0; i < 4; i++) elf_header->magic[i] = elf[i];
    elf_header->magic[4] = 0x00;
    if (!is_valid_elf(elf_header->magic)) return 0;
    elf_header->elf_format = elf[0x04];
    elf_header->endianness = elf[0x05];
    elf_header->version = elf[0x06];
    elf_header->abi = elf[0x07];
    elf_header->abi_version = elf[0x08];
    // TODO: parse padding bytes if needed
    elf_header->type = (uint16_t)(elf[0x11] << 8 | elf[0x10]);
    elf_header->machine_type = (uint16_t)(elf[0x13] << 8 | elf[0x12]);
    // Do not overwrite version field
    // elf_header->version = elf[0x14];
    elf_header->entry_point_address = combine64bit(elf[0x1F], elf[0x1E], elf[0x1D], elf[0x1C], elf[0x1B], elf[0x1A], elf[0x19], elf[0x18]);
    elf_header->section_header_offset = combine64bit(elf[0x2F], elf[0x2E], elf[0x2D], elf[0x2C], elf[0x2B], elf[0x2A], elf[0x29], elf[0x28]);
    // TODO: parse rest of fields in header
    elf_header->section_header_entry_size = (uint16_t)(elf[0x3B] << 8 | elf[0x3A]);
    elf_header->section_header_entry_count = (uint16_t)(elf[0x3D] << 8 | elf[0x3C]);
    // TODO: parse rest of fields in header
    return 1;
}


void print_elf_header(struct ELF_FILE_HEADER_T elf_header) {
    kdebug("magic: %s\n", elf_header.magic);
    kdebug("elf_format: [%s] 0x%02X\n", (elf_header.elf_format == 0x01) ? "32 bit" : "64 bit", elf_header.elf_format);
    kdebug("endianness: [%s] 0x%02X\n", (elf_header.endianness == 0x01) ? "little endian" : "big endian", elf_header.endianness);
    kdebug("ELF version: %d\n", elf_header.version);
    const char* abi_str = "Unknown";
    switch (elf_header.abi) {
        case 0x00: abi_str = "System V"; break;
        // TODO: Add more ABI types
    }
    kdebug("ABI: [%s] 0x%02X\n", abi_str, elf_header.abi);
    kdebug("ABI version: %d\n", elf_header.abi_version);
    const char* type_str = "Unknown";
    switch (elf_header.type) {
        case 0x00: type_str = "Unknown"; break;
        case 0x01: type_str = "Relocatable file"; break;
        case 0x02: type_str = "Executable file"; break;
        case 0x03: type_str = "Shared object"; break;
        case 0x04: type_str = "Core file"; break;
        case 0xFE00: type_str = "Reserved. LOOS. Operating system specific"; break;
        case 0xFEFF: type_str = "Reserved. HIOS. Operating system specific"; break;
        case 0xFF00: type_str = "Reserved. LOPROC. Processor specific"; break;
        case 0xFFFF: type_str = "Reserved. HIPROC. Processor specific"; break;
    }
    kdebug("type: [%s] 0x%02X\n", type_str, elf_header.type);
    const char* machine_type_str = "Unknown";
    switch (elf_header.machine_type) {
        case 0x3E: machine_type_str = "AMD x86-64"; break;
        // TODO: Add more machine types
    }
    kdebug("machine type: [%s] 0x%02X\n", machine_type_str, elf_header.machine_type);
    kdebug("version: %d\n", elf_header.version);
    kdebug("entry point address: 0x%zx\n", (size_t)elf_header.entry_point_address);
    kdebug("section_header_offset: 0x%zx\n", (size_t)elf_header.section_header_offset);
    kdebug("section_header_entry_size: %hu\n", elf_header.section_header_entry_size);
    kdebug("section_header_entry_count: %hu\n", elf_header.section_header_entry_count);
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
    // Parse virtual address (sh_addr)
    section_header->virtual_address = combine64bit(elf[base_offset+0x17],elf[base_offset+0x16],elf[base_offset+0x15],elf[base_offset+0x14],elf[base_offset+0x13],elf[base_offset+0x12],elf[base_offset+0x11],elf[base_offset+0x10]);
    section_header->offset = combine64bit(elf[base_offset+0x1F],elf[base_offset+0x1E],elf[base_offset+0x1D],elf[base_offset+0x1C],elf[base_offset+0x1B],elf[base_offset+0x1A],elf[base_offset+0x19],elf[base_offset+0x18]);
    section_header->size = combine64bit(elf[base_offset+0x27],elf[base_offset+0x26],elf[base_offset+0x25],elf[base_offset+0x24],elf[base_offset+0x23],elf[base_offset+0x22],elf[base_offset+0x21],elf[base_offset+0x20]);
    return 0;
}




int read_elf(const uint8_t* elf, bool run) {
    struct ELF_FILE_HEADER_T elf_header;
    if (!parse_elf_header(elf, &elf_header)) {
        kdebug("Failed to parse ELF header\n");
        return -1;
    }
    print_elf_header(elf_header);

    // Find min/max virtual address for SHT_PROGBITS
    uint64_t min_addr = (uint64_t)-1, max_addr = 0;
    for (int i = 0; i < elf_header.section_header_entry_count; i++) {
        struct ELF_SECTION_HEADER_T sh;
        if (parse_section_header((uint8_t*)elf, &sh, elf_header.section_header_offset, i, elf_header.section_header_entry_size) != 0) continue;
        if (sh.type == 0x01 && sh.size > 0) {
            if (sh.virtual_address < min_addr) min_addr = sh.virtual_address;
            if (sh.virtual_address + sh.size > max_addr) max_addr = sh.virtual_address + sh.size;
        }
    }
    if (min_addr == (uint64_t)-1 || max_addr <= min_addr) {
        kdebug("No valid SHT_PROGBITS sections found.\n");
        return -1;
    }
    uint64_t image_size = max_addr - min_addr;
    uint8_t* image = (uint8_t*)kmalloc(image_size);
    if (!image) {
        kdebug("Failed to allocate memory for ELF image.\n");
        return -1;
    }
    memset(image, 0, image_size);
    // Copy all SHT_PROGBITS sections to their virtual address offset in image
    for (int i = 0; i < elf_header.section_header_entry_count; i++) {
        struct ELF_SECTION_HEADER_T sh;
        if (parse_section_header((uint8_t*)elf, &sh, elf_header.section_header_offset, i, elf_header.section_header_entry_size) != 0) continue;
        if (sh.type == 0x01 && sh.size > 0) {
            if (sh.offset + sh.size > 4608) {
                kdebug("Section offset+size out of buffer bounds, skipping\n");
                continue;
            }
            uint64_t off = sh.virtual_address - min_addr;
            for (size_t j = 0; j < sh.size; j++) {
                image[off + j] = elf[sh.offset + j];
            }
        }
    }
    // Check entry point is within image
    if (elf_header.entry_point_address < min_addr || elf_header.entry_point_address >= max_addr) {
        kdebug("Entry point not within loaded image.\n");
        kfree(image);
        return -1;
    }
    if (run) {
        uint64_t entry_offset = elf_header.entry_point_address - min_addr;
        int (*elf_entry_point)(void) = (int(*)(void))(image + kernel.hhdm + entry_offset);
        printk("Running ELF entry point at offset 0x%zx\n", (size_t)entry_offset);
        elf_entry_point();
    }
    kfree(image);
    return 0;
}
