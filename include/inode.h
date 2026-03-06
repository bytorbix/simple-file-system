#pragma once
#include <stdint.h>

// Inode Constants
#define POINTERS_PER_INODE (5)


// Inode Status
#define INODE_FREE 0
#define INODE_FILE 1
#define INODE_DIR 2


// struct

typedef struct Inode Inode;
struct Inode{
    uint32_t valid;                      // Status: 0 Indicates Free/Deleted Slot, 
                                            // 1 Indicatesa an Allocated File Inode 
                                            // 2 Indicates a Directory Inode
    uint32_t size;                       // File size in bytes.
    uint32_t direct[POINTERS_PER_INODE]; // Direct block addresses for the file's first data blocks.
    uint32_t indirect;                   // Address of the single indirect block (1024 indirect pointers).
    uint32_t double_indirect;            // Address of the double indirect block
};