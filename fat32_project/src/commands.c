#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/commands.h"
#include "../include/fat32.h"

/* Helper: Find open file */
OpenFile *find_open_file(FileSystem *fs, const char *filename) {
    char formatted_name[12];
    format_filename(filename, formatted_name);

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fs->open_files[i].is_open) {
            char open_formatted[12];
            format_filename(fs->open_files[i].filename, open_formatted);
            if (memcmp(open_formatted, formatted_name, 11) == 0) {
                return &fs->open_files[i];
            }
        }
    }
    return NULL;
}

/* Helper: Add open file */
int add_open_file(FileSystem *fs, const char *filename, const char *mode,
                  const char *path, uint32_t first_cluster, uint32_t size) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fs->open_files[i].is_open) {
            strncpy(fs->open_files[i].filename, filename, 11);
            fs->open_files[i].filename[11] = '\0';
            strncpy(fs->open_files[i].mode, mode, 3);
            fs->open_files[i].mode[3] = '\0';
            strncpy(fs->open_files[i].path, path, MAX_PATH_LENGTH - 1);
            fs->open_files[i].path[MAX_PATH_LENGTH - 1] = '\0';
            fs->open_files[i].offset = 0;
            fs->open_files[i].first_cluster = first_cluster;
            fs->open_files[i].size = size;
            fs->open_files[i].is_open = 1;
            return 0;
        }
    }
    return -1;
}

/* Helper: Remove open file */
void remove_open_file(FileSystem *fs, const char *filename) {
    OpenFile *file = find_open_file(fs, filename);
    if (file) {
        file->is_open = 0;
    }
}

/* info command */
void cmd_info(FileSystem *fs) {
    printf("position of root cluster: %u\n", fs->boot_sector.BPB_RootClus);
    printf("bytes per sector: %u\n", fs->boot_sector.BPB_BytsPerSec);
    printf("sectors per cluster: %u\n", fs->boot_sector.BPB_SecPerClus);
    printf("total # of clusters in data region: %u\n", fs->total_clusters);
    printf("# of entries in one FAT: %u\n", 
           fs->boot_sector.BPB_FATSz32 * fs->boot_sector.BPB_BytsPerSec / 4);
    
    /* Calculate image size */
    fseek(fs->image, 0, SEEK_END);
    long size = ftell(fs->image);
    printf("size of image (in bytes): %ld\n", size);
}

/* ls command */
void cmd_ls(FileSystem *fs) {
    int num_entries;
    DirEntry *entries = read_directory(fs, fs->current_cluster, &num_entries);

    for (int i = 0; i < num_entries; i++) {
        char filename[13];
        parse_filename((char *)entries[i].DIR_Name, filename);
        printf("%s\n", filename);
    }

    free(entries);
}

/* cd command */
void cmd_cd(FileSystem *fs, const char *dirname) {
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    DirEntry *entry = find_entry(fs, fs->current_cluster, dirname);
    if (!entry) {
        printf("Error: Directory does not exist\n");
        return;
    }

    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
        printf("Error: Not a directory\n");
        free(entry);
        return;
    }

    uint32_t new_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                           entry->DIR_FstClusLO;

    if (strcmp(dirname, "..") == 0) {
        /* Going to parent */
        if (strcmp(fs->current_path, "/") != 0) {
            char *last_slash = strrchr(fs->current_path, '/');
            if (last_slash == fs->current_path) {
                strcpy(fs->current_path, "/");
            } else {
                *last_slash = '\0';
            }
        }
        fs->current_cluster = (new_cluster == 0) ? 
                              fs->root_cluster : new_cluster;
    } else {
        /* Going to child */
        if (strcmp(fs->current_path, "/") != 0) {
            strcat(fs->current_path, "/");
        }
        strcat(fs->current_path, dirname);
        fs->current_cluster = new_cluster;
    }

    free(entry);
}

