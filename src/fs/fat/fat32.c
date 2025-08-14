#include <fs/fat/fat32.h>
#include <drivers/disk.h>
#include <kernel/printk.h>
#include <stdlib/binop.h>
#include <stdlib/string.h>
#include <ctype.h>
//#include <kernel/pmm.h>
#include  <kernel/mem.h>


FAT fat = {0};

void fat32_init() {
   read_ebpb();
}

uint32_t get_next_cluster(int cluster) {
        uint16_t first_fat_sector = fat.ebpb.reserved_sectors;
        uint8_t fat_table[512];
        uint8_t fat_offset = cluster * 4;
        uint8_t fat_sector = first_fat_sector + (fat_offset / 512);
        uint8_t ent_offset = fat_offset % 512;
        char* fat_contents = readdisk(fat_sector);
        for(int i=0;i<512;i++) {
                fat_table[i] = fat_contents[i];
        }
        uint32_t table_value = combine32bit(fat_table[ent_offset+3],fat_table[ent_offset+2],fat_table[ent_offset+1],fat_table[ent_offset]);
        table_value &= 0x0FFFFFFF;
        if (table_value >= 0x0FFFFFF8) {
                return 0;
        }
        kdebug("NEXT CLUSTER: %zu\n",table_value);
        return table_value;
}



uint32_t get_first_sector_of_cluster(int cluster) {
        uint32_t root_sector = ((fat.ebpb.number_of_root_dir_entries * 32) + (fat.ebpb.bytes_per_sector - 1)) / fat.ebpb.bytes_per_sector;
        kdebug("root_sector %zu\n",root_sector);
        uint32_t first_data_sector = fat.ebpb.reserved_sectors + (fat.ebpb.number_of_FAT * fat.ebpb.sectors_per_FAT) + root_sector;
        kdebug("first_data_sector %zu\n",first_data_sector);
        uint32_t sector = (((cluster - 2) * fat.ebpb.sectors_per_cluster) + first_data_sector);
        kdebug("sector %zu\n",sector);
        return sector;
}

DIR_ENTRY_LIST parse_directory_sector(char* directory_sector) {
    DIR_ENTRY_LIST entry_list;
    entry_list.size = 0;
    if (!directory_sector) {
        kdebug("parse_directory_sector: NULL directory_sector\n");
        return entry_list;
    }
    char* dir_entries[16];
    int entries = 0;
    for (int i = 0; i < 16; i++) { dir_entries[i] = &directory_sector[i * 32]; }
    for (int i = 0; i < 16; i++) {
        if (dir_entries[i][0] != DIR_ENTRY_UNUSED) {
            if (dir_entries[i][DIR_ENTRY_DIR_ATTR_OFFSET] != DIR_ENTRY_ATTR_LONG_FILE_NAME) {
                DIR_ENTRY dir_entry;
                // Use safe copy for name and extension
                strncpy_safe(dir_entry.dir_name, &dir_entries[i][DIR_ENTRY_DIR_NAME_OFFSET], 9);
                dir_entry.dir_name[8] = 0x00;
                strncpy_safe(dir_entry.dir_extension, &dir_entries[i][DIR_ENTRY_DIR_EXT_OFFSET], 4);
                dir_entry.dir_extension[3] = 0x00;
                dir_entry.dir_first_cluster_high = ((uint16_t)dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_HIGH_OFFSET + 1] << 8 | dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_HIGH_OFFSET]);
                dir_entry.dir_first_cluster_low = ((uint16_t)dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_LOW_OFFSET + 1] << 8 | dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_LOW_OFFSET]);
                dir_entry.dir_file_size = combine32bit(dir_entries[i][DIR_ENTRY_DIR_FILE_SIZE_OFFSET + 3], dir_entries[i][DIR_ENTRY_DIR_FILE_SIZE_OFFSET + 2], dir_entries[i][DIR_ENTRY_DIR_FILE_SIZE_OFFSET + 1], dir_entries[i][DIR_ENTRY_DIR_FILE_SIZE_OFFSET]);
                dir_entry.dir_first_cluster = combine32bit(dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_HIGH_OFFSET + 1], dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_HIGH_OFFSET], dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_LOW_OFFSET + 1], dir_entries[i][DIR_ENTRY_DIR_FIRST_CLUSTER_LOW_OFFSET]);
                if (entries < (int)(sizeof(entry_list.list) / sizeof(entry_list.list[0]))) {
                    entry_list.list[entries++] = dir_entry;
                    entry_list.size = entries;
                } else {
                    kdebug("parse_directory_sector: entry_list overflow\n");
                }
            } else {
                // no long file name support, yet
                continue;
            }
        }
    }
    return entry_list;
}

