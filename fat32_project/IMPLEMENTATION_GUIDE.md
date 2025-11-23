# FAT32 File System Project - Complete Implementation Guide

## Project Overview

You're building a shell-like utility that can read and manipulate FAT32 file system images. Think of it like implementing your own version of commands like `cd`, `ls`, `mkdir`, etc., but they work on a FAT32 image file instead of your actual file system.

## Prerequisites & Setup

### 1. Understanding FAT32 Basics

Before coding, understand these key concepts:

**Boot Sector**: The first sector (512 bytes) containing metadata about the file system
- Bytes per sector (usually 512)
- Sectors per cluster (how sectors group together)
- Location of FAT tables
- Location of root directory

**FAT (File Allocation Table)**: A lookup table that tracks which clusters belong to which files
- Each entry is 4 bytes (32 bits)
- Entry value points to next cluster in chain
- Special values: 0 = free, 0x0FFFFFF8-F = end of chain

**Clusters**: Basic allocation unit (multiple sectors grouped together)
- Files are stored in cluster chains
- FAT tells you which cluster comes next

**Directory Entries**: 32-byte structures describing files/folders
- First 11 bytes: filename (8.3 format - 8 for name, 3 for extension)
- Attributes: directory, file, hidden, etc.
- Starting cluster number
- File size

### 2. Tools You'll Need

**For Development:**
```bash
# GCC compiler (already on linprog)
gcc --version

# Make utility
make --version

# Text editor (vim, nano, or VSCode with SSH)
```

**For Testing:**
```bash
# Create a test FAT32 image (on your system, not linprog)
dd if=/dev/zero of=test.img bs=1M count=10
mkfs.vfat -F 32 test.img

# Mount to add test files
mkdir mnt
sudo mount -o loop test.img mnt
# Add some files/folders
sudo umount mnt

# Examine the image
hexdump -C test.img | less
```

## Step-by-Step Implementation

## PHASE 1: Project Setup (30 minutes)

### Step 1.1: Create Directory Structure

```bash
mkdir -p fat32_project/{src,include,bin}
cd fat32_project
```

### Step 1.2: Create Git Repository

```bash
git init
echo "bin/" > .gitignore
echo "obj/" >> .gitignore
echo "*.o" >> .gitignore
git add .gitignore
git commit -m "Initial commit with gitignore"
```

### Step 1.3: Plan Your Files

You need:
- `include/fat32.h` - Structures and function declarations
- `include/commands.h` - Command function declarations
- `src/fat32.c` - Core FAT32 operations
- `src/commands.c` - Command implementations
- `src/main.c` - Shell interface
- `Makefile` - Build system
- `README.md` - Documentation

---

## PHASE 2: Core FAT32 Implementation (Part 1 - 3 hours)

### Step 2.1: Define FAT32 Structures (include/fat32.h)

