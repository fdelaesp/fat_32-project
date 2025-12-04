# FAT32 File System Utility

**Authors:** Francisco de La Espriella (delaespr), Samuel Marcano, Diego Chang  
**Course:** COP4610 - Operating Systems  
**Institution:** Florida State University  
**Semester:** Fall 2024

## Project Overview

This program provides a comprehensive interface for working with FAT32 file system images without corrupting them. It implements various commands for navigation, file creation, reading, writing, and deletion within a FAT32 image file.

## Features

### Part 1: Mounting
- Mount FAT32 image files
- Parse and display boot sector information
- Safe exit with resource cleanup

### Part 2: Navigation
- Change directories (`cd`)
- List directory contents (`ls`)
- Maintain current working directory state

### Part 3: Create
- Create new directories (`mkdir`)
- Create new files (`creat`)

### Part 4: Read
- Open files with different modes (`open`)
- Close files (`close`)
- List open files (`lsof`)
- Seek to file positions (`lseek`)
- Read file contents (`read`)

### Part 5: Update
- Write data to files (`write`)
- Move/rename files and directories (`mv`)

### Part 6: Delete
- Remove files (`rm`)
- Remove empty directories (`rmdir`)

## File Listing

```
fat32_project/
├── bin/                    # Output directory for executables (created by make)
├── include/               # Header files
│   ├── fat32.h           # FAT32 structures and core function declarations
│   └── commands.h        # Command function declarations
├── src/                   # Source files
│   ├── fat32.c           # FAT32 utility functions implementation
│   ├── commands.c        # Command implementations
│   └── main.c            # Main program and shell interface
├── Makefile              # Build configuration
└── README.md             # This file
```

## How to Compile and Run

### Compilation

To compile the program, simply run:

```bash
make
```

This will create the `filesys` executable in the `bin/` directory.

To clean up build artifacts:

```bash
make clean
```

### Running the Program

To run the program with a FAT32 image:

```bash
./bin/filesys <path_to_fat32_image>
```

Example:
```bash
./bin/filesys fat32.img
```

For our project we have a test image called test.img, so the command would be:
```bash
./bin/filesys test.img
```


## Usage

Once the program is running, you'll see a prompt like:

```
[image_name]//>
```

### Available Commands

#### Information and Navigation
- `info` - Display boot sector information
- `ls` - List current directory contents
- `cd <dirname>` - Change to directory
- `exit` - Exit the program

#### File and Directory Creation
- `mkdir <dirname>` - Create a new directory
- `creat <filename>` - Create a new file (size 0)

#### File Operations
- `open <filename> <mode>` - Open a file
  - Modes: `-r` (read), `-w` (write), `-rw` or `-wr` (read-write)
- `close <filename>` - Close an open file
- `lsof` - List all open files
- `lseek <filename> <offset>` - Set file position
- `read <filename> <size>` - Read bytes from file
- `write <filename> "string"` - Write string to file

#### File and Directory Management
- `mv <source> <dest>` - Move/rename file or directory
- `rm <filename>` - Remove a file
- `rmdir <dirname>` - Remove an empty directory

### Command Examples

```bash
# View file system information
info

# Create a directory
mkdir documents

# Change to the directory
cd documents

# Create a file
creat myfile

# Open file for writing
open myfile -w

# Write to the file
write myfile "Hello, World!"

# Close the file
close myfile

# Open for reading
open myfile -r

# Read contents
read myfile 13

# Close and return to parent
close myfile
cd ..

# Remove the file
rm documents/myfile

# Remove the directory
rmdir documents
```

## Project Structure

The code is organized into modular components:

- **fat32.h/fat32.c**: Core FAT32 file system operations including:
  - Boot sector parsing
  - FAT table manipulation
  - Cluster allocation and deallocation
  - Directory entry management
  
- **commands.h/commands.c**: Implementation of all shell commands:
  - File system navigation
  - File and directory creation
  - File I/O operations
  - File and directory deletion
  
- **main.c**: Shell interface and command parsing

## Implementation Details

### FAT32 Structures

The program uses packed structures to accurately represent FAT32 on-disk formats:
- Boot Sector (BPB)
- Directory Entries
- FAT Table

### Key Features

- **Error Handling**: Comprehensive error checking with descriptive messages
- **Resource Management**: Proper cleanup of allocated memory and file handles
- **State Maintenance**: Tracks current directory and open files
- **Cluster Management**: Efficient allocation and deallocation of disk clusters
- **File Extension**: Automatically extends files when writing beyond current size

### Assumptions and Limitations

- Maximum 10 files can be open simultaneously
- Filenames must be 11 characters or less (8.3 format)
- No support for deep directory paths (no "/" expansion)
- Long directory names are skipped
- File and directory names cannot contain spaces
- No file extensions in names (though handled internally)

## Testing

The program has been tested with various FAT32 images and can be validated using:
- `hexedit` to inspect the image file
- Linux `mount` command with loopback option to verify integrity

## Division of Labor

### Before Implementation

**Team Planning and Task Distribution:**

**Francisco de La Espriella (delaespr):**
- Lead FAT32 core implementation (fat32.c)
- Part 1: Mounting and info command
- Part 2: Navigation commands (cd, ls)
- Project coordination and integration testing

**Samuel Marcano:**
- Lead command implementation Part 4: Read operations
- Part 3: Create commands (mkdir, creat)
- Open file management system
- Testing and validation

**Diego Chang:**
- Lead command implementation Part 5 & 6: Update and Delete operations
- Shell interface and command parsing (main.c)
- Documentation and README
- Error handling standardization