/* mkdir command */
void cmd_mkdir(FileSystem *fs, const char *dirname) {
    /* Check if already exists */
    DirEntry *existing = find_entry(fs, fs->current_cluster, dirname);
    if (existing) {
        printf("Error: Directory/file already exists\n");
        free(existing);
        return;
    }

    /* Allocate cluster for new directory */
    uint32_t new_cluster = allocate_cluster(fs);
    if (new_cluster == 0) {
        printf("Error: No free clusters available\n");
        return;
    }

    /* Create "." entry */
    DirEntry dot_entry;
    memset(&dot_entry, 0, sizeof(DirEntry));
    memset(dot_entry.DIR_Name, ' ', 11);
    dot_entry.DIR_Name[0] = '.';
    dot_entry.DIR_Attr = ATTR_DIRECTORY;
    dot_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    dot_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    write_directory_entry(fs, new_cluster, &dot_entry, 0);

    /* Create ".." entry */
    DirEntry dotdot_entry;
    memset(&dotdot_entry, 0, sizeof(DirEntry));
    memset(dotdot_entry.DIR_Name, ' ', 11);
    dotdot_entry.DIR_Name[0] = '.';
    dotdot_entry.DIR_Name[1] = '.';
    dotdot_entry.DIR_Attr = ATTR_DIRECTORY;
    uint32_t parent = (fs->current_cluster == fs->root_cluster) ? 
                      0 : fs->current_cluster;
    dotdot_entry.DIR_FstClusHI = (parent >> 16) & 0xFFFF;
    dotdot_entry.DIR_FstClusLO = parent & 0xFFFF;
    write_directory_entry(fs, new_cluster, &dotdot_entry, 1);

    /* Create entry in parent directory */
    if (create_directory_entry(fs, fs->current_cluster, dirname,
                               ATTR_DIRECTORY, new_cluster, 0) < 0) {
        printf("Error: Failed to create directory entry\n");
        free_cluster_chain(fs, new_cluster);
    }
}

/* creat command */
void cmd_creat(FileSystem *fs, const char *filename) {
    /* Check if already exists */
    DirEntry *existing = find_entry(fs, fs->current_cluster, filename);
    if (existing) {
        printf("Error: Directory/file already exists\n");
        free(existing);
        return;
    }

    /* Create file with no clusters allocated initially */
    if (create_directory_entry(fs, fs->current_cluster, filename,
                               ATTR_ARCHIVE, 0, 0) < 0) {
        printf("Error: Failed to create file entry\n");
    }
}

/* open command */
void cmd_open(FileSystem *fs, const char *filename, const char *mode) {
    /* Validate mode */
    if (strcmp(mode, "-r") != 0 && strcmp(mode, "-w") != 0 &&
        strcmp(mode, "-rw") != 0 && strcmp(mode, "-wr") != 0) {
        printf("Error: Invalid mode\n");
        return;
    }

    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }

    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: Cannot open a directory\n");
        free(entry);
        return;
    }

    /* Check if already open */
    if (find_open_file(fs, filename)) {
        printf("Error: File is already open\n");
        free(entry);
        return;
    }

    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;

    /* Add to open files */
    char mode_str[4];
    strncpy(mode_str, mode + 1, 3);
    mode_str[3] = '\0';

    if (add_open_file(fs, filename, mode_str, fs->current_path,
                      first_cluster, entry->DIR_FileSize) < 0) {
        printf("Error: Too many open files\n");
    }

    free(entry);
}

/* close command */
void cmd_close(FileSystem *fs, const char *filename) {
    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }
    free(entry);

    /* Check if file is open */
    if (!find_open_file(fs, filename)) {
        printf("Error: File is not open\n");
        return;
    }

    remove_open_file(fs, filename);
}

/* lsof command */
void cmd_lsof(FileSystem *fs) {
    int has_open = 0;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fs->open_files[i].is_open) {
            if (!has_open) {
                printf("Index\tFilename\tMode\tOffset\tPath\n");
                has_open = 1;
            }
            printf("%d\t%s\t\t%s\t%u\t%s\n", i,
                   fs->open_files[i].filename,
                   fs->open_files[i].mode,
                   fs->open_files[i].offset,
                   fs->open_files[i].path);
        }
    }
    if (!has_open) {
        printf("No files are currently open\n");
    }
}