```c
// Boot sector structure - MUST be packed to match disk layout
typedef struct __attribute__((packed)) {
    uint8_t  BS_jmpBoot[3];        // Jump instruction
    uint8_t  BS_OEMName[8];        // OEM name
    uint16_t BPB_BytsPerSec;       // Bytes per sector (usually 512)
    uint8_t  BPB_SecPerClus;       // Sectors per cluster
    uint16_t BPB_RsvdSecCnt;       // Reserved sector count
    uint8_t  BPB_NumFATs;          // Number of FATs (usually 2)
    uint16_t BPB_RootEntCnt;       // Root entry count (0 for FAT32)
    uint16_t BPB_TotSec16;         // Total sectors (0 for FAT32)
    uint8_t  BPB_Media;            // Media type
    uint16_t BPB_FATSz16;          // FAT size (0 for FAT32)
    uint16_t BPB_SecPerTrk;        // Sectors per track
    uint16_t BPB_NumHeads;         // Number of heads
    uint32_t BPB_HiddSec;          // Hidden sectors
    uint32_t BPB_TotSec32;         // Total sectors (FAT32)
    uint32_t BPB_FATSz32;          // FAT size in sectors (FAT32)
    uint16_t BPB_ExtFlags;         // Extended flags
    uint16_t BPB_FSVer;            // File system version
    uint32_t BPB_RootClus;         // Root directory cluster
    uint16_t BPB_FSInfo;           // FSInfo sector
    uint16_t BPB_BkBootSec;        // Backup boot sector
    uint8_t  BPB_Reserved[12];     // Reserved
    uint8_t  BS_DrvNum;            // Drive number
    uint8_t  BS_Reserved1;         // Reserved
    uint8_t  BS_BootSig;           // Boot signature
    uint32_t BS_VolID;             // Volume ID
    uint8_t  BS_VolLab[11];        // Volume label
    uint8_t  BS_FilSysType[8];     // File system type
} BootSector;

// Directory entry structure - MUST be packed
typedef struct __attribute__((packed)) {
    uint8_t  DIR_Name[11];         // File name (8.3 format)
    uint8_t  DIR_Attr;             // Attributes
    uint8_t  DIR_NTRes;            // Reserved
    uint8_t  DIR_CrtTimeTenth;     // Creation time (tenths)
    uint16_t DIR_CrtTime;          // Creation time
    uint16_t DIR_CrtDate;          // Creation date
    uint16_t DIR_LstAccDate;       // Last access date
    uint16_t DIR_FstClusHI;        // High word of first cluster
    uint16_t DIR_WrtTime;          // Write time
    uint16_t DIR_WrtDate;          // Write date
    uint16_t DIR_FstClusLO;        // Low word of first cluster
    uint32_t DIR_FileSize;         // File size in bytes
} DirEntry;

// File system state - maintains current state
typedef struct {
    FILE *image;                    // File pointer to image
    BootSector boot_sector;         // Cached boot sector
    uint32_t current_cluster;       // Current directory cluster
    char current_path[256];         // Current path string
    char image_name[256];           // Image filename
    OpenFile open_files[10];        // Array of open files
    uint32_t data_start_sector;     // Where data region starts
    uint32_t fat_start_sector;      // Where FAT starts
    uint32_t root_cluster;          // Root directory cluster
    uint32_t total_clusters;        // Total clusters in data region
} FileSystem;
```

**Why packed structures?** FAT32 is an on-disk format. The structures must match EXACTLY how data is laid out on disk, with no padding bytes added by the compiler.

### Step 2.2: Implement Core Functions (src/fat32.c)

**Function 1: mount_image()**
```c
int mount_image(FileSystem *fs, const char *image_path) {
    // 1. Open the image file in binary read/write mode
    fs->image = fopen(image_path, "r+b");
    if (!fs->image) return -1;
    
    // 2. Read the boot sector (first 512 bytes)
    fseek(fs->image, 0, SEEK_SET);
    fread(&fs->boot_sector, sizeof(BootSector), 1, fs->image);
    
    // 3. Calculate important values
    // FAT starts after reserved sectors
    fs->fat_start_sector = fs->boot_sector.BPB_RsvdSecCnt;
    
    // Data region starts after FATs
    fs->data_start_sector = fs->boot_sector.BPB_RsvdSecCnt + 
                            (fs->boot_sector.BPB_NumFATs * 
                             fs->boot_sector.BPB_FATSz32);
    
    // Root cluster from boot sector
    fs->root_cluster = fs->boot_sector.BPB_RootClus;
    fs->current_cluster = fs->root_cluster;
    
    // Calculate total clusters
    uint32_t total_sectors = fs->boot_sector.BPB_TotSec32;
    uint32_t data_sectors = total_sectors - fs->data_start_sector;
    fs->total_clusters = data_sectors / fs->boot_sector.BPB_SecPerClus;
    
    // 4. Initialize state
    strcpy(fs->current_path, "/");
    // Extract image name from path
    const char *slash = strrchr(image_path, '/');
    strcpy(fs->image_name, slash ? slash + 1 : image_path);
    
    return 0;
}
```

