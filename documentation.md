# MiniVSFS File System - Complete Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [What This Project Does](#what-this-project-does)
3. [Command Structures](#command-structures)
4. [File System Architecture](#file-system-architecture)
5. [Data Structures](#data-structures)
6. [mkfs_builder.c - Detailed Code Analysis](#mkfs_builderc---detailed-code-analysis)
7. [mkfs_adder.c - Detailed Code Analysis](#mkfs_adderc---detailed-code-analysis)
8. [Key Concepts Explained](#key-concepts-explained)
9. [Common Interview Questions](#common-interview-questions)

---

## Project Overview

**MiniVSFS** is a simplified, educational file system implementation based on VSFS (Very Simple File System). It's designed to teach the fundamental concepts of how file systems work at the lowest level.

### What is a File System?
A **file system** is like a librarian for your computer's storage. It keeps track of:
- Where files are stored on the disk
- What files exist and their names
- File permissions and ownership
- Directory structure
- Free space available

Think of it as a sophisticated filing cabinet system that knows exactly where every document is stored and how to find it quickly.

---

## What This Project Does

This project creates **two programs** that work together to build and manage a complete file system:

### 1. **mkfs_builder** - The File System Creator
- **Purpose**: Creates a brand new, empty file system from scratch
- **Input**: Takes parameters like size and number of files it can hold
- **Output**: Produces a binary file (`.img`) that represents a complete file system
- **Analogy**: Like building a new library building with empty shelves, catalog system, and filing cabinets

### 2. **mkfs_adder** - The File Adder
- **Purpose**: Takes an existing file system and adds new files to it
- **Input**: An existing file system image + a file to add
- **Output**: A new file system image with the file added
- **Analogy**: Like adding new books to an existing library, updating the catalog, and finding space on the shelves

---

## Command Structures

### mkfs_builder Command
```bash
./mkfs_builder --image <filename> --size-kib <180..4096> --inodes <128..512>
```

**Parameter Breakdown:**
- `--image`: The name of the output file (e.g., `myfilesystem.img`)
- `--size-kib`: Total size in kilobytes (must be 180-4096, multiple of 4)
- `--inodes`: Number of files/directories the system can hold (128-512)

**Example:**
```bash
./mkfs_builder --image test.img --size-kib 1024 --inodes 256
```
This creates a 1MB file system that can hold up to 256 files.

### mkfs_adder Command
```bash
./mkfs_adder --input <input.img> --output <output.img> --file <filename>
```

**Parameter Breakdown:**
- `--input`: The existing file system image to modify
- `--output`: The new file system image with the file added
- `--file`: The file to add (must exist in current directory)

**Example:**
```bash
./mkfs_adder --input test.img --output test_with_file.img --file document.txt
```
This takes `test.img`, adds `document.txt` to it, and saves the result as `test_with_file.img`.

---

## File System Architecture

### Disk Layout (How Data is Organized)
```
Block 0: Superblock (116 bytes + padding to 4096 bytes)
Block 1: Inode Bitmap (4096 bytes)
Block 2: Data Bitmap (4096 bytes)
Block 3+: Inode Table (128 bytes × number of inodes)
Data Region: Data blocks for files and directories
```

**What is a Block?**
- A **block** is a fixed-size chunk of storage (4096 bytes = 4KB)
- Think of it like a page in a book - all pages are the same size
- The file system divides the entire disk into these equal-sized blocks

**Why 4096 bytes?**
- It's a standard size that balances efficiency and waste
- Large enough to store meaningful data
- Small enough to not waste too much space

### Key Components Explained

#### 1. **Superblock** - The Master Directory
- **Purpose**: Contains all the important information about the file system
- **Location**: Always the first block (Block 0)
- **Contains**: File system size, number of inodes, where everything is located
- **Analogy**: Like the front page of a library that tells you how many books it holds, how many shelves, etc.

#### 2. **Inode Bitmap** - The Inode Availability Tracker
- **Purpose**: Tracks which inodes are in use and which are free
- **Location**: Block 1
- **How it works**: Each bit represents one inode (1 = used, 0 = free)
- **Analogy**: Like a seating chart that shows which seats are occupied

#### 3. **Data Bitmap** - The Data Block Availability Tracker
- **Purpose**: Tracks which data blocks are in use and which are free
- **Location**: Block 2
- **How it works**: Each bit represents one data block (1 = used, 0 = free)
- **Analogy**: Like a parking lot map showing which spaces are taken

#### 4. **Inode Table** - The File Information Database
- **Purpose**: Stores detailed information about each file/directory
- **Location**: Blocks 3 and beyond
- **Contains**: File size, permissions, timestamps, where data is stored
- **Analogy**: Like individual file cards in a library catalog

#### 5. **Data Region** - Where Actual File Content Lives
- **Purpose**: Stores the actual content of files
- **Location**: After the inode table
- **How it works**: Each file's data is stored in one or more data blocks
- **Analogy**: Like the actual shelves where books are stored

---

## Data Structures

### 1. Superblock Structure (`superblock_t`)
```c
typedef struct {
    uint32_t magic;              // File system identifier (0x4D565346)
    uint32_t version;            // File system version (1)
    uint32_t block_size;         // Size of each block (4096)
    uint64_t total_blocks;       // Total number of blocks
    uint64_t inode_count;        // Number of inodes
    uint64_t inode_bitmap_start; // Where inode bitmap begins
    uint64_t inode_bitmap_blocks;// How many blocks for inode bitmap
    uint64_t data_bitmap_start;  // Where data bitmap begins
    uint64_t data_bitmap_blocks; // How many blocks for data bitmap
    uint64_t inode_table_start;  // Where inode table begins
    uint64_t inode_table_blocks;// How many blocks for inode table
    uint64_t data_region_start;  // Where data region begins
    uint64_t data_region_blocks; // How many blocks for data region
    uint64_t root_inode;         // Inode number of root directory (1)
    uint64_t mtime_epoch;        // Creation time
    uint32_t flags;              // Reserved flags
    uint32_t checksum;           // Integrity check
} superblock_t;
```

**Field Explanations:**
- **magic**: A unique identifier that proves this is a MiniVSFS file system
- **version**: Allows for future compatibility if the format changes
- **block_size**: Always 4096 bytes in this implementation
- **total_blocks**: Calculated from the size-kib parameter
- **inode_count**: How many files/directories the system can hold
- **root_inode**: Always 1 (inodes are 1-indexed, not 0-indexed)
- **checksum**: A mathematical value that detects corruption

### 2. Inode Structure (`inode_t`)
```c
typedef struct {
    uint16_t mode;               // File type and permissions
    uint16_t links;              // Number of references to this inode
    uint32_t uid;                // User ID (owner)
    uint32_t gid;                // Group ID
    uint64_t size_bytes;         // File size in bytes
    uint64_t atime;              // Last access time
    uint64_t mtime;              // Last modification time
    uint64_t ctime;              // Last change time
    uint32_t direct[12];         // Pointers to data blocks
    uint32_t reserved_0;         // Reserved for future use
    uint32_t reserved_1;         // Reserved for future use
    uint32_t reserved_2;         // Reserved for future use
    uint32_t proj_id;            // Project ID (set to 9)
    uint32_t uid16_gid16;        // Additional user/group info
    uint64_t xattr_ptr;          // Extended attributes pointer
    uint64_t inode_crc;          // Integrity check
} inode_t;
```

**Field Explanations:**
- **mode**: 
  - `0x8000` (32768 in decimal) = regular file
  - `0x4000` (16384 in decimal) = directory
- **links**: How many directory entries point to this file
- **direct[12]**: Array of 12 pointers to data blocks where file content is stored
- **size_bytes**: Actual size of the file content
- **timestamps**: When the file was accessed, modified, or changed

### 3. Directory Entry Structure (`dirent64_t`)
```c
typedef struct {
    uint32_t inode_no;           // Which inode this entry points to
    uint8_t type;                // 1=file, 2=directory
    char name[58];               // File/directory name
    uint8_t checksum;           // Integrity check
} dirent64_t;
```

**Field Explanations:**
- **inode_no**: Points to the inode that contains file information
- **type**: Distinguishes between files and directories
- **name**: The actual name of the file (up to 58 characters)
- **checksum**: Ensures the directory entry hasn't been corrupted

---

## mkfs_builder.c - Detailed Code Analysis

### Header Section and Includes
```c
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
```

**Line-by-Line Explanation:**
- `#define _FILE_OFFSET_BITS 64`: Enables support for large files (>2GB) on 32-bit systems
- `#include <stdio.h>`: Standard input/output functions (printf, fopen, etc.)
- `#include <stdlib.h>`: Standard library functions (malloc, exit, etc.)
- `#include <stdint.h>`: Fixed-width integer types (uint32_t, uint64_t, etc.)
- `#include <string.h>`: String manipulation functions (strcpy, memset, etc.)
- `#include <inttypes.h>`: Format specifiers for fixed-width integers (PRIu64, etc.)
- `#include <errno.h>`: Error handling functions
- `#include <time.h>`: Time functions (time, etc.)
- `#include <assert.h>`: Assertion macros for debugging
- `#include <unistd.h>`: Unix standard functions
- `#include <getopt.h>`: Command-line argument parsing

### Constants and Global Variables
```c
#define BS 4096u               // Block size: 4096 bytes
#define INODE_SIZE 128u        // Inode size: 128 bytes
#define ROOT_INO 1u            // Root inode number: 1
uint64_t g_random_seed = 0;    // Global random seed (unused in this implementation)
```

**Explanation:**
- **BS (Block Size)**: Every block in the file system is exactly 4096 bytes
- **INODE_SIZE**: Each inode structure takes exactly 128 bytes
- **ROOT_INO**: The root directory always has inode number 1 (not 0)

### CRC32 Implementation (Checksum Functions)
```c
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
```

**What is CRC32?**
- **CRC32** stands for "Cyclic Redundancy Check, 32-bit"
- It's a mathematical algorithm that creates a unique "fingerprint" for data
- If data changes, the CRC32 value changes, detecting corruption
- **Analogy**: Like a checksum on a credit card number

**How it works:**
1. Creates a lookup table of 256 pre-calculated values
2. Uses bit manipulation to compute checksums quickly
3. Each byte of data is processed through the table

### Checksum Finalization Functions
```c
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;                                    // Clear checksum field
    uint32_t s = crc32((void *) sb, BS - 4);            // Calculate CRC of superblock
    sb->checksum = s;                                    // Store the result
    return s;
}
```

**Line-by-Line Explanation:**
- `sb->checksum = 0`: Temporarily set checksum to 0 so it doesn't affect the calculation
- `crc32((void *) sb, BS - 4)`: Calculate CRC32 of the superblock, excluding the last 4 bytes (which is the checksum field itself)
- `sb->checksum = s`: Store the calculated checksum in the superblock
- **Why exclude last 4 bytes?**: The checksum field shouldn't be included in its own calculation

### Command Line Parsing
```c
int parse_arguments(int argc, char *argv[], char **image_name, uint32_t *size_kib, uint32_t *inodes) {
    int opt;
    int image_set = 0, size_set = 0, inodes_set = 0;
    
    static struct option long_options[] = {
        {"image", required_argument, 0, 'i'},
        {"size-kib", required_argument, 0, 's'},
        {"inodes", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };
```

**Explanation:**
- **argc**: Number of command-line arguments
- **argv**: Array of command-line argument strings
- **image_set, size_set, inodes_set**: Flags to track which parameters were provided
- **long_options**: Defines the valid command-line options

**The parsing loop:**
```c
while ((opt = getopt_long(argc, argv, "i:s:n:", long_options, NULL)) != -1) {
    switch (opt) {
        case 'i':
            *image_name = optarg;    // Store the image filename
            image_set = 1;          // Mark that image parameter was set
            break;
        case 's':
            *size_kib = atoi(optarg);  // Convert string to integer
            if (*size_kib < 180 || *size_kib > 4096 || (*size_kib % 4 != 0)) {
                fprintf(stderr, "Error: size-kib must be between 180 and 4096 and multiple of 4\n");
                return -1;
            }
            size_set = 1;
            break;
        case 'n':
            *inodes = atoi(optarg);
            if (*inodes < 128 || *inodes > 512) {
                fprintf(stderr, "Error: inodes must be between 128 and 512\n");
                return -1;
            }
            inodes_set = 1;
            break;
        default:
            print_usage(argv[0]);
            return -1;
    }
}
```

**Line-by-Line Explanation:**
- `getopt_long()`: Parses command-line arguments according to the defined options
- `"i:s:n:"`: Short options (i for image, s for size, n for inodes)
- `atoi(optarg)`: Converts the argument string to an integer
- **Validation**: Checks that size is between 180-4096 and multiple of 4
- **Validation**: Checks that inodes is between 128-512

### Main File System Creation Function
```c
void create_file_system(const char *image_name, uint32_t size_kib, uint32_t inodes) {
    FILE *fp = fopen(image_name, "wb");    // Open file for writing in binary mode
    if (!fp) {
        perror("Error opening output file");
        exit(1);
    }
```

**Explanation:**
- `fopen(image_name, "wb")`: Opens a file for writing in binary mode
- `"wb"`: Write mode, binary (not text)
- `perror()`: Prints a descriptive error message if file opening fails

**Calculating File System Layout:**
```c
uint64_t total_blocks = (uint64_t)size_kib * 1024 / 4096;           // Convert KiB to blocks
uint64_t inode_table_blocks = (inodes * INODE_SIZE + BS - 1) / BS; // Calculate blocks needed for inodes
uint64_t data_region_start = 3 + inode_table_blocks;               // Where data region begins
uint64_t data_region_blocks = (total_blocks >= data_region_start) ? (total_blocks - data_region_start) : 0;
```

**Mathematical Explanation:**
- `size_kib * 1024 / 4096`: Converts kilobytes to blocks (1024 bytes per KB, 4096 bytes per block)
- `(inodes * INODE_SIZE + BS - 1) / BS`: Calculates how many blocks are needed for the inode table
  - This is "ceiling division" - rounds up to the next block
  - Example: 256 inodes × 128 bytes = 32768 bytes = 8 blocks exactly
- `data_region_start = 3 + inode_table_blocks`: Data region starts after superblock (1) + bitmaps (2) + inode table

### Superblock Creation
```c
void write_superblock(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    superblock_t sb = {0};    // Initialize all fields to zero
    
    sb.magic = 0x4D565346;    // MiniVSFS magic number
    sb.version = 1;            // Version 1
    sb.block_size = 4096;     // Block size
    sb.total_blocks = (uint64_t)size_kib * 1024 / 4096;
    sb.inode_count = inodes;
    sb.inode_bitmap_start = 1;        // Block 1
    sb.inode_bitmap_blocks = 1;       // Always 1 block
    sb.data_bitmap_start = 2;         // Block 2
    sb.data_bitmap_blocks = 1;        // Always 1 block
    sb.inode_table_start = 3;         // Block 3
    sb.inode_table_blocks = (inodes * INODE_SIZE + 4096 - 1) / 4096;
    sb.data_region_start = 3 + sb.inode_table_blocks;
    sb.data_region_blocks = (sb.total_blocks >= sb.data_region_start) ? (sb.total_blocks - sb.data_region_start) : 0;
    
    sb.flags = 0;                     // No special flags
    sb.root_inode = ROOT_INO;         // Root directory is inode 1
    sb.mtime_epoch = time(NULL);      // Current time
    
    superblock_crc_finalize(&sb);     // Calculate and set checksum
    
    fwrite(&sb, sizeof(sb), 1, fp);   // Write superblock t
        fwrite(&sb, sizeof(sb), 1, fp);   // Write superblock to file
    
    // Pad the rest of the block with zeros
    uint8_t padding[BS - sizeof(sb)];
    memset(padding, 0, sizeof(padding));
    fwrite(padding, sizeof(padding), 1, fp);
}
```

**Line-by-Line Explanation:**
- `superblock_t sb = {0}`: Creates a superblock structure and initializes all fields to zero
- `sb.magic = 0x4D565346`: Sets the magic number that identifies this as a MiniVSFS file system
- `sb.version = 1`: Sets the file system version to 1
- `sb.block_size = 4096`: Sets the block size to 4096 bytes
- `sb.total_blocks = (uint64_t)size_kib * 1024 / 4096`: Calculates total blocks from size in KiB
- `sb.inode_count = inodes`: Sets the number of inodes
- `sb.inode_bitmap_start = 1`: Inode bitmap starts at block 1
- `sb.data_bitmap_start = 2`: Data bitmap starts at block 2
- `sb.inode_table_start = 3`: Inode table starts at block 3
- `sb.root_inode = ROOT_INO`: Root directory is inode 1
- `sb.mtime_epoch = time(NULL)`: Sets creation time to current time
- `superblock_crc_finalize(&sb)`: Calculates and sets the checksum
- `fwrite(&sb, sizeof(sb), 1, fp)`: Writes the superblock to the file
- **Padding**: Fills the rest of the 4096-byte block with zeros

### Bitmap Creation
```c
void write_bitmaps(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    (void)size_kib; (void)inodes;  // Suppress unused parameter warnings
    
    // Create inode bitmap
    uint8_t inode_bitmap[BS]; 
    memset(inode_bitmap, 0, BS);           // Initialize all bits to 0 (free)
    inode_bitmap[0] |= 0x01;               // Set bit 0 to 1 (inode 1 is used for root)
    if (fwrite(inode_bitmap, BS, 1, fp) != 1) { 
        perror("fwrite inode bitmap"); 
        exit(1);
    }
    
    // Create data bitmap
    uint8_t data_bitmap[BS]; 
    memset(data_bitmap, 0, BS);            // Initialize all bits to 0 (free)
    data_bitmap[0] |= 0x01;               // Set bit 0 to 1 (first data block used for root)
    if (fwrite(data_bitmap, BS, 1, fp) != 1) { 
        perror("fwrite data bitmap"); 
        exit(1);
    }
}
```

**Explanation:**
- **Inode Bitmap**: Tracks which inodes are in use
  - `memset(inode_bitmap, 0, BS)`: Sets all bits to 0 (all inodes free)
  - `inode_bitmap[0] |= 0x01`: Sets bit 0 to 1 (inode 1 is used for root directory)
- **Data Bitmap**: Tracks which data blocks are in use
  - `memset(data_bitmap, 0, BS)`: Sets all bits to 0 (all data blocks free)
  - `data_bitmap[0] |= 0x01`: Sets bit 0 to 1 (first data block used for root directory)

### Inode Table Creation
```c
void write_inode_table(FILE *fp, uint32_t inodes) {
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + BS - 1) / BS;
    uint64_t data_region_start = 3 + inode_table_blocks;
    uint64_t total_slots = inode_table_blocks * (BS / INODE_SIZE);
    
    // Create root inode (inode 1)
    inode_t root_inode = {0};
    root_inode.mode = 0040000;              // Directory mode (octal)
    root_inode.links = 2;                  // Root has 2 links (. and ..)
    root_inode.uid = 0;                     // Root user
    root_inode.gid = 0;                     // Root group
    root_inode.size_bytes = 2 * 64;        // Size of . and .. entries
    uint64_t now = time(NULL);
    root_inode.atime = now;                 // Access time
    root_inode.mtime = now;                 // Modification time
    root_inode.ctime = now;                 // Change time
    root_inode.direct[0] = (uint32_t)data_region_start;  // Points to first data block
    for (int i=1;i<12;i++) root_inode.direct[i] = 0;    // Other direct blocks unused
    root_inode.proj_id = 9;                 // Project ID
    root_inode.uid16_gid16 = 0;             // Additional user/group info
    root_inode.xattr_ptr = 0;               // No extended attributes
    
    inode_crc_finalize(&root_inode);        // Calculate checksum
    if (fwrite(&root_inode, sizeof(root_inode), 1, fp) != 1) { 
        perror("fwrite root inode"); 
        exit(1);
    }
    
    // Create empty inodes for the rest
    inode_t empty_inode; 
    memset(&empty_inode, 0, sizeof(empty_inode)); 
    for (uint32_t i = 1; i < inodes; i++) {
        if (fwrite(&empty_inode, sizeof(empty_inode), 1, fp) != 1) { 
            perror("fwrite inode"); 
            exit(1);
        }
    }
    
    // Pad remaining slots in the last block
    for (uint64_t i = inodes; i < total_slots; i++) {
        if (fwrite(&empty_inode, sizeof(empty_inode), 1, fp) != 1) { 
            perror("fwrite inode pad"); 
            exit(1);
        }
    }
}
```

**Line-by-Line Explanation:**
- **Root Inode Creation**:
  - `root_inode.mode = 0040000`: Sets directory mode (octal notation)
  - `root_inode.links = 2`: Root directory has 2 links (. and ..)
  - `root_inode.size_bytes = 2 * 64`: Size is 2 directory entries × 64 bytes each
  - `root_inode.direct[0] = data_region_start`: Points to the first data block
  - `for (int i=1;i<12;i++) root_inode.direct[i] = 0`: Other direct blocks are unused
- **Empty Inodes**: Creates empty inodes for all remaining slots
- **Padding**: Fills any remaining space in the last block with empty inodes

### Root Directory Creation
```c
void write_root_directory(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    (void)size_kib;  // Suppress unused parameter warning
    (void)inodes;    // Suppress unused parameter warning
    
    // Create "." entry (points to self)
    dirent64_t dot_entry = {0};
    dot_entry.inode_no = 1;                // Points to root inode
    dot_entry.type = 2;                    // Directory type
    strcpy(dot_entry.name, ".");           // Name is "."
    dirent_checksum_finalize(&dot_entry);  // Calculate checksum
    
    // Create ".." entry (points to self, since root is top-level)
    dirent64_t dotdot_entry = {0};
    dotdot_entry.inode_no = 1;             // Points to root inode
    dotdot_entry.type = 2;                 // Directory type
    strcpy(dotdot_entry.name, "..");       // Name is ".."
    dirent_checksum_finalize(&dotdot_entry); // Calculate checksum
    
    // Write both entries to the file
    fwrite(&dot_entry, sizeof(dot_entry), 1, fp);
    fwrite(&dotdot_entry, sizeof(dotdot_entry), 1, fp);
    
    // Pad the rest of the block with zeros
    uint8_t padding[BS - 2 * sizeof(dirent64_t)];
    memset(padding, 0, sizeof(padding));
    fwrite(padding, sizeof(padding), 1, fp);
}
```

**Explanation:**
- **"." Entry**: Points to the current directory (root)
- **".." Entry**: Points to the parent directory (also root, since root has no parent)
- **Both entries**: Point to inode 1 (the root inode)
- **Type 2**: Indicates these are directory entries
- **Padding**: Fills the rest of the 4096-byte block with zeros

### Data Blocks Creation
```c
void write_data_blocks(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    uint64_t total_blocks = (uint64_t)size_kib * 1024 / 4096;
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + 4096 - 1) / 4096;
    uint64_t data_region_start = 3 + inode_table_blocks;
    uint64_t data_region_blocks = (total_blocks >= data_region_start) ? (total_blocks - data_region_start) : 0;
    
    if (data_region_blocks == 0) return;  // No data blocks available
    
    // Write empty data blocks
    uint8_t zero_block[BS]; 
    memset(zero_block, 0, BS);
    for (uint64_t i = 1; i < data_region_blocks; i++) {  // Skip first block (used by root)
        if (fwrite(zero_block, BS, 1, fp) != 1) { 
            perror("fwrite data block"); 
            exit(1);
        }
    }
}
```

**Explanation:**
- **Calculates**: How many data blocks are available
- **Skips first block**: The first data block is used by the root directory
- **Writes empty blocks**: All remaining data blocks are filled with zeros
- **Error handling**: Checks if each write operation succeeds

### Main Function
```c
int main(int argc, char *argv[]) {
    crc32_init();                          // Initialize CRC32 lookup table
    
    char *image_name = NULL;
    uint32_t size_kib = 0, inodes = 0;
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &image_name, &size_kib, &inodes) != 0) {
        return 1;
    }
    
    // Create the file system
    create_file_system(image_name, size_kib, inodes);
    
    return 0;
}
```

**Explanation:**
- `crc32_init()`: Initializes the CRC32 lookup table
- `parse_arguments()`: Parses command-line arguments and validates them
- `create_file_system()`: Creates the complete file system
- **Return values**: 0 for success, 1 for error

---

## mkfs_adder.c - Detailed Code Analysis

### Header Section and Includes
```c
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
```

**Explanation:**
- Same includes as mkfs_builder, plus `sys/stat.h` for file statistics
- `sys/stat.h`: Provides file status information functions

### Constants and Data Structures
```c
#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12
```

**Explanation:**
- **DIRECT_MAX**: Maximum number of direct blocks (12)
- This limits file size to 12 × 4096 = 48KB

### Command Line Parsing
```c
int parse_arguments(int argc, char *argv[], char **input_name, char **output_name, char **file_name) {
    int opt;
    int input_set = 0, output_set = 0, file_set = 0;
    
    static struct option long_options[] = {
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"file", required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "i:o:f:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                *input_name = optarg;
                input_set = 1;
                break;
            case 'o':
                *output_name = optarg;
                output_set = 1;
                break;
            case 'f':
                *file_name = optarg;
                file_set = 1;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }
    
    if (!input_set || !output_set || !file_set) {
        fprintf(stderr, "Error: all parameters are required\n");
        print_usage(argv[0]);
        return -1;
    }
    
    return 0;
}
```

**Explanation:**
- **Three required parameters**: input image, output image, file to add
- **Validation**: Ensures all three parameters are provided
- **Error handling**: Shows usage if any parameter is missing

### Main File Addition Function
```c
int add_file_to_fs(const char *input_name, const char *output_name, const char *file_name) {
    // Open the file to be added
    FILE *file_fp = fopen(file_name, "rb");
    if (!file_fp) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", file_name, strerror(errno));
        return -1;
    }
    
    // Get file size
    fseek(file_fp, 0, SEEK_END);
    size_t file_size = ftell(file_fp);
    fseek(file_fp, 0, SEEK_SET);
    
    // Check if file is too large
    if (file_size > DIRECT_MAX * BS) {
        fprintf(stderr, "Warning: file '%s' is too large to fit in %d direct blocks\n", file_name, DIRECT_MAX);
        fclose(file_fp);
        return -1;
    }
    
    fclose(file_fp);
    
    // Open input image
    FILE *input_fp = fopen(input_name, "rb");
    if (!input_fp) {
        fprintf(stderr, "Error: cannot open input image '%s': %s\n", input_name, strerror(errno));
        return -1;
    }
    
    // Read superblock
    superblock_t sb;
    if (fread(&sb, sizeof(sb), 1, input_fp) != 1) {
        fprintf(stderr, "Error: cannot read superblock\n");
        fclose(input_fp);
        return -1;
    }
    
    // Validate magic number
    if (sb.magic != 0x4D565346) {
        fprintf(stderr, "Error: invalid MiniVSFS magic number\n");
        fclose(input_fp);
        return -1;
    }
    
    // Find free inode and data block
    int inode_num = find_free_inode(input_fp, &sb);
    if (inode_num == -1) {
        fprintf(stderr, "Error: no free inodes available\n");
        fclose(input_fp);
        return -1;
    }
    
    int data_block = find_free_data_block(input_fp, &sb);
    if (data_block == -1) {
        fprintf(stderr, "Error: no free data blocks available\n");
        fclose(input_fp);
        return -1;
    }
    
    printf("Adding file '%s' (size: %zu bytes) to inode %d, data block %d\n", 
           file_name, file_size, inode_num, data_block);
    
    // Create output image
    FILE *output_fp = fopen(output_name, "wb");
    if (!output_fp) {
        fprintf(stderr, "Error: cannot create output image '%s': %s\n", output_name, strerror(errno));
        fclose(input_fp);
        return -1;
    }
    
    // Copy input image to output
    fseek(input_fp, 0, SEEK_SET);
    uint8_t buffer[BS];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BS, input_fp)) > 0) {
        fwrite(buffer, 1, bytes_read, output_fp);
    }
    
    // Update file system structures
    update_bitmaps(output_fp, &sb, inode_num, data_block);
    update_inode_table(output_fp, &sb, inode_num, file_name, data_block, file_size);
    update_root_directory(output_fp, &sb, file_name, inode_num);
    write_file_data(output_fp, &sb, data_block, file_name);
    
    // Update superblock timestamp and checksum
    sb.mtime_epoch = (uint64_t)time(NULL);
    superblock_crc_finalize(&sb);
    fseek(output_fp, 0, SEEK_SET);
    if (fwrite(&sb, sizeof(sb), 1, output_fp) != 1) {
        perror("rewrite superblock");
        fclose(input_fp);
        fclose(output_fp);
        return -1;
    }
    
    // Pad superblock if needed
    if (BS > sizeof(sb)) {
        static uint8_t pad[BS];
        memset(pad, 0, BS);
        if (fwrite(pad, BS - sizeof(sb), 1, output_fp) != 1) {
            perror("rewrite superblock pad");
            fclose(input_fp);
            fclose(output_fp);
            return -1;
        }
    }
    
    fclose(input_fp);
    fclose(output_fp);
    
    printf("File '%s' added successfully to '%s'\n", file_name, output_name);
    return 0;
}
```

**Step-by-Step Explanation:**
1. **File Validation**: Opens the file to be added and checks its size
2. **Size Check**: Ensures file fits within 12 direct blocks (48KB limit)
3. **Image Validation**: Opens input image and validates it's a MiniVSFS file system
4. **Resource Allocation**: Finds free inode and data block
5. **Image Copying**: Copies entire input image to output image
6. **Structure Updates**: Updates bitmaps, inode table, and root directory
7. **Data Writing**: Writes the actual file content to the data block
8. **Superblock Update**: Updates timestamp and recalculates checksum

### Finding Free Resources
```c
int find_free_inode(FILE *fp, superblock_t *sb) {
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    
    uint8_t bitmap[BS];
    if (fread(bitmap, BS, 1, fp) != 1) {
        return -1;
    }
    
    // Search for free inode
    for (int i = 0; i < (int)sb->inode_count; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            return i + 1;  // Return 1-indexed inode number
        }
    }
    
    return -1;  // No free inode found
}
```

**Explanation:**
- **Bitmap Reading**: Reads the inode bitmap from the file system
- **Bit Manipulation**: 
  - `byte_idx = i / 8`: Which byte contains the bit
  - `bit_idx = i % 8`: Which bit within that byte
  - `!(bitmap[byte_idx] & (1 << bit_idx))`: Checks if bit is 0 (free)
- **1-Indexing**: Returns `i + 1` because inodes are 1-indexed

```c
int find_free_data_block(FILE *fp, superblock_t *sb) {
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    
    uint8_t bitmap[BS];
    if (fread(bitmap, BS, 1, fp) != 1) {
        return -1;
    }
    
    // Search for free data block
    for (int i = 0; i < (int)sb->data_region_blocks; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            return i;  // Return 0-indexed block number
        }
    }
    
    return -1;  // No free data block found
}
```

**Explanation:**
- **Similar to inode search**: But searches data bitmap instead
- **0-Indexing**: Returns `i` directly because data blocks are 0-indexed
- **Range Check**: Only searches within `data_region_blocks`

### Updating Bitmaps
```c
void update_bitmaps(FILE *fp, superblock_t *sb, int inode_num, int data_block) {
    // Update inode bitmap
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    uint8_t inode_bitmap[BS];
    if (fread(inode_bitmap, BS, 1, fp) != 1) {
        perror("fread inode bitmap");
        return;
    }
    
    int inode_bit_idx = inode_num - 1;  // Convert to 0-indexed
    inode_bitmap[inode_bit_idx / 8] |= (1 << (inode_bit_idx % 8));
    
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    fwrite(inode_bitmap, BS, 1, fp);
    
    // Update data bitmap
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    uint8_t data_bitmap[BS];
    if (fread(data_bitmap, BS, 1, fp) != 1) {
        perror("fread data bitmap");
        return;
    }
    
    data_bitmap[data_block / 8] |= (1 << (data_block % 8));
    
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    fwrite(data_bitmap, BS, 1, fp);
}
```

**Explanation:**
- **Inode Bitmap Update**: Sets the bit for the allocated inode to 1
- **Data Bitmap Update**: Sets the bit for the allocated data block to 1
- **Bit Manipulation**: Uses OR operation to set specific bits
- **File Positioning**: Uses `fseek()` to position at the correct location

### Updating Inode Table
```c
void update_inode_table(FILE *fp, superblock_t *sb, int inode_num, const char *file_name, int data_block, size_t file_size) {
    (void)file_name;  // Suppress unused parameter warning
    
    // Calculate offset for the new inode
    uint64_t inode_offset = sb->inode_table_start * BS + (inode_num - 1) * INODE_SIZE;
    
    // Create new inode
    inode_t new_inode = {0};
    new_inode.mode = 0x8000;   // Regular file mode
    new_inode.links = 1;       // One link (from root directory)
    new_inode.uid = 0;         // Root user
    new_inode.gid = 0;         // Root group
    new_inode.size_bytes = file_size;
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.direct[0] = (uint32_t)(sb->data_region_start + data_block);  // First data block
    new_inode.proj_id = 9;
    
    // Calculate checksum
    inode_crc_finalize(&new_inode);
    
    // Write new inode
    fseek(fp, inode_offset, SEEK_SET);
    fwrite(&new_inode, sizeof(new_inode), 1, fp);
    
    // Update root inode (increment link count)
    fseek(fp, sb->inode_table_start * BS, SEEK_SET);
    inode_t root_inode;
    if (fread(&root_inode, sizeof(root_inode), 1, fp) != 1) {
        perror("fread root inode");
        return;
    }
    
    root_inode.links++;  // Increment link count
    inode_crc_finalize(&root_inode);
    
    fseek(fp, sb->inode_table_start * BS, SEEK_SET);
    fwrite(&root_inode, sizeof(root_inode), 1, fp);
}
```

**Explanation:**
- **New Inode Creation**: Creates an inode for the new file
- **File Mode**: `0x8000` indicates a regular file
- **Link Count**: Set to 1 (referenced by root directory)
- **Direct Block**: Points to the allocated data block
- **Root Inode Update**: Increments the link count of the root directory
- **Checksum**: Recalculates checksum after modification

### Root Directory Update Function
```c
void update_root_directory(FILE *fp, superblock_t *sb, const char *file_name, int inode_num) {
    fseek(fp, sb->data_region_start * BS, SEEK_SET);    // Go to root directory data
    uint8_t root_block[BS];
    if (fread(root_block, BS, 1, fp) != 1) {
        perror("fread root dir block");
        return;
    }
    
    dirent64_t *entries = (dirent64_t *)root_block;     // Cast to directory entries
    size_t entry_idx = 0;
    const size_t max = BS / sizeof(dirent64_t);         // Max entries per block
    while (entry_idx < max && entries[entry_idx].inode_no != 0) {
        entry_idx++;                                    // Find first free entry
    }
    if (entry_idx >= max) {
        fprintf(stderr, "Error: no free directory entries in root\n");
        return;
    }
    
    dirent64_t new_entry = {0};
    new_entry.inode_no = inode_num;                     // Point to new file's inode
    new_entry.type = 1;                                 // 1 = file
    strncpy(new_entry.name, file_name, 57);            // Copy filename
    new_entry.name[57] = '\0';                          // Ensure null termination
    dirent_checksum_finalize(&new_entry);               // Calculate checksum
    
    entries[entry_idx] = new_entry;                     // Store the entry
    
    fseek(fp, sb->data_region_start * BS, SEEK_SET);   // Go back to start
    fwrite(root_block, BS, 1, fp);                      // Write updated block
}
```

**Line-by-Line Explanation:**
- `fseek(fp, sb->data_region_start * BS, SEEK_SET)`: Moves file pointer to the root directory's data block
- `dirent64_t *entries = (dirent64_t *)root_block`: Treats the block as an array of directory entries
- `while (entry_idx < max && entries[entry_idx].inode_no != 0)`: Finds the first empty directory entry (inode_no = 0 means free)
- `strncpy(new_entry.name, file_name, 57)`: Copies filename, ensuring it fits in the 58-byte name field
- `dirent_checksum_finalize(&new_entry)`: Calculates XOR checksum for the directory entry

### File Data Writing Function
```c
void write_file_data(FILE *fp, superblock_t *sb, int data_block, const char *file_name) {
    FILE *file_fp = fopen(file_name, "rb");            // Open source file
    if (!file_fp) {
        fprintf(stderr, "Error: cannot open source file '%s'\n", file_name);
        return;
    }
    
    uint64_t data_offset = sb->data_region_start * BS + data_block * BS;  // Calculate position
    
    fseek(fp, data_offset, SEEK_SET);                   // Go to data block
    uint8_t buffer[BS];
    size_t bytes_read = fread(buffer, 1, BS, file_fp); // Read file content
    fwrite(buffer, 1, bytes_read, fp);                 // Write to filesystem
    
    if (bytes_read < BS) {                             // If file is smaller than block
        uint8_t padding[BS - bytes_read];
        memset(padding, 0, BS - bytes_read);           // Zero out remaining space
        fwrite(padding, 1, BS - bytes_read, fp);       // Write padding
    }
    
    fclose(file_fp);
}
```

**Line-by-Line Explanation:**
- `fopen(file_name, "rb")`: Opens the source file in binary read mode
- `data_offset = sb->data_region_start * BS + data_block * BS`: Calculates exact byte position in the filesystem
- `fread(buffer, 1, BS, file_fp)`: Reads up to one block (4096 bytes) from the source file
- **Padding**: If the file is smaller than 4096 bytes, the remaining space is filled with zeros

---

## Key Concepts Explained

### 1. **Little Endian Format**
**What is it?**
- A way of storing multi-byte numbers in memory
- The least significant byte (smallest part) is stored first
- Example: The number 0x12345678 is stored as: 78 56 34 12

**Why use it?**
- Most modern processors (Intel, AMD) use little-endian
- Ensures compatibility with the host system
- Makes data transfer more efficient

### 2. **1-Indexed Inodes**
**What does this mean?**
- Inodes are numbered starting from 1, not 0
- Root directory is inode 1, not inode 0
- This is different from typical array indexing

**Why 1-indexed?**
- Inode 0 is often reserved for "no inode" or "invalid inode"
- Makes it easier to detect uninitialized inode references
- Common convention in many file systems

### 3. **Bitmap Management**
**How bitmaps work:**
- Each bit represents one resource (inode or data block)
- Bit = 1 means "allocated/in use"
- Bit = 0 means "free/available"

**Bit manipulation example:**
```c
// To set bit 5 in a byte:
byte |= (1 << 5);    // Sets bit 5 to 1

// To check if bit 5 is set:
if (byte & (1 << 5)) {
    // Bit 5 is set
}

// To clear bit 5:
byte &= ~(1 << 5);   // Sets bit 5 to 0
```

### 4. **Checksums and Integrity**
**Why checksums?**
- Detect data corruption
- Ensure data hasn't been accidentally modified
- Provide confidence in data integrity

**Types used in MiniVSFS:**
- **CRC32**: For superblock and inodes (more robust)
- **XOR**: For directory entries (simpler, faster)

### 5. **File Allocation Policy**
**First-Fit Algorithm:**
- Allocates the first available resource found
- Simple and fast
- May lead to fragmentation over time

**Example:**
```
Inode Bitmap: 11110000... (first 4 inodes used)
Looking for free inode: Start from bit 0, find first 0
Result: Inode 5 is allocated
```

---

## Common Interview Questions

### 1. **"Explain how a file system works"**
**Answer Structure:**
- Start with the analogy of a library
- Explain the key components (superblock, inodes, data blocks)
- Describe how files are stored and retrieved
- Mention the role of directories and bitmaps

**Key Points:**
- File systems organize storage into blocks
- Metadata (file info) is separate from data (file content)
- Directories are special files that contain lists of other files
- Bitmaps track which resources are free/used

### 2. **"What happens when you add a file to the file system?"**
**Step-by-Step Process:**
1. **Validation**: Check if file exists and fits in available space
2. **Resource Allocation**: Find free inode and data block using bitmaps
3. **Inode Creation**: Create inode with file metadata
4. **Directory Update**: Add entry to root directory
5. **Data Writing**: Copy file content to allocated data block
6. **Bitmap Update**: Mark inode and data block as used
7. **Checksum Update**: Recalculate checksums for modified structures

### 3. **"Why is the block size 4096 bytes?"**
**Reasons:**
- **Efficiency**: Large enough to minimize overhead
- **Compatibility**: Standard size used by many file systems
- **Memory Management**: Aligns with typical page sizes
- **Balance**: Not too small (wasteful) or too large (inefficient for small files)

### 4. **"What are the limitations of MiniVSFS?"**
**Key Limitations:**
- **File Size**: Maximum 48KB (12 blocks × 4KB)
- **Directory Support**: Only root directory
- **No Indirect Blocks**: Can't handle large files
- **Fixed Bitmap Size**: Limited scalability
- **No Subdirectories**: Flat file structure only

### 5. **"How do you handle file system corruption?"**
**Detection Methods:**
- **Magic Number**: Verify file system type
- **Checksums**: Detect corrupted structures
- **Consistency Checks**: Verify bitmap vs. actual usage

**Recovery Strategies:**
- **Backup**: Always keep copies of important data
- **Validation**: Check integrity before operations
- **Error Handling**: Graceful failure with informative messages

### 6. **"Explain the difference between mkfs_builder and mkfs_adder"**
**mkfs_builder:**
- Creates a new, empty file system
- Sets up all metadata structures
- Initializes bitmaps and inode table
- Creates root directory with "." and ".." entries

**mkfs_adder:**
- Modifies an existing file system
- Adds new files to the root directory
- Updates bitmaps and inode table
- Preserves existing data

### 7. **"What is the purpose of the superblock?"**
**Functions:**
- **Identification**: Magic number identifies file system type
- **Layout Information**: Tells where each component is located
- **Configuration**: Stores file system parameters
- **Integrity**: Contains checksum for corruption detection
- **Metadata**: Creation time, version, flags

### 8. **"How do you calculate file system layout?"**
**Step-by-Step Calculation:**
1. **Total Blocks**: `size_kib * 1024 / 4096`
2. **Inode Table Blocks**: `(inodes * 128 + 4095) / 4096` (ceiling division)
3. **Data Region Start**: `3 + inode_table_blocks` (after superblock + bitmaps)
4. **Data Region Blocks**: `total_blocks - data_region_start`

**Example with 1024 KiB, 256 inodes:**
- Total blocks: 1024 * 1024 / 4096 = 256 blocks
- Inode table: (256 * 128 + 4095) / 4096 = 8 blocks
- Data region start: 3 + 8 = 11
- Data region blocks: 256 - 11 = 245 blocks

### 9. **"What happens if a file is too large?"**
**Current Behavior:**
- Program issues a warning message
- File addition fails gracefully
- No partial file is created
- User must use smaller files or increase file system size

**Why this limitation?**
- MiniVSFS only supports 12 direct blocks
- No indirect block mechanism implemented
- Maximum file size: 12 × 4096 = 49,152 bytes (48KB)

### 10. **"How do you debug file system issues?"**
**Tools and Techniques:**
- **Hexdump/xxd**: Examine raw binary data
- **dd command**: Extract specific blocks for analysis
- **Checksum verification**: Check for corruption
- **Bit manipulation**: Verify bitmap consistency
- **Structure validation**: Ensure proper field values

**Example debugging commands:**
```bash
# View superblock
xxd -g1 -l 32 filesystem.img

# Extract inode bitmap
dd if=filesystem.img bs=4096 skip=1 count=1 2>/dev/null | xxd -g1

# Check root directory
dd if=filesystem.img bs=4096 skip=11 count=1 2>/dev/null | xxd -g1
```

---

## Summary

This MiniVSFS implementation demonstrates the fundamental concepts of file system design:

1. **Block-based Storage**: Data organized in fixed-size blocks
2. **Metadata Management**: Separate storage for file information and content
3. **Resource Tracking**: Bitmaps track free/used resources
4. **Directory Structure**: Special files that contain file listings
5. **Integrity Checking**: Checksums detect corruption
6. **Error Handling**: Graceful failure with informative messages

The project provides hands-on experience with:
- Low-level file I/O operations
- Binary data structures
- Bit manipulation
- Command-line argument parsing
- Error handling and validation
- File system design principles

This knowledge forms the foundation for understanding more complex file systems and storage technologies used in modern operating systems.