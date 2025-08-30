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

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

typedef struct __attribute__((packed)) {
    // CREATE YOUR SUPERBLOCK HERE
    // ADD ALL FIELDS AS PROVIDED BY THE SPECIFICATION
    uint32_t magic;              // 0x4D565346
    uint32_t version;            // 1
    uint32_t block_size;         // 4096
    uint64_t total_blocks;       // calculated from size_kib
    uint64_t inode_count;        // from --inodes parameter
    uint64_t inode_bitmap_start; // block 1
    uint64_t inode_bitmap_blocks; // 1
    uint64_t data_bitmap_start;   // block 2
    uint64_t data_bitmap_blocks;  // 1
    uint64_t inode_table_start;   // block 3
    uint64_t inode_table_blocks;  // calculated
    uint64_t data_region_start;   // calculated
    uint64_t data_region_blocks;  // calculated
    uint64_t root_inode;          // 1
    uint64_t mtime_epoch;         // build time
    uint32_t flags;               // 0
    // Removed reserved field to reduce size by 4 bytes
    
    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

typedef struct __attribute__((packed)) {
    // CREATE YOUR INODE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint16_t mode;                // file/directory mode
    uint16_t links;               // number of links
    uint32_t uid;                 // user ID
    uint32_t gid;                 // group ID
    uint64_t size_bytes;          // file size in bytes
    uint64_t atime;               // access time
    uint64_t mtime;               // modification time
    uint64_t ctime;               // creation time
    uint32_t direct[12];          // direct block pointers
    uint32_t reserved_0;          // reserved
    uint32_t reserved_1;          // reserved
    // Removed reserved_2 to reduce size by 4 bytes
    uint32_t proj_id;             // project ID
    uint32_t uid16_gid16;         // additional user/group info
    uint64_t xattr_ptr;           // extended attributes pointer
    uint32_t padding;             // padding to make total size 128 bytes

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0

} inode_t;
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

typedef struct __attribute__((packed)) {
    // CREATE YOUR DIRECTORY ENTRY STRUCTURE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint32_t inode_no;            // inode number (0 if free)
    uint8_t type;                 // 1=file, 2=dir
    char name[58];                // filename
    uint8_t checksum;             // XOR of bytes 0..62
} dirent64_t;
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

// Function prototypes
void print_usage(const char *program_name);
int parse_arguments(int argc, char *argv[], char **input_name, char **output_name, char **file_name);
int add_file_to_fs(const char *input_name, const char *output_name, const char *file_name);
int find_free_inode(FILE *fp, superblock_t *sb);
int find_free_data_block(FILE *fp, superblock_t *sb);
void update_bitmaps(FILE *fp, superblock_t *sb, int inode_num, int data_block);
void update_inode_table(FILE *fp, superblock_t *sb, int inode_num, const char *file_name, int data_block, size_t file_size);
void update_root_directory(FILE *fp, superblock_t *sb, const char *file_name, int inode_num);
void write_file_data(FILE *fp, superblock_t *sb, int data_block, const char *file_name);

// ==========================DO NOT CHANGE THIS PORTION=========================
// These functions are there for your help. You should refer to the specifications to see how you can use them.
// ====================================CRC32====================================
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
// ====================================CRC32====================================

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    // zero crc area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c; // low 4 bytes carry the crc
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];   // covers ino(4) + type(1) + name(58)
    de->checksum = x;
}

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --input <input.img> --output <output.img> --file <filename>\n", program_name);
    fprintf(stderr, "  --input     : input image filename\n");
    fprintf(stderr, "  --output    : output image filename\n");
    fprintf(stderr, "  --file      : file to add to the file system\n");
}

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

int add_file_to_fs(const char *input_name, const char *output_name, const char *file_name) {
    // Check if the file to add exists
    FILE *file_fp = fopen(file_name, "rb");
    if (!file_fp) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", file_name, strerror(errno));
        return -1;
    }
    
    // Get file size
    fseek(file_fp, 0, SEEK_END);
    size_t file_size = ftell(file_fp);
    fseek(file_fp, 0, SEEK_SET);
    
    // Check if file is too large for direct blocks
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
    
    // Verify magic number
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
    
    // Update bitmaps
    update_bitmaps(output_fp, &sb, inode_num, data_block);
    
    // Update inode table
    update_inode_table(output_fp, &sb, inode_num, file_name, data_block, file_size);
    
    // Update root directory
    update_root_directory(output_fp, &sb, file_name, inode_num);
    
    // Write file data
    write_file_data(output_fp, &sb, data_block, file_name);
    
    fclose(input_fp);
    fclose(output_fp);
    
    printf("File '%s' added successfully to '%s'\n", file_name, output_name);
    return 0;
}

int find_free_inode(FILE *fp, superblock_t *sb) {
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    
    uint8_t bitmap[BS];
    if (fread(bitmap, BS, 1, fp) != 1) {
        return -1;
    }
    
    // Find first free inode (bit = 0)
    for (int i = 0; i < (int)sb->inode_count; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            return i + 1; // 1-indexed
        }
    }
    
    return -1;
}

int find_free_data_block(FILE *fp, superblock_t *sb) {
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    
    uint8_t bitmap[BS];
    if (fread(bitmap, BS, 1, fp) != 1) {
        return -1;
    }
    
    // Find first free data block (bit = 0)
    for (int i = 0; i < (int)sb->data_region_blocks; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (!(bitmap[byte_idx] & (1 << bit_idx))) {
            return i;
        }
    }
    
    return -1;
}