**Function 2: get_fat_entry() - Read FAT table**
```c
uint32_t get_fat_entry(FileSystem *fs, uint32_t cluster) {
    // FAT entry is 4 bytes, at position cluster * 4
    uint32_t fat_offset = cluster * 4;
    
    // Which sector is it in?
    uint32_t fat_sector = fs->fat_start_sector + 
                          (fat_offset / fs->boot_sector.BPB_BytsPerSec);
    
    // Offset within that sector
    uint32_t entry_offset = fat_offset % fs->boot_sector.BPB_BytsPerSec;
    
    // Seek and read
    fseek(fs->image, fat_sector * fs->boot_sector.BPB_BytsPerSec + entry_offset,
          SEEK_SET);
    uint32_t entry;
    fread(&entry, 4, 1, fs->image);
    
    // Only use lower 28 bits (FAT32 convention)
    return entry & 0x0FFFFFFF;
}
```

**Why mask with 0x0FFFFFFF?** FAT32 only uses 28 bits; upper 4 bits are reserved.

**Function 3: set_fat_entry() - Write FAT table**
```c
void set_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t entry_offset = fat_offset % fs->boot_sector.BPB_BytsPerSec;
    
    // Mask value to 28 bits
    value = value & 0x0FFFFFFF;
    
    // IMPORTANT: Write to BOTH FAT copies for redundancy
    for (int i = 0; i < fs->boot_sector.BPB_NumFATs; i++) {
        uint32_t fat_base = fs->fat_start_sector + 
                            (i * fs->boot_sector.BPB_FATSz32);
        
        uint32_t sector = fat_base + (fat_offset / fs->boot_sector.BPB_BytsPerSec);
        
        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec + entry_offset,
              SEEK_SET);
        fwrite(&value, 4, 1, fs->image);
    }
    fflush(fs->image);  // Ensure written to disk
}
```

**Function 4: get_first_sector_of_cluster()**
```c
uint32_t get_first_sector_of_cluster(FileSystem *fs, uint32_t cluster) {
    // Cluster 2 is the first data cluster
    // Formula: first_sector = ((cluster - 2) * sectors_per_cluster) + data_start
    return ((cluster - 2) * fs->boot_sector.BPB_SecPerClus) + 
           fs->data_start_sector;
}
```

**Why cluster - 2?** Clusters are numbered starting at 2. Clusters 0 and 1 are reserved.

**Function 5: read_directory() - Read all entries from a directory**
```c
DirEntry *read_directory(FileSystem *fs, uint32_t cluster, int *num_entries) {
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec * 
                                  fs->boot_sector.BPB_SecPerClus;
    int max_entries = bytes_per_cluster / 32;  // Each entry is 32 bytes
    
    // Allocate space for entries
    DirEntry *entries = malloc(max_entries * sizeof(DirEntry) * 16);
    int entry_count = 0;
    
    // Follow the cluster chain
    uint32_t current_cluster = cluster;
    while (is_valid_cluster(fs, current_cluster)) {
        uint32_t sector = get_first_sector_of_cluster(fs, current_cluster);
        fseek(fs->image, sector * fs->boot_sector.BPB_BytsPerSec, SEEK_SET);
        
        // Read all entries in this cluster
        for (int i = 0; i < max_entries; i++) {
            DirEntry entry;
            fread(&entry, sizeof(DirEntry), 1, fs->image);
            
            // 0x00 = end of directory
            if (entry.DIR_Name[0] == 0x00) {
                *num_entries = entry_count;
                return entries;
            }
            
            // 0xE5 = deleted entry, skip it
            if (entry.DIR_Name[0] == 0xE5) continue;
            
            // Long filename entry, skip it (we don't support long names)
            if (entry.DIR_Attr == 0x0F) continue;
            
            entries[entry_count++] = entry;
        }
        
        // Move to next cluster in chain
        current_cluster = get_fat_entry(fs, current_cluster);
    }
    
    *num_entries = entry_count;
    return entries;
}
```