/* lseek command */
void cmd_lseek(FileSystem *fs, const char *filename, uint32_t offset) {
    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }

    OpenFile *file = find_open_file(fs, filename);
    if (!file) {
        printf("Error: File is not open\n");
        free(entry);
        return;
    }

    if (offset > entry->DIR_FileSize) {
        printf("Error: Offset is larger than file size\n");
        free(entry);
        return;
    }

    file->offset = offset;
    free(entry);
}

/* read command */
void cmd_read(FileSystem *fs, const char *filename, uint32_t size) {
    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }

    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: Cannot read a directory\n");
        free(entry);
        return;
    }

    OpenFile *file = find_open_file(fs, filename);
    if (!file) {
        printf("Error: File is not open\n");
        free(entry);
        return;
    }

    if (strchr(file->mode, 'r') == NULL) {
        printf("Error: File is not open for reading\n");
        free(entry);
        return;
    }

    /* Calculate actual size to read */
    uint32_t bytes_to_read = size;
    if (file->offset + size > entry->DIR_FileSize) {
        bytes_to_read = entry->DIR_FileSize - file->offset;
    }

    if (bytes_to_read == 0) {
        free(entry);
        return;
    }

    /* Read data */
    uint8_t *buffer = malloc(bytes_to_read);
    uint32_t bytes_read = 0;
    uint32_t current_offset = file->offset;
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;

    if (first_cluster == 0) {
        free(buffer);
        free(entry);
        return;
    }

    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    uint32_t cluster_num = current_offset / bytes_per_cluster;
    uint32_t offset_in_cluster = current_offset % bytes_per_cluster;

    /* Navigate to starting cluster */
    uint32_t current_cluster = first_cluster;
    for (uint32_t i = 0; i < cluster_num && is_valid_cluster(fs, current_cluster);
         i++) {
        current_cluster = get_fat_entry(fs, current_cluster);
    }

    /* Read data cluster by cluster */
    while (bytes_read < bytes_to_read && is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        uint32_t bytes_in_cluster = bytes_per_cluster - offset_in_cluster;
        if (bytes_read + bytes_in_cluster > bytes_to_read) {
            bytes_in_cluster = bytes_to_read - bytes_read;
        }

        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec +
              offset_in_cluster, SEEK_SET);
        fread(buffer + bytes_read, 1, bytes_in_cluster, fs->image);

        bytes_read += bytes_in_cluster;
        offset_in_cluster = 0;
        current_cluster = get_fat_entry(fs, current_cluster);
    }

    /* Print data */
    for (uint32_t i = 0; i < bytes_read; i++) {
        printf("%c", buffer[i]);
    }

    /* Update offset */
    file->offset += bytes_read;

    free(buffer);
    free(entry);
}

