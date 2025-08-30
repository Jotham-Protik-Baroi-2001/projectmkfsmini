# MiniVSFS File System Implementation

This project implements a simplified inode-based file system called MiniVSFS, based on VSFS. The implementation consists of two main programs:

1. **`mkfs_builder`** - Creates a raw disk image for the MiniVSFS file system
2. **`mkfs_adder`** - Adds files to an existing MiniVSFS file system image

## Project Overview

MiniVSFS is a block-based file system with the following characteristics:
- **Block Size**: 4096 bytes
- **Inode Size**: 128 bytes
- **Supported Directories**: Only root (/) directory
- **Bitmap Blocks**: One block each for inode and data bitmaps
- **Direct Blocks**: 12 direct block pointers per inode (no indirect pointers)
- **Endianness**: Little-endian format

## File System Layout

The disk image is organized as follows:
```
Block 0: Superblock (116 bytes + padding to 4096 bytes)
Block 1: Inode Bitmap (4096 bytes)
Block 2: Data Bitmap (4096 bytes)
Block 3+: Inode Table (128 bytes × number of inodes)
Data Region: Data blocks for files and directories
```

## Building the Programs

### Prerequisites
- GCC compiler with C17 support
- Standard C libraries (stdio, stdint, string, etc.)

### Build Commands

```bash
# Build mkfs_builder
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder

# Build mkfs_adder
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder

# Build both programs at once
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder && \
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
```

## Usage

### mkfs_builder

Creates a new MiniVSFS file system image.

```bash
./mkfs_builder --image <output.img> --size-kib <180..4096> --inodes <128..512>
```

**Parameters:**
- `--image`: Output image filename (e.g., `filesystem.img`)
- `--size-kib`: Total size in KiB (must be between 180 and 4096, multiple of 4)
- `--inodes`: Number of inodes (must be between 128 and 512)

**Examples:**
```bash
# Create a 1024 KiB file system with 256 inodes
./mkfs_builder --image test.img --size-kib 1024 --inodes 256

# Create a 2048 KiB file system with 512 inodes
./mkfs_builder --image large.img --size-kib 2048 --inodes 512

# Create a minimal 180 KiB file system with 128 inodes
./mkfs_builder --image minimal.img --size-kib 180 --inodes 128
```

### mkfs_adder

Adds a file to an existing MiniVSFS file system image.

```bash
./mkfs_adder --input <input.img> --output <output.img> --file <filename>
```

**Parameters:**
- `--input`: Input image filename (existing MiniVSFS image)
- `--output`: Output image filename (updated image with new file)
- `--file`: File to add to the file system (must exist in current directory)

**Examples:**
```bash
# Add a text file to the file system
./mkfs_adder --input test.img --output test_with_file.img --file sample.txt

# Add a binary file to the file system
./mkfs_adder --input test.img --output test_with_binary.img --file program.bin

# Add multiple files (run multiple times)
./mkfs_adder --input test.img --output test_with_file1.img --file file1.txt
./mkfs_adder --input test_with_file1.img --output test_with_both.img --file file2.txt
```

## Testing and Verification

### 1. Create a Test File System

```bash
# Create a test file system
./mkfs_builder --image test.img --size-kib 1024 --inodes 256
```

### 2. Create Test Files

```bash
# Create a simple text file
echo "Hello, MiniVSFS!" > testfile.txt

# Create a larger test file
dd if=/dev/urandom of=random.bin bs=1K count=10
```

### 3. Add Files to the File System

```bash
# Add the text file
./mkfs_adder --input test.img --output test_with_file.img --file testfile.txt

# Add the binary file
./mkfs_adder --input test_with_file.img --output final.img --file random.bin
```

### 4. Examine the Image (Optional)

```bash
# View the image in hexadecimal (requires xxd or hexdump)
xxd test.img | head -20

# Or use hexdump
hexdump -C test.img | head -20
```

## Error Handling

Both programs include comprehensive error handling for:
- Invalid command line arguments
- File system size/inode constraints
- File size limitations (max 12 direct blocks = 48KB)
- Missing input files
- Insufficient space in file system
- Invalid MiniVSFS images

## File System Structure Details

### Superblock Fields
- **Magic Number**: `0x4D565346` (MiniVSFS identifier)
- **Version**: 1
- **Block Size**: 4096 bytes
- **Total Blocks**: Calculated from size_kib
- **Inode Count**: Specified by user
- **Layout Information**: Bitmap and table positions
- **Root Inode**: Always 1
- **Timestamps**: Build time in Unix epoch

### Inode Structure
- **Mode**: File (0x8000) or Directory (0x4000)
- **Links**: Reference count
- **Size**: File size in bytes
- **Timestamps**: Access, modification, creation times
- **Direct Blocks**: Array of 12 data block pointers
- **Checksum**: CRC32 of inode data

### Directory Entry Structure
- **Inode Number**: Points to file/directory inode
- **Type**: 1=file, 2=directory
- **Name**: Up to 58 characters
- **Checksum**: XOR of entry data

## Limitations

- **File Size**: Maximum 48KB per file (12 direct blocks × 4KB)
- **Directory Support**: Only root directory supported
- **No Indirect Blocks**: Only direct block pointers implemented
- **Fixed Bitmap Size**: One block each for inode and data bitmaps

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure you have GCC with C17 support
2. **Permission Denied**: Check file permissions and directory access
3. **File Too Large**: Files larger than 48KB cannot be added
4. **No Free Space**: Ensure sufficient inodes and data blocks
5. **Invalid Image**: Verify input image is a valid MiniVSFS format

### Debugging

- Use `--help` or incorrect parameters to see usage information
- Check file sizes before adding to file system
- Verify input image integrity before modification
- Use hexdump/xxd to examine image structure

## Project Files

- `mkfs_builder.c` - File system creation program
- `mkfs_adder.c` - File addition program
- `README.md` - This documentation file

## Dependencies

- Standard C library
- `getopt.h` for command line parsing
- `time.h` for timestamp generation
- `stdint.h` for fixed-width integer types

## Notes

- All on-disk structures use little-endian format
- Inodes are 1-indexed (root inode is 1, not 0)
- Checksums are automatically calculated and verified
- Timestamps are set to current time when creating/modifying
- The root directory automatically contains "." and ".." entries