**Function 6: format_filename() - Convert "test.txt" to "TEST    TXT"**
```c
void format_filename(const char *input, char *output) {
    // Fill with spaces
    memset(output, ' ', 11);
    output[11] = '\0';
    
    // Handle special cases
    if (strcmp(input, ".") == 0) {
        output[0] = '.';
        return;
    }
    if (strcmp(input, "..") == 0) {
        output[0] = '.';
        output[1] = '.';
        return;
    }
    
    // Copy name part (up to 8 chars)
    int i = 0, o = 0;
    while (input[i] && input[i] != '.' && o < 8) {
        output[o++] = toupper(input[i++]);
    }
    
    // Copy extension part (up to 3 chars)
    if (input[i] == '.') {
        i++;
        o = 8;  // Extension starts at position 8
        while (input[i] && o < 11) {
            output[o++] = toupper(input[i++]);
        }
    }
}
```

**Function 7: allocate_cluster() - Find and allocate a free cluster**
```c
uint32_t allocate_cluster(FileSystem *fs) {
    // Search for a free cluster (FAT entry = 0)
    for (uint32_t cluster = 2; cluster < fs->total_clusters + 2; cluster++) {
        uint32_t entry = get_fat_entry(fs, cluster);
        if (entry == 0) {
            // Mark as end of chain
            set_fat_entry(fs, cluster, 0x0FFFFFFF);
            
            // Zero out the cluster
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
    return 0;  // No free clusters
}
```

**Function 8: free_cluster_chain() - Free a chain of clusters**
```c
void free_cluster_chain(FileSystem *fs, uint32_t cluster) {
    while (is_valid_cluster(fs, cluster)) {
        uint32_t next = get_fat_entry(fs, cluster);
        set_fat_entry(fs, cluster, 0);  // Mark as free
        cluster = next;
    }
}
```

---

## PHASE 3: Command Implementation (Parts 2-6 - 4 hours)

### Step 3.1: Info Command (Part 1)

```c
void cmd_info(FileSystem *fs) {
    printf("position of root cluster: %u\n", fs->boot_sector.BPB_RootClus);
    printf("bytes per sector: %u\n", fs->boot_sector.BPB_BytsPerSec);
    printf("sectors per cluster: %u\n", fs->boot_sector.BPB_SecPerClus);
    printf("total # of clusters in data region: %u\n", fs->total_clusters);
    printf("# of entries in one FAT: %u\n", 
           fs->boot_sector.BPB_FATSz32 * fs->boot_sector.BPB_BytsPerSec / 4);
    
    // Get image size
    fseek(fs->image, 0, SEEK_END);
    long size = ftell(fs->image);
    printf("size of image (in bytes): %ld\n", size);
}
```

### Step 3.2: Navigation Commands (Part 2)

**ls command:**
```c
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
```

**cd command:**
```c
void cmd_cd(FileSystem *fs, const char *dirname) {
    if (strcmp(dirname, ".") == 0) return;  // Stay in current
    
    // Find the directory entry
    DirEntry *entry = find_entry(fs, fs->current_cluster, dirname);
    if (!entry) {
        printf("Error: Directory does not exist\n");
        return;
    }
    
    // Check it's actually a directory
    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
        printf("Error: Not a directory\n");
        free(entry);
        return;
    }
    
    // Get cluster number (combine high and low words)
    uint32_t new_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                           entry->DIR_FstClusLO;
    
    // Update path
    if (strcmp(dirname, "..") == 0) {
        // Going up - remove last component from path
        char *last_slash = strrchr(fs->current_path, '/');
        if (last_slash == fs->current_path) {
            strcpy(fs->current_path, "/");
        } else {
            *last_slash = '\0';
        }
        // Root's parent is 0, which we treat as root
        fs->current_cluster = (new_cluster == 0) ? 
                              fs->root_cluster : new_cluster;
    } else {
        // Going down - append to path
        if (strcmp(fs->current_path, "/") != 0) {
            strcat(fs->current_path, "/");
        }
        strcat(fs->current_path, dirname);
        fs->current_cluster = new_cluster;
    }
    
    free(entry);
}
```

### Step 3.3: Create Commands (Part 3)