/* write command */
void cmd_write(FileSystem *fs, const char *filename, const char *string) {
    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }

    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: Cannot write to a directory\n");
        free(entry);
        return;
    }

    OpenFile *file = find_open_file(fs, filename);
    if (!file) {
        printf("Error: File is not open\n");
        free(entry);
        return;
    }

    if (strchr(file->mode, 'w') == NULL) {
        printf("Error: File is not open for writing\n");
        free(entry);
        return;
    }

    uint32_t string_len = strlen(string);
    uint32_t new_size = file->offset + string_len;
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;

    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;

    /* Allocate first cluster if needed */
    if (first_cluster == 0 && string_len > 0) {
        first_cluster = allocate_cluster(fs);
        if (first_cluster == 0) {
            printf("Error: No free clusters available\n");
            free(entry);
            return;
        }
        entry->DIR_FstClusHI = (first_cluster >> 16) & 0xFFFF;
        entry->DIR_FstClusLO = first_cluster & 0xFFFF;
    }

    /* Calculate clusters needed */
    uint32_t clusters_needed = (new_size + bytes_per_cluster - 1) /
                               bytes_per_cluster;
    uint32_t clusters_allocated = 0;

    /* Count current clusters */
    if (first_cluster != 0) {
        uint32_t temp = first_cluster;
        while (is_valid_cluster(fs, temp)) {
            clusters_allocated++;
            temp = get_fat_entry(fs, temp);
        }
    }

    /* Allocate more clusters if needed */
    if (clusters_needed > clusters_allocated) {
        uint32_t last_cluster = first_cluster;
        if (last_cluster != 0) {
            uint32_t temp = get_fat_entry(fs, last_cluster);
            while (is_valid_cluster(fs, temp)) {
                last_cluster = temp;
                temp = get_fat_entry(fs, last_cluster);
            }
        }

        for (uint32_t i = clusters_allocated; i < clusters_needed; i++) {
            uint32_t new_cluster = allocate_cluster(fs);
            if (new_cluster == 0) {
                printf("Error: No free clusters available\n");
                free(entry);
                return;
            }
            if (last_cluster != 0) {
                set_fat_entry(fs, last_cluster, new_cluster);
            }
            last_cluster = new_cluster;
        }
    }

    /* Write data */
    uint32_t bytes_written = 0;
    uint32_t current_offset = file->offset;
    uint32_t cluster_num = current_offset / bytes_per_cluster;
    uint32_t offset_in_cluster = current_offset % bytes_per_cluster;

    /* Navigate to starting cluster */
    uint32_t current_cluster = first_cluster;
    for (uint32_t i = 0; i < cluster_num && is_valid_cluster(fs, current_cluster);
         i++) {
        current_cluster = get_fat_entry(fs, current_cluster);
    }

    /* Write data cluster by cluster */
    while (bytes_written < string_len && is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        uint32_t bytes_in_cluster = bytes_per_cluster - offset_in_cluster;
        if (bytes_written + bytes_in_cluster > string_len) {
            bytes_in_cluster = string_len - bytes_written;
        }

        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec +
              offset_in_cluster, SEEK_SET);
        fwrite(string + bytes_written, 1, bytes_in_cluster, fs->image);

        bytes_written += bytes_in_cluster;
        offset_in_cluster = 0;
        current_cluster = get_fat_entry(fs, current_cluster);
    }

    fflush(fs->image);

    /* Update file size if needed */
    if (new_size > entry->DIR_FileSize) {
        entry->DIR_FileSize = new_size;
        file->size = new_size;

        /* Update directory entry */
        char formatted_name[12];
        format_filename(filename, formatted_name);

        int num_entries;
        DirEntry *entries = read_directory(fs, fs->current_cluster, &num_entries);
        for (int i = 0; i < num_entries; i++) {
            if (memcmp(entries[i].DIR_Name, formatted_name, 11) == 0) {
                entries[i].DIR_FileSize = new_size;
                entries[i].DIR_FstClusHI = (first_cluster >> 16) & 0xFFFF;
                entries[i].DIR_FstClusLO = first_cluster & 0xFFFF;
                write_directory_entry(fs, fs->current_cluster, &entries[i], i);
                break;
            }
        }
        free(entries);
    }

    /* Update offset */
    file->offset += string_len;

    free(entry);
}

