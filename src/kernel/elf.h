#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef ELF_H
#define ELF_H

struct ELF_FILE_HEADER_T {
   uint8_t magic[5];
   uint8_t elf_format;
   uint8_t endianness;
   uint8_t elf_version;
   uint8_t abi;
   uint8_t abi_version;
   uint8_t padding[7];
   uint16_t type;
   uint16_t machine_type;
   uint32_t version;
   uint64_t entry_point_address;
   uint64_t program_header_offset;
   uint64_t section_header_offset;
   uint32_t flags;
   uint16_t header_size;
   uint16_t program_header_entry_size;
   uint16_t program_header_entry_count;
   uint16_t section_header_entry_size;
   uint16_t section_header_entry_count;
   uint16_t section_name_string_table_index;
};

struct ELF_SECTION_HEADER_T {
        uint32_t name_offset;
        uint32_t type;
        uint64_t flags;
        uint64_t virtual_address;
        uint64_t offset;
        uint64_t size;
        uint32_t link;
        uint32_t info;
        uint64_t alignment;
        uint64_t entry_size;
};


//bool is_valid_elf(struct ELF_FILE_HEADER_T header);
int read_elf(const uint8_t* elf, bool run);

#endif