**mkdir command:**
```c
void cmd_mkdir(FileSystem *fs, const char *dirname) {
    // 1. Check if name already exists
    DirEntry *existing = find_entry(fs, fs->current_cluster, dirname);
    if (existing) {
        printf("Error: Directory/file already exists\n");
        free(existing);
        return;
    }
    
    // 2. Allocate a cluster for the new directory
    uint32_t new_cluster = allocate_cluster(fs);
    if (new_cluster == 0) {
        printf("Error: No free clusters available\n");
        return;
    }
    
    // 3. Create "." entry (points to itself)
    DirEntry dot_entry;
    memset(&dot_entry, 0, sizeof(DirEntry));
    memset(dot_entry.DIR_Name, ' ', 11);
    dot_entry.DIR_Name[0] = '.';
    dot_entry.DIR_Attr = ATTR_DIRECTORY;
    dot_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    dot_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    write_directory_entry(fs, new_cluster, &dot_entry, 0);
    
    // 4. Create ".." entry (points to parent)
    DirEntry dotdot_entry;
    memset(&dotdot_entry, 0, sizeof(DirEntry));
    memset(dotdot_entry.DIR_Name, ' ', 11);
    dotdot_entry.DIR_Name[0] = '.';
    dotdot_entry.DIR_Name[1] = '.';
    dotdot_entry.DIR_Attr = ATTR_DIRECTORY;
    // Root's parent is represented as cluster 0
    uint32_t parent = (fs->current_cluster == fs->root_cluster) ? 
                      0 : fs->current_cluster;
    dotdot_entry.DIR_FstClusHI = (parent >> 16) & 0xFFFF;
    dotdot_entry.DIR_FstClusLO = parent & 0xFFFF;
    write_directory_entry(fs, new_cluster, &dotdot_entry, 1);
    
    // 5. Create entry in parent directory
    if (create_directory_entry(fs, fs->current_cluster, dirname,
                               ATTR_DIRECTORY, new_cluster, 0) < 0) {
        printf("Error: Failed to create directory entry\n");
        free_cluster_chain(fs, new_cluster);
    }
}
```

**creat command:**
```c
void cmd_creat(FileSystem *fs, const char *filename) {
    // Check if already exists
    DirEntry *existing = find_entry(fs, fs->current_cluster, filename);
    if (existing) {
        printf("Error: Directory/file already exists\n");
        free(existing);
        return;
    }
    
    // Create file with no clusters (size 0)
    // First cluster = 0 means no data allocated yet
    if (create_directory_entry(fs, fs->current_cluster, filename,
                               ATTR_ARCHIVE, 0, 0) < 0) {
        printf("Error: Failed to create file entry\n");
    }
}
```

### Step 3.4: Read Commands (Part 4)

**open command:**
```c
void cmd_open(FileSystem *fs, const char *filename, const char *mode) {
    // Validate mode
    if (strcmp(mode, "-r") != 0 && strcmp(mode, "-w") != 0 &&
        strcmp(mode, "-rw") != 0 && strcmp(mode, "-wr") != 0) {
        printf("Error: Invalid mode\n");
        return;
    }
    
    // Check if file exists
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
    
    // Check if already open
    if (find_open_file(fs, filename)) {
        printf("Error: File is already open\n");
        free(entry);
        return;
    }
    
    // Get first cluster
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;
    
    // Add to open files array (remove leading '-' from mode)
    char mode_str[4];
    strncpy(mode_str, mode + 1, 3);
    mode_str[3] = '\0';
    
    if (add_open_file(fs, filename, mode_str, fs->current_path,
                      first_cluster, entry->DIR_FileSize) < 0) {
        printf("Error: Too many open files\n");
    }
    
    free(entry);
}
```