**Estimated Timeline:**
- Week 1: Setup, FAT32 core structures, mounting (All members)
- Week 2: Navigation, creation commands (Francisco & Samuel)
- Week 3: Read operations, file management (Samuel & Diego)
- Week 4: Update/delete operations, testing, documentation (All members)

### After Implementation

**Francisco de La Espriella (delaespr)** - **~21 hours**

**Completed Components:**
- **FAT32 Core Functions** (src/fat32.c) - 7 hours
  - Boot sector reading and validation (`mount_image`, `close_image`)
  - FAT table operations (`get_fat_entry`, `set_fat_entry`)
  - Cluster management (`get_first_sector_of_cluster`, `is_valid_cluster`)
  - Directory reading (`read_directory`, `find_entry`)
  - Filename formatting (`format_filename`, `parse_filename`)
  
- **Navigation Commands** (src/commands.c) - 6 hours
  - `info` command - Boot sector information display
  - `cd` command - Directory navigation with . and .. support
  - `ls` command - Directory listing with proper entry parsing
  - Path management and state tracking
  
- **Integration and Testing** - 4 hours
  - Component integration
  - Cross-function testing
  - Bug fixes in cluster chain traversal
  - Memory leak detection and fixes

- **Project Setup** - 4 hours
  - Makefile configuration
  - Header file organization
  - Initial project structure
  - Git repository setup

**Key Contributions:**
- Designed the FileSystem state structure
- Implemented FAT table dual-write for redundancy
- Created robust directory entry traversal
- Established coding standards for the team

**Samuel Marcano** - **~22 hours**

**Completed Components:**
- **Create Operations** (src/commands.c) - 5 hours
  - `mkdir` command - Directory creation with . and .. entries
  - `creat` command - File creation with validation
  - Directory entry creation helpers
  - Free entry index finding algorithm
  
- **Read Operations** (src/commands.c) - 8 hours
  - `open` command - File opening with mode validation
  - `close` command - File closing and state cleanup
  - `lsof` command - Open file listing with formatted output
  - `lseek` command - File position management
  - `read` command - File reading with cluster chain navigation
  - Open file tracking system (OpenFile structure management)
  
- **Cluster Allocation** (src/fat32.c) - 4 hours
  - `allocate_cluster` function - Free cluster scanning and allocation
  - Cluster zeroing for clean initialization
  - FAT entry updates during allocation
  
- **Testing and Validation** - 5 hours
  - Comprehensive command testing
  - Edge case identification and handling
  - Test script creation
  - Image verification with hexdump

**Key Contributions:**
- Designed open file management system
- Implemented complex read logic with offset handling
- Created comprehensive test cases
- Optimized cluster allocation algorithm

**Diego Chang** - **~21 hours**

**Completed Components:**
- **Update Operations** (src/commands.c) - 7 hours
  - `write` command - File writing with automatic extension (most complex)
    - Cluster chain extension logic
    - Dynamic cluster allocation during write
    - Directory entry size updates
    - Offset management
  - `mv` command - File/directory move and rename operations
  
- **Delete Operations** (src/commands.c) - 5 hours
  - `rm` command - File deletion with cluster freeing
  - `rmdir` command - Directory deletion with empty validation
  - `free_cluster_chain` function in fat32.c
  - `delete_directory_entry` function in fat32.c
  - `is_directory_empty` function in fat32.c
  
- **Shell Interface** (src/main.c) - 5 hours
  - Interactive shell loop
  - Command parsing with quote handling for write command
  - Argument validation and error messages
  - User prompt with path display
  
- **Documentation** - 4 hours
  - Comprehensive README.md
  - Code commenting
  - Implementation guides
  - Usage examples and command reference

**Key Contributions:**
- Implemented the most complex command (write with extension)
- Created user-friendly shell interface
- Standardized error messages across all commands
- Comprehensive project documentation

### Team Collaboration

**Joint Efforts:**
- Weekly team meetings for progress sync and problem-solving
- Peer code reviews before integration
- Collaborative debugging of cluster chain issues
- Shared testing and validation efforts

**Communication:**
- Regular Discord/Slack discussions for quick questions
- Shared Google Doc for tracking bugs and TODOs
- Git for version control and code sharing

**Challenges Overcome as a Team:**
1. **Write command complexity** - Diego led implementation with Samuel providing cluster allocation support
2. **Directory entry management** - Francisco designed structure, all members implemented their commands
3. **Testing coordination** - Samuel created test framework, all members contributed test cases
4. **Integration issues** - Regular team debugging sessions resolved conflicts

**Individual Challenges:**

**Francisco:**
- Understanding FAT32 specification details for boot sector
- Implementing proper FAT table access with sector calculations
- Debugging directory traversal across multiple clusters

**Samuel:**
- Managing open file state consistently across operations
- Implementing read with correct offset and cluster navigation
- Handling edge cases like reading beyond EOF

**Diego:**
- Write command automatic file extension logic
- Calculating clusters needed and linking them correctly
- Parsing quoted strings in shell for write command

**What Worked Well:**
- **Clear division of responsibility** prevented merge conflicts
- **Modular design** allowed parallel development
- **Regular integration** caught issues early
- **Comprehensive testing** by Samuel caught many edge cases
- **Good documentation** by Diego helped everyone understand the full system

**Lessons Learned:**
- File systems require careful attention to data structures
- Team communication is essential for complex projects
- Modular design enables effective collaboration
- Testing early and often saves debugging time later
- Clear documentation helps both team and future users

**Total Team Hours:** ~64 hours combined

## Building in linprog Environment

This program is designed to compile and run on the linprog environment. Ensure you have:
- GCC compiler
- Standard C library
- Make utility

The Makefile is configured to produce warnings and debugging information, making development and testing easier.