void update_bitmaps(FILE *fp, superblock_t *sb, int inode_num, int data_block) {
    // Update inode bitmap
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    uint8_t inode_bitmap[BS];
    fread(inode_bitmap, BS, 1, fp);
    
    int inode_bit_idx = inode_num - 1; // Convert to 0-indexed
    int byte_idx = inode_bit_idx / 8;
    int bit_idx = inode_bit_idx % 8;
    inode_bitmap[byte_idx] |= (1 << bit_idx);
    
    fseek(fp, sb->inode_bitmap_start * BS, SEEK_SET);
    fwrite(inode_bitmap, BS, 1, fp);
    
    // Update data bitmap
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    uint8_t data_bitmap[BS];
    fread(data_bitmap, BS, 1, fp);
    
    int byte_idx_data = data_block / 8;
    int bit_idx_data = data_block % 8;
    data_bitmap[byte_idx_data] |= (1 << bit_idx_data);
    
    fseek(fp, sb->data_bitmap_start * BS, SEEK_SET);
    fwrite(data_bitmap, BS, 1, fp);
}

void update_inode_table(FILE *fp, superblock_t *sb, int inode_num, const char *file_name, int data_block, size_t file_size) {
    (void)file_name;  // Suppress unused parameter warning
    
    // Calculate inode position
    uint64_t inode_offset = sb->inode_table_start * BS + (inode_num - 1) * INODE_SIZE;
    
    // Create new inode
    inode_t new_inode = {0};
    new_inode.mode = 0x8000;  // File mode (0100000)8
    new_inode.links = 1;      // Only root directory points to it
    new_inode.uid = 0;
    new_inode.gid = 0;
    new_inode.size_bytes = file_size;
    new_inode.atime = time(NULL);
    new_inode.mtime = time(NULL);
    new_inode.ctime = time(NULL);
    new_inode.direct[0] = data_block;  // First data block
    new_inode.proj_id = 9;
    
    // Calculate checksum
    inode_crc_finalize(&new_inode);
    
    // Write inode
    fseek(fp, inode_offset, SEEK_SET);
    fwrite(&new_inode, sizeof(new_inode), 1, fp);
    
    // Update root inode link count
    fseek(fp, sb->inode_table_start * BS, SEEK_SET);
    inode_t root_inode;
    fread(&root_inode, sizeof(root_inode), 1, fp);
    root_inode.links++;  // Increment link count
    inode_crc_finalize(&root_inode);
    
    fseek(fp, sb->inode_table_start * BS, SEEK_SET);
    fwrite(&root_inode, sizeof(root_inode), 1, fp);
}

void update_root_directory(FILE *fp, superblock_t *sb, const char *file_name, int inode_num) {
    // Read root directory block
    fseek(fp, sb->data_region_start * BS, SEEK_SET);
    uint8_t root_block[BS];
    fread(root_block, BS, 1, fp);
    
    // Find first free directory entry
    dirent64_t *entries = (dirent64_t *)root_block;
    int entry_idx = 0;
    while (entry_idx < BS / sizeof(dirent64_t) && entries[entry_idx].inode_no != 0) {
        entry_idx++;
    }
    
    if (entry_idx >= BS / sizeof(dirent64_t)) {
        fprintf(stderr, "Error: no free directory entries in root\n");
        return;
    }
    
    // Create new directory entry
    dirent64_t new_entry = {0};
    new_entry.inode_no = inode_num;
    new_entry.type = 1;  // File
    strncpy(new_entry.name, file_name, 57);
    new_entry.name[57] = '\0';  // Ensure null termination
    dirent_checksum_finalize(&new_entry);
    
    // Write entry
    entries[entry_idx] = new_entry;
    
    // Write back to disk
    fseek(fp, sb->data_region_start * BS, SEEK_SET);
    fwrite(root_block, BS, 1, fp);
}

void write_file_data(FILE *fp, superblock_t *sb, int data_block, const char *file_name) {
    // Open source file
    FILE *file_fp = fopen(file_name, "rb");
    if (!file_fp) {
        fprintf(stderr, "Error: cannot open source file '%s'\n", file_name);
        return;
    }
    
    // Calculate data block position
    uint64_t data_offset = sb->data_region_start * BS + data_block * BS;
    
    // Write file data
    fseek(fp, data_offset, SEEK_SET);
    uint8_t buffer[BS];
    size_t bytes_read = fread(buffer, 1, BS, file_fp);
    fwrite(buffer, 1, bytes_read, fp);
    
    // Pad with zeros if necessary
    if (bytes_read < BS) {
        uint8_t padding[BS - bytes_read];
        memset(padding, 0, BS - bytes_read);
        fwrite(padding, 1, BS - bytes_read, fp);
    }
    
    fclose(file_fp);
}

int main(int argc, char *argv[]) {
    crc32_init();
    
    char *input_name = NULL, *output_name = NULL, *file_name = NULL;
    
    // Parse command line arguments
    if (parse_arguments(argc, argv, &input_name, &output_name, &file_name) != 0) {
        return 1;
    }
    
    // Add file to file system
    if (add_file_to_fs(input_name, output_name, file_name) != 0) {
        return 1;
    }
    
    return 0;
}