**read command (the complex one):**
```c
void cmd_read(FileSystem *fs, const char *filename, uint32_t size) {
    // 1. Validate file exists and is open for reading
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
    
    if (strchr(file->mode, 'r') == NULL) {
        printf("Error: File is not open for reading\n");
        free(entry);
        return;
    }
    
    // 2. Calculate how much to actually read
    uint32_t bytes_to_read = size;
    if (file->offset + size > entry->DIR_FileSize) {
        bytes_to_read = entry->DIR_FileSize - file->offset;
    }
    
    if (bytes_to_read == 0) {
        free(entry);
        return;
    }
    
    // 3. Allocate buffer
    uint8_t *buffer = malloc(bytes_to_read);
    
    // 4. Navigate to starting cluster based on offset
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    
    uint32_t cluster_num = file->offset / bytes_per_cluster;
    uint32_t offset_in_cluster = file->offset % bytes_per_cluster;
    
    // Follow cluster chain to starting position
    uint32_t current_cluster = first_cluster;
    for (uint32_t i = 0; i < cluster_num; i++) {
        current_cluster = get_fat_entry(fs, current_cluster);
    }
    
    // 5. Read data cluster by cluster
    uint32_t bytes_read = 0;
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
        offset_in_cluster = 0;  // Only first cluster has offset
        current_cluster = get_fat_entry(fs, current_cluster);
    }
    
    // 6. Print data
    for (uint32_t i = 0; i < bytes_read; i++) {
        printf("%c", buffer[i]);
    }
    
    // 7. Update offset
    file->offset += bytes_read;
    
    free(buffer);
    free(entry);
}
```

### Step 3.5: Write Command (Part 5 - Most Complex)

```c
void cmd_write(FileSystem *fs, const char *filename, const char *string) {
    // 1. Validate
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }
    
    OpenFile *file = find_open_file(fs, filename);
    if (!file || strchr(file->mode, 'w') == NULL) {
        printf("Error: File is not open for writing\n");
        free(entry);
        return;
    }
    
    uint32_t string_len = strlen(string);
    uint32_t new_size = file->offset + string_len;
    uint32_t bytes_per_cluster = fs->boot_sector.BPB_BytsPerSec *
                                  fs->boot_sector.BPB_SecPerClus;
    
    // 2. Get/allocate first cluster
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;
    
    if (first_cluster == 0 && string_len > 0) {
        first_cluster = allocate_cluster(fs);
        if (first_cluster == 0) {
            printf("Error: No free clusters available\n");
            free(entry);
            return;
        }
    }
    
    // 3. Calculate clusters needed
    uint32_t clusters_needed = (new_size + bytes_per_cluster - 1) /
                               bytes_per_cluster;
    
    // 4. Count current clusters
    uint32_t clusters_allocated = 0;
    if (first_cluster != 0) {
        uint32_t temp = first_cluster;
        while (is_valid_cluster(fs, temp)) {
            clusters_allocated++;
            temp = get_fat_entry(fs, temp);
        }
    }
    
    // 5. Allocate more clusters if needed
    if (clusters_needed > clusters_allocated) {
        // Find last cluster
        uint32_t last_cluster = first_cluster;
        if (last_cluster != 0) {
            uint32_t temp = get_fat_entry(fs, last_cluster);
            while (is_valid_cluster(fs, temp)) {
                last_cluster = temp;
                temp = get_fat_entry(fs, last_cluster);
            }
        }
        
        // Allocate new clusters
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
    
    // 6. Navigate to starting position
    uint32_t cluster_num = file->offset / bytes_per_cluster;
    uint32_t offset_in_cluster = file->offset % bytes_per_cluster;
    
    uint32_t current_cluster = first_cluster;
    for (uint32_t i = 0; i < cluster_num; i++) {
        current_cluster = get_fat_entry(fs, current_cluster);
    }
    
    // 7. Write data
    uint32_t bytes_written = 0;
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
    
    // 8. Update directory entry if file grew
    if (new_size > entry->DIR_FileSize) {
        entry->DIR_FileSize = new_size;
        file->size = new_size;
        entry->DIR_FstClusHI = (first_cluster >> 16) & 0xFFFF;
        entry->DIR_FstClusLO = first_cluster & 0xFFFF;
        
        // Find and update entry in directory
        // (You'll need to search through directory and write it back)
        // See provided implementation for details
    }
    
    // 9. Update offset
    file->offset += string_len;
    
    free(entry);
}
```

### Step 3.6: Delete Commands (Part 6)