uint8_t* read_file(char* filename, uint8_t* buffer, int size) {
      read_ebpb();
      uint32_t sector = get_first_sector_of_cluster(fat.ebpb.cluster_number_of_root_directory);
      uint8_t* directory_sector = readdisk(sector);
      DIR_ENTRY_LIST dir_entry_list = parse_directory_sector(directory_sector);
      kdebug("Number of directory entries: %d\n", dir_entry_list.size);
      for(int i=0;i<dir_entry_list.size;i++) {
         trim(dir_entry_list.list[i].dir_name);
         trim(dir_entry_list.list[i].dir_extension);
         if (strcmp(filename,dir_entry_list.list[i].dir_name)) {
            kdebug("dir name: [%s]\n", dir_entry_list.list[i].dir_name);
            kdebug("dir extension: [%s]\n",dir_entry_list.list[i].dir_extension);
            kdebug("dir file size: %zu\n\n",dir_entry_list.list[i].dir_file_size);
            kdebug("dir first cluster high: %04X\n", dir_entry_list.list[i].dir_first_cluster_high);
            kdebug("dir first cluster low: %04X\n", dir_entry_list.list[i].dir_first_cluster_low);
            kdebug("dir first cluster: %zu\n", dir_entry_list.list[i].dir_first_cluster);
            //uint8_t buffer[(dir_entry_list.list[i].dir_file_size % 512 > 0)?((dir_entry_list.list[i].dir_file_size / 512 ) + 1 ) * 512:(dir_entry_list.list[i].dir_file_size / 512) * 512];
            //uint8_t* buffer = (uint8_t*) kmalloc();
            kdebug("size of buffer: %d\n",sizeof(buffer));
            int k=0;
            int current_cluster = dir_entry_list.list[i].dir_first_cluster_low;
            while(1) {
               kdebug("reading cluster %d\n",current_cluster);
               int sector = get_first_sector_of_cluster(current_cluster);
               kdebug("reading sector: %d\n",sector);
               uint8_t* sector_contents = (uint8_t*)readdisk(get_first_sector_of_cluster(current_cluster));
               for (int j=0;j<512;j++) { 
                       buffer[j+k] = (uint8_t)sector_contents[j];
               }
               current_cluster = get_next_cluster(current_cluster);
               if (current_cluster == 0) {
                  break;
               } else {
                       k += 512;
               }
            }
            //return buffer; // found
            return buffer; // found
         }
      }
      return directory_sector;
}

