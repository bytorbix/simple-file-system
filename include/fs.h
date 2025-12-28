    /* File System */

    #ifndef FS_H
    #define FS_H
    

    #include "disk.h"
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <math.h>
    #include <stdio.h> // Added for consistency with required types like ssize_t

    // File System Constants
    #define MAGIC_NUMBER (0xf0f03410)
    #define INODES_PER_BLOCK (128)
    #define POINTERS_PER_INODE (5)
    #define POINTERS_PER_BLOCK (1024)
    #define BITS_PER_WORD (32)



    // File System Structure

    typedef struct SuperBlock SuperBlock;
    struct SuperBlock {
        uint32_t magic_number;  // File system type identifier
        uint32_t blocks;        // Total number of blocks in the file system
        uint32_t inode_blocks;  // Total number of blocks reserved for the Inode Table
        uint32_t inodes;        // Total number of Inode structures
    };

    typedef struct Bitmap Bitmap;
    struct Bitmap {
        uint32_t *bitmap;   // Bitmap array Cache
    };


    typedef struct Inode Inode;
    struct Inode{
        uint32_t valid;                      // Status: 1 if allocated (in use), 0 if free (available).
        uint32_t size;                       // File size in bytes.
        uint32_t direct[POINTERS_PER_INODE];  // Direct block addresses for the file's first data blocks.
        uint32_t indirect;                   // Address of the single indirect block (1024 indirect pointers).
    };

    typedef union Block Block;
    union Block {
        // Block Roles: A single block can only serve ONE of these purposes at a time.
        
        SuperBlock super;                      // File System Metadata: Contains the SuperBlock structure (Block 0).
        Inode inodes[INODES_PER_BLOCK];        // Inode Table Block: Stores an array of 128 Inode structures (metadata for files).
        uint32_t pointers[POINTERS_PER_BLOCK]; // Indirect Block: An array of block numbers used for large file addressing.
        char data[BLOCK_SIZE];                 // Data Block: Raw storage for file content.

    };


    typedef struct FileSystem FileSystem;
    struct FileSystem {
        Disk *disk;             // Instance of the emulated Disk
        uint32_t *bitmap;      // Array of free blocks, (In-Memory Bitmap Cache)
        uint32_t *ibitmap;     // Array of free blocks (In-Memory Inodes Bitmap Cache)
        SuperBlock *meta_data;  // Meta data of the file system
    };


    /* File System Functions Prototypes (Declarations) */

    void fs_debug(FileSystem *fs);
    bool fs_format(Disk *disk);
    bool fs_mount(FileSystem *fs, Disk *disk);
    void fs_unmount(FileSystem *fs);
    ssize_t fs_create(FileSystem *fs);
    bool fs_remove(FileSystem *fs, size_t inode_number);
    ssize_t fs_stat(FileSystem *fs, size_t inode_number);
    ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset);
    ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset);
    size_t* fs_allocate(FileSystem *fs, size_t blocks_to_reserve);
    bool fs_bitmap_to_disk(FileSystem *fs);


    #endif // FS_H