**rm command:**
```c
void cmd_rm(FileSystem *fs, const char *filename) {
    // 1. Find file
    DirEntry *entry = find_entry(fs, fs->current_cluster, filename);
    if (!entry) {
        printf("Error: File does not exist\n");
        return;
    }
    
    // 2. Check it's a file, not directory
    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        printf("Error: Cannot remove a directory\n");
        free(entry);
        return;
    }
    
    // 3. Check not open
    if (find_open_file(fs, filename)) {
        printf("Error: File is open\n");
        free(entry);
        return;
    }
    
    // 4. Free cluster chain
    uint32_t first_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                             entry->DIR_FstClusLO;
    if (first_cluster != 0) {
        free_cluster_chain(fs, first_cluster);
    }
    
    // 5. Mark directory entry as deleted (first byte = 0xE5)
    delete_directory_entry(fs, fs->current_cluster, filename);
    
    free(entry);
}
```

**rmdir command:**
```c
void cmd_rmdir(FileSystem *fs, const char *dirname) {
    // 1. Find directory
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
    
    // 2. Get cluster
    uint32_t dir_cluster = ((uint32_t)entry->DIR_FstClusHI << 16) |
                           entry->DIR_FstClusLO;
    
    // 3. Check if empty (only . and ..)
    if (!is_directory_empty(fs, dir_cluster)) {
        printf("Error: Directory is not empty\n");
        free(entry);
        return;
    }
    
    // 4. Check no files open in it
    // (Check open files array for paths starting with this directory)
    
    // 5. Free clusters
    if (dir_cluster != 0) {
        free_cluster_chain(fs, dir_cluster);
    }
    
    // 6. Delete entry
    delete_directory_entry(fs, fs->current_cluster, dirname);
    
    free(entry);
}
```

---

## PHASE 4: Shell Interface (1 hour)

### Step 4.1: Create Main Loop (src/main.c)

```c
void shell_loop(FileSystem *fs) {
    char input[1024];
    char *args[10];
    
    while (1) {
        // Print prompt
        printf("[%s]%s/>", fs->image_name, fs->current_path);
        fflush(stdout);
        
        // Read input
        if (!fgets(input, 1024, stdin)) break;
        
        // Remove newline
        input[strcspn(input, "\n")] = '\0';
        
        // Parse into arguments (handle quotes for write command)
        int argc = parse_input(input, args);
        if (argc == 0) continue;
        
        // Execute commands
        if (strcmp(args[0], "exit") == 0) {
            break;
        } else if (strcmp(args[0], "info") == 0) {
            cmd_info(fs);
        } else if (strcmp(args[0], "ls") == 0) {
            cmd_ls(fs);
        }
        // ... etc for all commands
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FAT32 image>\n", argv[0]);
        return 1;
    }
    
    FileSystem fs;
    if (mount_image(&fs, argv[1]) < 0) {
        fprintf(stderr, "Error: Cannot open image file\n");
        return 1;
    }
    
    shell_loop(&fs);
    close_image(&fs);
    return 0;
}
```

---

## PHASE 5: Testing (2 hours)

### Step 5.1: Create Test Image

```bash
# Create 10MB image
dd if=/dev/zero of=test.img bs=1M count=10

# Format as FAT32
mkfs.vfat -F 32 test.img

# Mount it
mkdir mnt
sudo mount -o loop test.img mnt

# Add test files
echo "Hello World" > mnt/test.txt
mkdir mnt/dir1
echo "File in directory" > mnt/dir1/file2.txt

# Unmount
sudo umount mnt
```

### Step 5.2: Test Each Command

```bash
./bin/filesys test.img

# Test info
info

# Test ls
ls

# Test cd
cd dir1
ls
cd ..

# Test mkdir
mkdir newdir
ls
cd newdir

# Test creat
creat myfile
ls

# Test open/write/close
open myfile -w
write myfile "Test content"
close myfile

# Test open/read
open myfile -r
read myfile 12
close myfile

# Test rm
rm myfile
ls

# Test rmdir
cd ..
rmdir newdir
ls

# Exit
exit
```

### Step 5.3: Verify with hexdump

