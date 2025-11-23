#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/fat32.h"

/* Mount the FAT32 image */
int mount_image(FileSystem *fs, const char *image_path) {
    fs->image = fopen(image_path, "r+b");
    if (!fs->image) {
        return -1;
    }

    /* Read boot sector */
    fseek(fs->image, 0, SEEK_SET);
    if (fread(&fs->boot_sector, sizeof(BootSector), 1, fs->image) != 1) {
        fclose(fs->image);
        return -1;
    }

    /* Calculate important values */
    fs->fat_start_sector = fs->boot_sector.BPB_RsvdSecCnt;
    fs->data_start_sector = fs->boot_sector.BPB_RsvdSecCnt + 
                            (fs->boot_sector.BPB_NumFATs * 
                             fs->boot_sector.BPB_FATSz32);
    fs->root_cluster = fs->boot_sector.BPB_RootClus;
    fs->current_cluster = fs->root_cluster;
    
    uint32_t total_sectors = fs->boot_sector.BPB_TotSec32;
    uint32_t data_sectors = total_sectors - fs->data_start_sector;
    fs->total_clusters = data_sectors / fs->boot_sector.BPB_SecPerClus;

    strcpy(fs->current_path, "/");
    
    /* Extract image name from path */
    const char *slash = strrchr(image_path, '/');
    strcpy(fs->image_name, slash ? slash + 1 : image_path);

    /* Initialize open files */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        fs->open_files[i].is_open = 0;
    }

    return 0;
}

/* Close the image */
void close_image(FileSystem *fs) {
    if (fs->image) {
        fclose(fs->image);
        fs->image = NULL;
    }
}

/* Get FAT entry for a cluster */
uint32_t get_fat_entry(FileSystem *fs, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + 
                          (fat_offset / fs->boot_sector.BPB_BytsPerSec);
    uint32_t entry_offset = fat_offset % fs->boot_sector.BPB_BytsPerSec;

    fseek(fs->image, fat_sector * fs->boot_sector.BPB_BytsPerSec + entry_offset,
          SEEK_SET);
    uint32_t entry;
    fread(&entry, 4, 1, fs->image);
    return entry & 0x0FFFFFFF;
}

/* Set FAT entry for a cluster */
void set_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t entry_offset = fat_offset % fs->boot_sector.BPB_BytsPerSec;

    value = value & 0x0FFFFFFF;
    
    /* Write to both FATs */
    for (int i = 0; i < fs->boot_sector.BPB_NumFATs; i++) {
        uint32_t fat_base = fs->fat_start_sector + 
                            (i * fs->boot_sector.BPB_FATSz32);
        fseek(fs->image, 
              (fat_base + (fat_offset / fs->boot_sector.BPB_BytsPerSec)) * 
              fs->boot_sector.BPB_BytsPerSec + entry_offset, SEEK_SET);
        fwrite(&value, 4, 1, fs->image);
    }
    fflush(fs->image);
}

/* Get first sector of a cluster */
uint32_t get_first_sector_of_cluster(FileSystem *fs, uint32_t cluster) {
    return ((cluster - 2) * fs->boot_sector.BPB_SecPerClus) + 
           fs->data_start_sector;
}

/* Check if cluster is valid */
int is_valid_cluster(FileSystem *fs, uint32_t cluster) {
    return cluster >= 2 && cluster < (fs->total_clusters + 2) &&
           cluster < 0x0FFFFFF8;
}

/* Read directory entries from a cluster */
DirEntry *read_directory(FileSystem *fs, uint32_t cluster, int *num_entries) {
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec * 
                                  fs->boot_sector.BPB_SecPerClus;
    int max_entries = bytes_per_cluster / DIR_ENTRY_SIZE;
    DirEntry *entries = malloc(max_entries * sizeof(DirEntry) * 16);
    int entry_count = 0;

    uint32_t current_cluster = cluster;
    while (is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec, SEEK_SET);

        for (int i = 0; i < max_entries; i++) {
            DirEntry entry;
            fread(&entry, sizeof(DirEntry), 1, fs->image);

            if (entry.DIR_Name[0] == 0x00) {
                *num_entries = entry_count;
                return entries;
            }

            if (entry.DIR_Name[0] == 0xE5) {
                continue;
            }

            if (entry.DIR_Attr == ATTR_LONG_NAME) {
                continue;
            }

            entries[entry_count++] = entry;
        }

        current_cluster = get_fat_entry(fs, current_cluster);
    }

    *num_entries = entry_count;
    return entries;
}

