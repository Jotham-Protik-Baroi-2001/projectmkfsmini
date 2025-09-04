// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_minivsfs.c -o mkfs_builder
#define _FILE_OFFSET_BITS 64 //ensures large file support on 32-bit systems
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

#define BS 4096u               // block size
#define INODE_SIZE 128u     // inode size
#define ROOT_INO 1u          // root inode number

uint64_t g_random_seed = 0; // This should be replaced by seed value from the CLI.
                            //if no seed value given then use current time/default as seed

// below contains some basic structures you need for your project
// you are free to create more structures as you require

#pragma pack(push, 1) // ensure no padding Tells the compiler to pack structure members with 1-byte alignment—no padding between fields. This ensures the struct uses the minimum possible space.
typedef struct {
    // CREATE YOUR SUPERBLOCK HERE
    // ADD ALL FIELDS AS PROVIDED BY THE SPECIFICATION

    uint32_t magic;              // 0x4D565346
    uint32_t version;            // 1
    uint32_t block_size;         // 4096
    uint64_t total_blocks;       // calculated from size_kib size_kib*1024/4096
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


    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;
#pragma pack(pop) //Restores the previous packing alignment (undoes the effect of pack(push, 1)).
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block"); // static assert to ensure superblock size is correct

#pragma pack(push,1)
typedef struct {
    // CREATE YOUR INODE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint16_t mode; // file/directory mode
    uint16_t links; // number of links
    uint32_t uid; // user ID
    uint32_t gid; // group ID
    uint64_t size_bytes; // file size in bytes
    uint64_t atime; // access time
    uint64_t mtime; // modification time
    uint64_t ctime; // creation time
    uint32_t direct[12]; // direct block pointers (absolute)
    uint32_t reserved_0; // 0
    uint32_t reserved_1; // 0
    uint32_t reserved_2; // 0 (restored)
    uint32_t proj_id; // project ID
    uint32_t uid16_gid16; // 0
    uint64_t xattr_ptr; // 0  Pointer to extended attributes (xattr), which are extra metadata (like ACLs, user-defined attributes). Set to 0 if not used.
 

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0

} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    // CREATE YOUR DIRECTORY ENTRY STRUCTURE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint32_t inode_no;            // inode number (0 if free)
    uint8_t type;                 // 1=file, 2=dir
    char name[58];                // filename It allows each directory entry to store a filename up to 58 characters long (including the null terminator if needed).
    uint8_t  checksum; // XOR of bytes 0..62
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");


// ==========================DO NOT CHANGE THIS PORTION=========================
// These functions are there for your help. You should refer to the specifications to see how you can use them.
// ====================================CRC32====================================
uint32_t CRC32_TAB[256]; //Declares a table to store 256 precomputed CRC32 values for each possible byte.
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i; //start with the current byte value.
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1); //For each bit in the byte, apply the CRC32 polynomial if the lowest bit is set, otherwise just shift right.
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
} // in summary This code sets up and uses a fast CRC32 checksum algorithm, which is used to detect data corruption in your filesystem structures. 
// ====================================CRC32====================================

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4); //Calculates the CRC32 checksum of the superblock, excluding the last 4 bytes
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


// ============================== Prototypes (kept) ===============================
void print_usage(const char *program_name); 
int parse_arguments(int argc, char *argv[], char **image_name, uint32_t *size_kib, uint32_t *inodes);
void create_file_system(const char *image_name, uint32_t size_kib, uint32_t inodes);
void write_superblock(FILE *fp, uint32_t size_kib, uint32_t inodes);
void write_bitmaps(FILE *fp, uint32_t size_kib, uint32_t inodes);
void write_inode_table(FILE *fp, uint32_t inodes);
void write_root_directory(FILE *fp, uint32_t size_kib, uint32_t inodes);
void write_data_blocks(FILE *fp, uint32_t size_kib, uint32_t inodes);

// ============================== Prototypes (kept) ===============================

//===================function print_usage============================================

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s --image <image> --size-kib <180..4096> --inodes <128..512>\n", program_name);
    fprintf(stderr, " --image : output image filename\n");
    fprintf(stderr, " --size-kib : total size in KiB (multiple of 4)\n");
    fprintf(stderr, " --inodes : number of inodes\n");
}

// ===================================end of function print_usage=========================