```bash
# Check boot sector
hexdump -C test.img | head -20

# Check FAT table
hexdump -C test.img -s 512 | head -20

# Check root directory
hexdump -C test.img -s [data_start_sector * 512] | head -50
```

---

## PHASE 6: Documentation (1 hour)

### Step 6.1: Write README.md

Include:
- Project description
- File listing with descriptions
- How to compile
- How to run
- Example usage
- Implementation details

### Step 6.2: Division of Labor

Create two sections:
- **Before**: What you plan to implement
- **After**: What you actually implemented (with details)

---

## PHASE 7: Submission

### Step 7.1: Final Checklist

```bash
# Verify structure
ls -R

# Should have:
# - src/ (with .c files)
# - include/ (with .h files)
# - Makefile
# - README.md
# - NO bin/ in git
# - NO obj/ in git
# - NO executables in git

# Check .gitignore
cat .gitignore

# Should contain:
# bin/
# obj/
# *.o
```

### Step 7.2: Test Compilation

```bash
# Clean build
make clean
make

# Verify executable location
ls bin/filesys

# Test run
./bin/filesys test.img
```

### Step 7.3: Git Submission

```bash
# Add files
git add src/ include/ Makefile README.md

# Commit
git commit -m "Complete FAT32 implementation"

# Push to GitHub Classroom
git push origin main
```

---

## Extra Credit Opportunities

**Note**: The project description doesn't mention specific extra credit, but here are enhancements that could impress:

1. **Long Filename Support** (Complex)
   - Parse long filename entries (LFN)
   - Display proper long names in ls
   - Handle mixed short/long names

2. **Recursive Operations** (Medium)
   - `ls -R` for recursive listing
   - `rm -r` for recursive delete
   - Path handling with `/` separator

3. **Better Error Messages** (Easy)
   - More descriptive error messages
   - Suggest corrections ("Did you mean...")

4. **File Permissions** (Medium)
   - Check read-only attribute
   - Prevent writing to read-only files

5. **Defragmentation** (Hard)
   - Analyze fragmentation
   - Implement simple defrag

6. **FSInfo Sector** (Medium)
   - Read/update FSInfo sector
   - Track free clusters efficiently

7. **Timestamps** (Easy)
   - Set proper timestamps on create/modify
   - Display timestamps in ls

8. **Tab Completion** (Medium)
   - Implement tab completion for filenames
   - Requires readline library

---

## Common Pitfalls to Avoid

1. **Forgetting to flush writes**
   - Always call `fflush()` after writing

2. **Not updating both FAT copies**
   - FAT32 has 2 FAT tables for redundancy

3. **Incorrect cluster arithmetic**
   - Remember clusters start at 2, not 0

4. **Not handling . and ..**
   - Directories need these special entries

5. **Memory leaks**
   - Free all malloc'd memory
   - Close all file handles

6. **Endianness issues**
   - FAT32 is little-endian
   - x86/x64 is little-endian (lucky!)

7. **Buffer overflows**
   - Check array bounds
   - Use strncpy instead of strcpy

8. **Not masking FAT entries**
   - Only use lower 28 bits of FAT32 entries

---

## Time Estimates

- Phase 1 (Setup): 30 minutes
- Phase 2 (Core FAT32): 3 hours
- Phase 3 (Commands): 4 hours
- Phase 4 (Shell): 1 hour
- Phase 5 (Testing): 2 hours
- Phase 6 (Documentation): 1 hour
- **Total: ~12 hours**

Add buffer time for debugging: **15-18 hours total**

---

## Getting Help

1. **Hexdump is your friend**
   ```bash
   hexdump -C test.img | less
   ```

2. **Mount to verify**
   ```bash
   sudo mount -o loop test.img mnt
   ls -la mnt
   ```

3. **Use debugger**
   ```bash
   gdb ./bin/filesys
   ```

4. **Print debugging**
   ```c
   printf("DEBUG: cluster=%u, offset=%u\n", cluster, offset);
   ```

Good luck! This is a challenging but rewarding project that teaches real file system concepts.