EBPB read_ebpb() {
    kdebug("[FAT32] read_ebpb\n");
    char* sector_zero_array = readdisk(0);
    if (!sector_zero_array) {
        kdebug("[FAT32] ERROR: Failed to read sector 0 from disk!\n");
        EBPB ebpb = {0};
        ebpb.populated = false;
        fat.ebpb = ebpb;
        return fat.ebpb;
    }
    uint8_t sector_zero[512];
    memcpy(sector_zero, sector_zero_array, 512);
    EBPB ebpb;
    for (int i = 0; i < 2; i++) { ebpb.jmp_code[i] = sector_zero[i]; }
    for (int i = 0; i < 8; i++) { ebpb.OEM_identifier[i] = sector_zero[i + EBPB_OEM_IDENTIFIER_OFFSET]; } ebpb.OEM_identifier[8] = 0x00;
    ebpb.bytes_per_sector = ((uint16_t)sector_zero[EBPB_NUMBER_BYTES_PER_SECTOR_OFFSET + 1] << 8) | sector_zero[EBPB_NUMBER_BYTES_PER_SECTOR_OFFSET];
    ebpb.sectors_per_cluster = sector_zero[EBPB_SECTORS_PER_CLUSTER_OFFSET];
    ebpb.reserved_sectors = ((uint16_t)sector_zero[EBPB_RESERVED_SECTORS_OFFSET + 1] << 8 | sector_zero[EBPB_RESERVED_SECTORS_OFFSET]);
    ebpb.number_of_FAT = sector_zero[EBPB_NUMBER_FAT_OFFSET];
    ebpb.number_of_root_dir_entries = ((uint16_t)sector_zero[EBPB_NUMBER_ROOT_ENTRIES_OFFSET + 1] << 8 | sector_zero[EBPB_NUMBER_ROOT_ENTRIES_OFFSET]);
    ebpb.total_sectors_logical_volume = ((uint16_t)sector_zero[EBPB_TOTAL_SECTORS_LOGICAL_VOLUME_OFFSET + 1] << 8 | sector_zero[EBPB_TOTAL_SECTORS_LOGICAL_VOLUME_OFFSET]);
    ebpb.data_media_descriptor = sector_zero[EBPB_DATA_MEDIA_DESCRIPTOR_OFFSET];
    ebpb.number_of_sectors_per_FAT = ((uint16_t)sector_zero[EBPB_NUMBER_SECTORS_PER_FAT_OFFSET + 1] << 8 | sector_zero[EBPB_NUMBER_SECTORS_PER_FAT_OFFSET]);
    ebpb.number_of_sectors_per_track = ((uint16_t)sector_zero[EBPB_NUMBER_SECTORS_PER_TRACK_OFFSET + 1] << 8 | sector_zero[EBPB_NUMBER_SECTORS_PER_TRACK_OFFSET]);
    ebpb.number_of_heads = ((uint16_t)sector_zero[EBPB_NUMBER_HEADS_OFFSET + 1] << 8 | sector_zero[EBPB_NUMBER_HEADS_OFFSET]);
    ebpb.number_of_hidden_sectors = combine32bit(sector_zero[EBPB_NUMBER_HIDDEN_SECTORS_OFFSET + 3], sector_zero[EBPB_NUMBER_HIDDEN_SECTORS_OFFSET + 2], sector_zero[EBPB_NUMBER_HIDDEN_SECTORS_OFFSET + 1], sector_zero[EBPB_NUMBER_HIDDEN_SECTORS_OFFSET]);
    ebpb.sectors_per_FAT = ((uint16_t)sector_zero[EBPB_SECTORS_PER_FAT_OFFSET + 1] << 8 | sector_zero[EBPB_SECTORS_PER_FAT_OFFSET]);
    ebpb.large_sector_count = combine32bit(sector_zero[EBPB_LARGE_SECTOR_COUNT_OFFSET + 3], sector_zero[EBPB_LARGE_SECTOR_COUNT_OFFSET + 2], sector_zero[EBPB_LARGE_SECTOR_COUNT_OFFSET + 1], sector_zero[EBPB_LARGE_SECTOR_COUNT_OFFSET]);
    ebpb.flags = ((uint16_t)sector_zero[EBPB_FLAGS_OFFSET + 1] << 8 | sector_zero[EBPB_FLAGS_OFFSET]);
    ebpb.version_number = ((uint16_t)sector_zero[EBPB_FAT_VERSION_NUMBER_OFFSET + 1] << 8 | sector_zero[EBPB_FAT_VERSION_NUMBER_OFFSET]);
    ebpb.cluster_number_of_root_directory = combine32bit(sector_zero[EBPB_CLUSTER_NUMBER_ROOT_DIRECTORY_OFFSET + 3], sector_zero[EBPB_CLUSTER_NUMBER_ROOT_DIRECTORY_OFFSET + 2], sector_zero[EBPB_CLUSTER_NUMBER_ROOT_DIRECTORY_OFFSET + 1], sector_zero[EBPB_CLUSTER_NUMBER_ROOT_DIRECTORY_OFFSET]);
    ebpb.sector_number_of_FSInfo_structure = ((uint16_t)sector_zero[EBPB_SECTOR_NUMBER_FSINFO_STRUCTURE_OFFSET + 1] << 8 | sector_zero[EBPB_SECTOR_NUMBER_FSINFO_STRUCTURE_OFFSET]);
    ebpb.sector_number_of_backup_boot_sector = ((uint16_t)sector_zero[EBPB_SECTOR_NUMBER_BACKUP_SECTOR_OFFSET + 1] << 8 | sector_zero[EBPB_SECTOR_NUMBER_BACKUP_SECTOR_OFFSET]);
    for (int i = 0; i < 12; i++) { ebpb.reserved[i] = sector_zero[i + EBPB_RESERVED_OFFSET]; }
    ebpb.drive_number = sector_zero[EBPB_DRIVE_NUMBER_OFFSET];
    ebpb.nt_flags = sector_zero[EBPB_NT_FLAGS_OFFSET];
    ebpb.signature = sector_zero[EBPB_SIGNATURE_OFFSET];
    ebpb.volume_id_serial_number = combine32bit(sector_zero[EBPB_VOLUME_ID_SERIAL_NUMBER_OFFSET + 3], sector_zero[EBPB_VOLUME_ID_SERIAL_NUMBER_OFFSET + 2], sector_zero[EBPB_VOLUME_ID_SERIAL_NUMBER_OFFSET + 1], sector_zero[EBPB_VOLUME_ID_SERIAL_NUMBER_OFFSET]);
    for (int i = 0; i < 11; i++) { ebpb.volume_label_string[i] = sector_zero[i + EBPB_VOLUME_LABEL_STRING_OFFSET]; } ebpb.volume_label_string[11] = 0x00;
    for (int i = 0; i < 8; i++) { ebpb.system_identifier[i] = sector_zero[i + EBPB_SYSTEM_IDENTIFIER_OFFSET]; } ebpb.system_identifier[8] = 0x00;
    ebpb.populated = true;
    fat.ebpb = ebpb;
    return fat.ebpb;
}