/* Find directory entry by name */
DirEntry *find_entry(FileSystem *fs, uint32_t cluster, const char *name) {
    char formatted_name[12];
    format_filename(name, formatted_name);

    int num_entries;
    DirEntry *entries = read_directory(fs, cluster, &num_entries);

    for (int i = 0; i < num_entries; i++) {
        if (memcmp(entries[i].DIR_Name, formatted_name, 11) == 0) {
            DirEntry *result = malloc(sizeof(DirEntry));
            *result = entries[i];
            free(entries);
            return result;
        }
    }

    free(entries);
    return NULL;
}

/* Format filename to FAT32 11-byte format */
void format_filename(const char *input, char *output) {
    memset(output, ' ', 11);
    output[11] = '\0';

    if (strcmp(input, ".") == 0) {
        output[0] = '.';
        return;
    }
    if (strcmp(input, "..") == 0) {
        output[0] = '.';
        output[1] = '.';
        return;
    }

    int i = 0, o = 0;
    while (input[i] && input[i] != '.' && o < 8) {
        output[o++] = toupper(input[i++]);
    }

    if (input[i] == '.') {
        i++;
        o = 8;
        while (input[i] && o < 11) {
            output[o++] = toupper(input[i++]);
        }
    }
}

/* Parse FAT32 filename to readable format */
void parse_filename(const char *formatted, char *output) {
    int i = 0, o = 0;

    /* Handle special cases */
    if (formatted[0] == '.' && formatted[1] == ' ') {
        output[0] = '.';
        output[1] = '\0';
        return;
    }
    if (formatted[0] == '.' && formatted[1] == '.') {
        output[0] = '.';
        output[1] = '.';
        output[2] = '\0';
        return;
    }

    /* Copy name part */
    for (i = 0; i < 8 && formatted[i] != ' '; i++) {
        output[o++] = formatted[i];
    }

    /* Copy extension part */
    int has_ext = 0;
    for (i = 8; i < 11 && formatted[i] != ' '; i++) {
        if (!has_ext) {
            output[o++] = '.';
            has_ext = 1;
        }
        output[o++] = formatted[i];
    }

    output[o] = '\0';
}

/* Allocate a new cluster */
uint32_t allocate_cluster(FileSystem *fs) {
    for (uint32_t cluster = 2; cluster < fs->total_clusters + 2; cluster++) {
        uint32_t entry = get_fat_entry(fs, cluster);
        if (entry == 0) {
            set_fat_entry(fs, cluster, 0x0FFFFFFF);
            
            /* Zero out the cluster */
            uint32_t sector = get_first_sector_of_cluster(fs, cluster);
            uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                         fs->boot_sector.BPB_SecPerClus;
            uint8_t *zero_buffer = calloc(bytes_per_cluster, 1);
            fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec, SEEK_SET);
            fwrite(zero_buffer, bytes_per_cluster, 1, fs->image);
            free(zero_buffer);
            fflush(fs->image);
            
            return cluster;
        }
    }
    return 0;
}

/* Free a cluster chain */
void free_cluster_chain(FileSystem *fs, uint32_t cluster) {
    while (is_valid_cluster(fs, cluster)) {
        uint32_t next = get_fat_entry(fs, cluster);
        set_fat_entry(fs, cluster, 0);
        cluster = next;
    }
}

