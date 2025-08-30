// Test program to check structure sizes
#include <stdio.h>
#include <stdint.h>

#define INODE_SIZE 128u

typedef struct __attribute__((packed)) {
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
    uint32_t reserved;            // padding to make total size 116 bytes
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;

typedef struct __attribute__((packed)) {
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
    uint32_t reserved_2;          // reserved
    uint32_t proj_id;             // project ID
    uint32_t uid16_gid16;         // additional user/group info
    uint64_t xattr_ptr;           // extended attributes pointer
    uint32_t padding;             // padding to make total size 128 bytes
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0
} inode_t;

int main() {
    printf("Superblock size: %zu\n", sizeof(superblock_t));
    printf("Inode size: %zu\n", sizeof(inode_t));
    return 0;
}