void print_fat32_ebpb() {
        printk("[FAT32] FAT32 EBPB structure dump\n");
        printk("-----------------------------------\n");
        printk("[FAT32] OEM_identifier: [%s]\n",fat.ebpb.OEM_identifier);
        printk("[FAT32] bytes_per_sector: %d\n",fat.ebpb.bytes_per_sector);
        printk("[FAT32] sectors_per_cluster: %d\n",fat.ebpb.sectors_per_cluster);
        printk("[FAT32] reserved_sectors: %d\n",fat.ebpb.reserved_sectors);
        printk("[FAT32] number of FAT: %d\n",fat.ebpb.number_of_FAT);
        printk("[FAT32] number of root dir entries: %d\n",fat.ebpb.number_of_root_dir_entries);
        printk("[FAT32] total sectors in logical volume: %d\n",fat.ebpb.total_sectors_logical_volume);
        printk("[FAT32] data media descriptor: 0x%02X\n",fat.ebpb.data_media_descriptor);
        printk("[FAT32] number sectors per FAT (FAT12 or FAT16): %d\n",fat.ebpb.number_of_sectors_per_FAT);
        printk("[FAT32] number sectors per track: %d\n",fat.ebpb.number_of_sectors_per_track);
        printk("[FAT32] number of heads: %d\n",fat.ebpb.number_of_heads);
        printk("[FAT32] number of hidden sectors: %zu\n",fat.ebpb.number_of_hidden_sectors);
        printk("[FAT32] large sector count: %zu\n",fat.ebpb.large_sector_count);
        printk("[FAT32] sectors per FAT (FAT32): %d\n",fat.ebpb.sectors_per_FAT);
        printk("[FAT32] flags: 0x%04X\n",fat.ebpb.flags);
        printk("[FAT32] version number: 0x%04X\n",fat.ebpb.version_number);
        printk("[FAT32] cluster number of root directory: %zu\n",fat.ebpb.cluster_number_of_root_directory);
        printk("[FAT32] sector number of FSInfo structure: %d\n",fat.ebpb.sector_number_of_FSInfo_structure);
        printk("[FAT32] sector number of backup sector: %d\n",fat.ebpb.sector_number_of_backup_boot_sector);
        printk("[FAT32] reserved: 0x%12X\n",fat.ebpb.reserved); // review
        printk("[FAT32] drive number: 0x%01X\n",fat.ebpb.drive_number);
        printk("[FAT32] Windows NT flags: 0x%01X\n",fat.ebpb.nt_flags);
        printk("[FAT32] signature: 0x%01X\n",fat.ebpb.signature);
        printk("[FAT32] volume id serial number: 0x%04X\n",fat.ebpb.volume_id_serial_number); // review
        printk("[FAT32] volume label string: [%s]\n",fat.ebpb.volume_label_string); 
        printk("[FAT32] system identifier string: [%s]\n",fat.ebpb.system_identifier); 
}