// ==================================function parse_arguments===========================
int parse_arguments(int argc, char *argv[], char **image_name, uint32_t *size_kib, uint32_t *inodes) {
    int opt;
    int image_set = 0, size_set = 0, inodes_set = 0;
    
    static struct option long_options[] = {
        {"image", required_argument, 0, 'i'},
        {"size-kib", required_argument, 0, 's'},
        {"inodes", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "i:s:n:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                *image_name = optarg;
                image_set = 1;
                break;
            case 's':
                *size_kib = atoi(optarg);
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
    
    if (!image_set || !size_set || !inodes_set) {
        fprintf(stderr, "Error: all parameters are required\n");
        print_usage(argv[0]);
        return -1;
    }
    
    return 0;
}

// ================================== end function parse_arguments===========================

// ==================================function create_file_system===========================
void create_file_system(const char *image_name, uint32_t size_kib, uint32_t inodes) {
    FILE *fp = fopen(image_name, "wb");
    if (!fp) {
        perror("Error opening output file");
        exit(1);
    }


    // Calculate layout (needed for prints only)
    uint64_t total_blocks = (uint64_t)size_kib * 1024 / 4096;
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + BS - 1) / BS;
    uint64_t data_region_start = 3 + inode_table_blocks; // SB + bitmaps + inode table
    uint64_t data_region_blocks = (total_blocks >= data_region_start) ? (total_blocks - data_region_start) : 0;
    if (data_region_blocks == 0) {
        fprintf(stderr, "Error: no space for data region (image too small)\n");
        fclose(fp);
        exit(1);
    }


    printf("Creating MiniVSFS file system:\n");
    printf(" Total blocks: %" PRIu64 "\n", total_blocks);
    printf(" Inode table blocks: %" PRIu64 "\n", inode_table_blocks);
    printf(" Data region start: %" PRIu64 "\n", data_region_start);
    printf(" Data region blocks: %" PRIu64 "\n", data_region_blocks);


    // Write superblock (block 0)
    write_superblock(fp, size_kib, inodes);


    // Write bitmaps (blocks 1-2)
    write_bitmaps(fp, size_kib, inodes);


    // Write inode table (blocks 3..)
    write_inode_table(fp, inodes);


    // Write root directory block (first data block)
    write_root_directory(fp, size_kib, inodes);


    // Write remaining data blocks (zero-filled)
    write_data_blocks(fp, size_kib, inodes);


    fclose(fp);
    printf("File system created successfully: %s\n", image_name);
}


// ================================== end function create_file_system===========================
// ================================== function write_superblock===========================

void write_superblock(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    superblock_t sb = {0};


    sb.magic = 0x4D565346; // 'MVFS'
    sb.version = 1;
    sb.block_size = 4096;
    sb.total_blocks = (uint64_t)size_kib * 1024 / 4096;
    sb.inode_count = inodes;
    sb.inode_bitmap_start = 1;
    sb.inode_bitmap_blocks = 1;
    sb.data_bitmap_start = 2;
    sb.data_bitmap_blocks = 1;
    sb.inode_table_start = 3;
    sb.inode_table_blocks = (inodes * INODE_SIZE + 4096 - 1) / 4096;
    sb.data_region_start = 3 + sb.inode_table_blocks;
    sb.data_region_blocks = (sb.total_blocks >= sb.data_region_start) ? (sb.total_blocks - sb.data_region_start) : 0;
    if (sb.data_region_blocks == 0) {
        fprintf(stderr, "Error: no space for data region (image too small)\n");
        exit(1);
    }

    sb.flags = 0;
    sb.root_inode = ROOT_INO; // root inode number 1u at the begining
    sb.mtime_epoch = time(NULL);


    superblock_crc_finalize(&sb);


    // Write superblock
    fwrite(&sb, sizeof(sb), 1, fp);
    
    // Pad to block size
    uint8_t padding[BS - sizeof(sb)];
    memset(padding, 0, sizeof(padding));
    fwrite(padding, sizeof(padding), 1, fp);
}
// ================================== end function write_superblock===========================
// ================================== function write_bitmaps===========================



void write_bitmaps(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    (void)size_kib; (void)inodes; // unused here


    // Inode bitmap (block 1): mark inode #1 (bit 0)
    uint8_t inode_bitmap[BS]; memset(inode_bitmap, 0, BS);
    inode_bitmap[0] |= 0x01; // bit 0 => inode 1 allocated
    if (fwrite(inode_bitmap, BS, 1, fp) != 1) { perror("fwrite inode bitmap"); exit(1);}


    // Data bitmap (block 2): mark first data block (bit 0)
    uint8_t data_bitmap[BS]; memset(data_bitmap, 0, BS);
    data_bitmap[0] |= 0x01; // bit 0 => first data block allocated for root dir
    if (fwrite(data_bitmap, BS, 1, fp) != 1) { perror("fwrite data bitmap"); exit(1);}
}

// ================================== end function write_bitmaps===========================

// ================================== function write_inode_table===========================
void write_inode_table(FILE *fp, uint32_t inodes) {
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + BS - 1) / BS;
    uint64_t data_region_start = 3 + inode_table_blocks; // absolute block number of first data block
    uint64_t total_slots = inode_table_blocks * (BS / INODE_SIZE); // blocks * 32


    // Create root inode (#1)
    inode_t root_inode = {0};
    root_inode.mode = 0040000; // Directory mode (octal)
    root_inode.links = 2; // . and .. point to itself
    root_inode.uid = 0;
    root_inode.gid = 0;
    root_inode.size_bytes = 2 * 64; // two directory entries
    uint64_t now = time(NULL);
    root_inode.atime = now;
    root_inode.mtime = now;
    root_inode.ctime = now;
    root_inode.direct[0] = (uint32_t)data_region_start; // (fixed) absolute first data block
    for (int i=1;i<12;i++) root_inode.direct[i] = 0; //12 direct pointers
    root_inode.proj_id = 9; 
    root_inode.uid16_gid16 = 0;
    root_inode.xattr_ptr = 0;


    inode_crc_finalize(&root_inode);


    // Write root inode
    if (fwrite(&root_inode, sizeof(root_inode), 1, fp) != 1) { perror("fwrite root inode"); exit(1);}


    // Write remaining declared inodes (2..inodes) as zero
    inode_t empty_inode; 
    memset(&empty_inode, 0, sizeof(empty_inode));  //It ensures that all unused inode slots in the inode table are initialized to a known, empty state.
    for (uint32_t i = 1; i < inodes; i++) {
        if (fwrite(&empty_inode, sizeof(empty_inode), 1, fp) != 1) { perror("fwrite inode"); exit(1);}
    }


    // (fixed) pad the remainder of the inode table up to full blocks
    for (uint64_t i = inodes; i < total_slots; i++) {
        if (fwrite(&empty_inode, sizeof(empty_inode), 1, fp) != 1) { perror("fwrite inode pad"); exit(1);}
    }
}