/* mv command */
void cmd_mv(FileSystem *fs, const char *source, const char *dest) {
    /* Check if source exists */
    DirEntry *src_entry = find_entry(fs, fs->current_cluster, source);
    if (!src_entry) {
        printf("Error: Source does not exist\n");
        return;
    }

    /* Check if file is open */
    if (!(src_entry->DIR_Attr & ATTR_DIRECTORY)) {
        if (find_open_file(fs, source)) {
            printf("Error: File must be closed\n");
            free(src_entry);
            return;
        }
    }

    /* Check if destination exists */
    DirEntry *dest_entry = find_entry(fs, fs->current_cluster, dest);

    if (dest_entry) {
        /* Destination exists - check if it's a directory */
        if (!(dest_entry->DIR_Attr & ATTR_DIRECTORY)) {
            printf("Error: Destination is a file\n");
            free(src_entry);
            free(dest_entry);
            return;
        }

        /* Move into directory */
        uint32_t dest_cluster = ((uint32_t)dest_entry->DIR_FstClusHI << 16) |
                                dest_entry->DIR_FstClusLO;

        /* Check if already exists in destination */
        DirEntry *existing = find_entry(fs, dest_cluster, source);
        if (existing) {
            printf("Error: File already exists in destination\n");
            free(src_entry);
            free(dest_entry);
            free(existing);
            return;
        }

        /* Create entry in destination */
        uint32_t src_cluster = ((uint32_t)src_entry->DIR_FstClusHI << 16) |
                               src_entry->DIR_FstClusLO;
        if (create_directory_entry(fs, dest_cluster, source,
                                   src_entry->DIR_Attr, src_cluster,
                                   src_entry->DIR_FileSize) < 0) {
            printf("Error: Failed to create entry in destination\n");
            free(src_entry);
            free(dest_entry);
            return;
        }

        free(dest_entry);
    } else {
        /* Simple rename */
        char formatted_dest[12];
        format_filename(dest, formatted_dest);

        char formatted_src[12];
        format_filename(source, formatted_src);

        int num_entries;
        DirEntry *entries = read_directory(fs, fs->current_cluster, &num_entries);
        for (int i = 0; i < num_entries; i++) {
            if (memcmp(entries[i].DIR_Name, formatted_src, 11) == 0) {
                memcpy(entries[i].DIR_Name, formatted_dest, 11);
                write_directory_entry(fs, fs->current_cluster, &entries[i], i);
                break;
            }
        }
        free(entries);
        free(src_entry);
        return;
    }

    /* Delete from source */
    delete_directory_entry(fs, fs->current_cluster, source);
    free(src_entry);
}

/* rm command */
void cmd_rm(FileSystem *fs, const char *filename) {
    /* Check if file exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }

    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: Cannot remove a directory\n");
        free(entry);
        return;
    }

    /* Check if file is open */
    if (find_open_file(fs, filename)) {
        printf("Error: File is open\n");
        free(entry);
        return;
    }

    /* Free cluster chain */
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;
    if (first_cluster != 0) {
        free_cluster_chain(fs, first_cluster);
    }

    /* Delete directory entry */
    delete_directory_entry(fs, fs->current_cluster, filename);

    free(entry);
}

/* rmdir command */
void cmd_rmdir(FileSystem *fs, const char *dirname) {
    /* Check if directory exists */
    DirEntry *entry = find_entry(fs, fs->current_cluster, dirname);
    if (!entry) {
        printf("Error: Directory does not exist\n");
        return;
    }

    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
        printf("Error: Not a directory\n");
        free(entry);
        return;
    }

    uint32_t dir_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                           entry->DIR_FstClusLO;

    /* Check if directory is empty */
    if (!is_directory_empty(fs, dir_cluster)) {
        printf("Error: Directory is not empty\n");
        free(entry);
        return;
    }

    /* Check if any files are open in this directory */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fs->open_files[i].is_open) {
            /* Build expected path */
            char expected_path[MAX_PATH_LENGTH + 20];
            if (strcmp(fs->current_path, "/") == 0) {
                snprintf(expected_path, sizeof(expected_path), "/%s", dirname);
            } else {
                snprintf(expected_path, sizeof(expected_path), "%s/%s",
                         fs->current_path, dirname);
            }
            if (strncmp(fs->open_files[i].path, expected_path,
                       strlen(expected_path)) == 0) {
                printf("Error: A file is open in this directory\n");
                free(entry);
                return;
            }
        }
    }

    /* Free cluster chain */
    if (dir_cluster != 0) {
        free_cluster_chain(fs, dir_cluster);
    }

    /* Delete directory entry */
    delete_directory_entry(fs, fs->current_cluster, dirname);

    free(entry);
}
