#include <stdint.h>
#include <stdbool.h>

#ifndef FAT32_H
#define FAT32_H

// Extended BIOS Parameter Block
// Every number encoded in little endian
// ASCII is big endian

#define EBPB_JUMP_OFFSET 0x00
#define EBPB_OEM_IDENTIFIER_OFFSET 0x03
#define EBPB_NUMBER_BYTES_PER_SECTOR_OFFSET 0x0B
#define EBPB_SECTORS_PER_CLUSTER_OFFSET 0x0D
#define EBPB_RESERVED_SECTORS_OFFSET 0x0E
#define EBPB_NUMBER_FAT_OFFSET 0x10
#define EBPB_NUMBER_ROOT_ENTRIES_OFFSET 0x11
#define EBPB_TOTAL_SECTORS_LOGICAL_VOLUME_OFFSET 0x13
#define EBPB_DATA_MEDIA_DESCRIPTOR_OFFSET 0x15
#define EBPB_NUMBER_SECTORS_PER_FAT_OFFSET 0x16
#define EBPB_NUMBER_SECTORS_PER_TRACK_OFFSET 0x18
#define EBPB_NUMBER_HEADS_OFFSET 0x1A
#define EBPB_NUMBER_HIDDEN_SECTORS_OFFSET 0x1C
#define EBPB_LARGE_SECTOR_COUNT_OFFSET 0x20
#define EBPB_SECTORS_PER_FAT_OFFSET 0x24
#define EBPB_FLAGS_OFFSET 0x28
#define EBPB_FAT_VERSION_NUMBER_OFFSET 0x2A
#define EBPB_CLUSTER_NUMBER_ROOT_DIRECTORY_OFFSET 0x2C
#define EBPB_SECTOR_NUMBER_FSINFO_STRUCTURE_OFFSET 0x30
#define EBPB_SECTOR_NUMBER_BACKUP_SECTOR_OFFSET 0x32
#define EBPB_RESERVED_OFFSET 0x34
#define EBPB_DRIVE_NUMBER_OFFSET 0x40
#define EBPB_NT_FLAGS_OFFSET 0x41
#define EBPB_SIGNATURE_OFFSET 0x42
#define EBPB_VOLUME_ID_SERIAL_NUMBER_OFFSET 0x43
#define EBPB_VOLUME_LABEL_STRING_OFFSET 0x47
#define EBPB_SYSTEM_IDENTIFIER_OFFSET 0x52

#define EBPB_BOOT_CODE_OFFSET 0x5A
#define EBPB_BOOTABLE_PARTITION_SIGNATURE 0x1FE


#define DIR_ENTRY_DIR_NAME_OFFSET 0x00
#define DIR_ENTRY_DIR_EXT_OFFSET 0x08
#define DIR_ENTRY_DIR_ATTR_OFFSET 0x0B
#define DIR_ENTRY_NT_RESERVED_OFFSET 0x0C
#define DIR_ENTRY_DIR_CREATION_HUNDRETH_SECOND_OFFSET 0x0D
#define DIR_ENTRY_DIR_CREATION_TIME_OFFSET 0x0E
#define DIR_ENTRY_DIR_CREATION_DATE_OFFSET 0x10
#define DIR_ENTRY_DIR_LAST_ACCESS_DATE_OFFSET 0x12
#define DIR_ENTRY_DIR_FIRST_CLUSTER_HIGH_OFFSET 0x14
#define DIR_ENTRY_DIR_WRITE_TIME_OFFSET 0x16
#define DIR_ENTRY_DIR_WRITE_DATE_OFFSET 0x18
#define DIR_ENTRY_DIR_FIRST_CLUSTER_LOW_OFFSET 0x1A
#define DIR_ENTRY_DIR_FILE_SIZE_OFFSET 0x1C

#define DIR_ENTRY_UNUSED 0x00
#define DIR_ENTRY_LAST_AND_UNUSED 0x00

#define DIR_ENTRY_ATTR_READONLY 0x01
#define DIR_ENTRY_ATTR_HIDDEN 0x02
#define DIR_ENTRY_ATTR_SYSTEM 0x04
#define DIR_ENTRY_ATTR_VOLUME_LABEL 0x08
#define DIR_ENTRY_ATTR_LONG_FILE_NAME 0x0F
#define DIR_ENTRY_ATTR_DIRECTORY 0x10
#define DIR_ENTRY_ATTR_ARCHIVE 0x20


typedef struct DIR_ENTRY_T {
      char        dir_name[9];
      char        dir_extension[4];
      uint8_t     dir_attributes;
      uint8_t     nt_reserved;
      uint8_t     dir_creation_time_hundreth_second;
      uint16_t    dir_creation_time;
      uint16_t    dir_creation_date;
      uint16_t    dir_last_access_date;
      uint16_t    dir_first_cluster_high;
      uint16_t    dir_write_time;
      uint16_t    dir_write_date;
      uint16_t    dir_first_cluster_low;
      uint32_t    dir_file_size;
      uint32_t    dir_first_cluster;
} DIR_ENTRY;

typedef struct DIR_ENTRY_LIST_T {
        DIR_ENTRY list[16];
        uint8_t size;
} DIR_ENTRY_LIST;

typedef struct EBPB_T { 
   // General FAT BPB
   uint8_t     jmp_code[3];
   char        OEM_identifier[9];  
   uint16_t    bytes_per_sector;
   uint8_t     sectors_per_cluster;
   uint16_t    reserved_sectors;
   uint8_t     number_of_FAT;
   uint16_t    number_of_root_dir_entries;
   uint16_t    total_sectors_logical_volume;
   uint8_t     data_media_descriptor;
   uint16_t    number_of_sectors_per_FAT; // only relevant in FAT12 or FAT16
   uint16_t    number_of_sectors_per_track;
   uint16_t    number_of_heads;
   uint32_t    number_of_hidden_sectors;
   uint32_t    large_sector_count;
   // FAT32 EBPB
   uint32_t    sectors_per_FAT;  // FAT32 -- see number_of_sectors_per_FAT above
   uint16_t    flags;
   uint16_t    version_number;
   uint32_t    cluster_number_of_root_directory;
   uint16_t    sector_number_of_FSInfo_structure;
   uint16_t    sector_number_of_backup_boot_sector;
   uint8_t     reserved[12];
   uint8_t     drive_number; // same numbering as BIOS irq 0x13
   uint8_t     nt_flags;
   uint8_t     signature;
   uint32_t    volume_id_serial_number;
   uint8_t     volume_label_string[12];
   uint8_t     system_identifier[9];
   bool        populated;
} EBPB;

typedef struct {
   EBPB ebpb;
} FAT;

extern FAT fat;




void fat32_init();
EBPB read_ebpb();
void print_fat32_ebpb();
uint8_t* read_file(char* filename, uint8_t* buffer, int size);

#endif