// ================================== end function write_inode_table===========================

// ================================== function write_root_directory===========================
void write_root_directory(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    (void)size_kib;  // Suppress unused parameter warning
    (void)inodes;     // Suppress unused parameter warning
    
    // Create directory entries for . and ..
    dirent64_t dot_entry = {0};
    dot_entry.inode_no = 1;  // Root inode
    dot_entry.type = 2;      // Directory
    strcpy(dot_entry.name, ".");
    dirent_checksum_finalize(&dot_entry);
    
    dirent64_t dotdot_entry = {0};
    dotdot_entry.inode_no = 1;  // Root inode
    dotdot_entry.type = 2;      // Directory
    strcpy(dotdot_entry.name, "..");
    dirent_checksum_finalize(&dotdot_entry);
    
    // Write directory entries
    fwrite(&dot_entry, sizeof(dot_entry), 1, fp);
    fwrite(&dotdot_entry, sizeof(dotdot_entry), 1, fp);
    
    // Pad to block size
    uint8_t padding[BS - 2 * sizeof(dirent64_t)];
    memset(padding, 0, sizeof(padding));
    fwrite(padding, sizeof(padding), 1, fp);
}

// ================================== end function write_root_directory===========================

void write_data_blocks(FILE *fp, uint32_t size_kib, uint32_t inodes) {
    // Calculate how many data blocks we need to write
    uint64_t total_blocks = (uint64_t)size_kib * 1024 / 4096;
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + 4096 - 1) / 4096;
    uint64_t data_region_start = 3 + inode_table_blocks;
    uint64_t data_region_blocks = (total_blocks >= data_region_start) ? (total_blocks - data_region_start) : 0;


    if (data_region_blocks == 0) return; // nothing to do (already guarded earlier)


    // We've already written the first data block (root directory). Write remaining as zero.
    uint8_t zero_block[BS]; 
    memset(zero_block, 0, BS);
    for (uint64_t i = 1; i < data_region_blocks; i++) {
        if (fwrite(zero_block, BS, 1, fp) != 1) { perror("fwrite data block"); exit(1);}
    }
}


int main(int argc, char *argv[]) {
    crc32_init();


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


