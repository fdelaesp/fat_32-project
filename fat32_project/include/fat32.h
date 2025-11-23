#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>

#define MAX_OPEN_FILES 10
#define MAX_PATH_LENGTH 256
#define DIR_ENTRY_SIZE 32
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

/* FAT32 Boot Sector Structure */
typedef struct __attribute__((packed)) {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint32_t BS_VolID;
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];
} BootSector;

/* FAT32 Directory Entry Structure */
typedef struct __attribute__((packed)) {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirEntry;

/* Open File Structure */
typedef struct {
    char filename[12];
    char mode[4];
    uint32_t offset;
    char path[MAX_PATH_LENGTH];
    uint32_t first_cluster;
    uint32_t size;
    int is_open;
} OpenFile;

/* File System State */
typedef struct {
    FILE *image;
    BootSector boot_sector;
    uint32_t current_cluster;
    char current_path[MAX_PATH_LENGTH];
    char image_name[256];
    OpenFile open_files[MAX_OPEN_FILES];
    uint32_t data_start_sector;
    uint32_t fat_start_sector;
    uint32_t root_cluster;
    uint32_t total_clusters;
} FileSystem;

/* Function declarations */
int mount_image(FileSystem *fs, const char *image_path);
void close_image(FileSystem *fs);
uint32_t get_fat_entry(FileSystem *fs, uint32_t cluster);
void set_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value);
uint32_t get_first_sector_of_cluster(FileSystem *fs, uint32_t cluster);
DirEntry *read_directory(FileSystem *fs, uint32_t cluster, int *num_entries);
DirEntry *find_entry(FileSystem *fs, uint32_t cluster, const char *name);
uint32_t allocate_cluster(FileSystem *fs);
void free_cluster_chain(FileSystem *fs, uint32_t cluster);
void format_filename(const char *input, char *output);
void parse_filename(const char *formatted, char *output);
int is_valid_cluster(FileSystem *fs, uint32_t cluster);
void write_directory_entry(FileSystem *fs, uint32_t cluster, DirEntry *entry, 
                          int entry_index);
int find_free_entry_index(FileSystem *fs, uint32_t cluster);
int create_directory_entry(FileSystem *fs, uint32_t parent_cluster, 
                          const char *name, uint8_t attr, uint32_t first_cluster,
                          uint32_t size);
int delete_directory_entry(FileSystem *fs, uint32_t cluster, const char *name);
int is_directory_empty(FileSystem *fs, uint32_t cluster);

#endif