/* Write a directory entry */
void write_directory_entry(FileSystem *fs, uint32_t cluster, DirEntry *entry,
                          int entry_index) {
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    int entries_per_cluster = bytes_per_cluster / DIR_ENTRY_SIZE;

    uint32_t current_cluster = cluster;
    int current_index = entry_index;

    while (current_index >= entries_per_cluster) {
        current_index -= entries_per_cluster;
        current_cluster = get_fat_entry(fs, current_cluster);
        if (!is_valid_cluster(fs, current_cluster)) {
            return;
        }
    }

    uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
    uint32_t offset = sector * fs->boot_sector.BPB_BytsPerSec +
                      current_index * DIR_ENTRY_SIZE;

    fseek(fs->image, offset, SEEK_SET);
    fwrite(entry, sizeof(DirEntry), 1, fs->image);
    fflush(fs->image);
}

/* Find free entry index in directory */
int find_free_entry_index(FileSystem *fs, uint32_t cluster) {
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    int max_entries = bytes_per_cluster / DIR_ENTRY_SIZE;
    int entry_index = 0;

    uint32_t current_cluster = cluster;
    while (is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec, SEEK_SET);

        for (int i = 0; i < max_entries; i++) {
            DirEntry entry;
            fread(&entry, sizeof(DirEntry), 1, fs->image);

            if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) {
                return entry_index + i;
            }
        }

        entry_index += max_entries;
        uint32_t next_cluster = get_fat_entry(fs, current_cluster);
        if (!is_valid_cluster(fs, next_cluster)) {
            /* Need to allocate new cluster */
            next_cluster = allocate_cluster(fs);
            if (next_cluster == 0) {
                return -1;
            }
            set_fat_entry(fs, current_cluster, next_cluster);
        }
        current_cluster = next_cluster;
    }

    return entry_index;
}

/* Create a directory entry */
int create_directory_entry(FileSystem *fs, uint32_t parent_cluster,
                          const char *name, uint8_t attr,
                          uint32_t first_cluster, uint32_t size) {
    int entry_index = find_free_entry_index(fs, parent_cluster);
    if (entry_index < 0) {
        return -1;
    }

    DirEntry entry;
    memset(&entry, 0, sizeof(DirEntry));
    format_filename(name, (char *)entry.DIR_Name);
    entry.DIR_Attr = attr;
    entry.DIR_FstClusHI = (first_cluster >> 16) & 0xFFFF;
    entry.DIR_FstClusLO = first_cluster & 0xFFFF;
    entry.DIR_FileSize = size;

    write_directory_entry(fs, parent_cluster, &entry, entry_index);
    return 0;
}

/* Delete a directory entry */
int delete_directory_entry(FileSystem *fs, uint32_t cluster, const char *name) {
    char formatted_name[12];
    format_filename(name, formatted_name);

    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    int max_entries = bytes_per_cluster / DIR_ENTRY_SIZE;
    int entry_index = 0;

    uint32_t current_cluster = cluster;
    while (is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec, SEEK_SET);

        for (int i = 0; i < max_entries; i++) {
            DirEntry entry;
            fread(&entry, sizeof(DirEntry), 1, fs->image);

            if (entry.DIR_Name[0] == 0x00) {
                return -1;
            }

            if (memcmp(entry.DIR_Name, formatted_name, 11) == 0) {
                entry.DIR_Name[0] = 0xE5;
                write_directory_entry(fs, cluster, &entry, entry_index + i);
                return 0;
            }
        }

        entry_index += max_entries;
        current_cluster = get_fat_entry(fs, current_cluster);
    }

    return -1;
}

/* Check if directory is empty (only has . and ..) */
int is_directory_empty(FileSystem *fs, uint32_t cluster) {
    int num_entries;
    DirEntry *entries = read_directory(fs, cluster, &num_entries);
    int count = 0;

    for (int i = 0; i < num_entries; i++) {
        if (entries[i].DIR_Name[0] != '.' &&
            entries[i].DIR_Name[0] != 0x00 &&
            entries[i].DIR_Name[0] != 0xE5) {
            count++;
        }
    }

    free(entries);
    return count == 0;
}
