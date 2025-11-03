/* File System */

#include "disk.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

// File System Constants
#define MAGIC_NUMBER (0xf0f03410)
#define INODES_PER_BLOCK (128)
#define POINTERS_PER_NODE (5)
#define POINTERS_PER_BLOCK (1024)


// File System Structure

typedef struct SuperBlock SuperBlock;
struct SuperBlock {
    uint32_t magic_number;  // File system type identifier
    uint32_t blocks;        // Total number of blocks in the file system
    uint32_t inode_blocks;  // Total number of blocks reserved for the Inode Table
    uint32_t inodes;        // Total number of Inode structures
};


typedef struct Inode Inode;
struct Inode{
    uint32_t valid;                      // Status: 1 if allocated (in use), 0 if free (available).
    uint32_t size;                       // File size in bytes.
    uint32_t direct[POINTERS_PER_NODE];  // Direct block addresses for the file's first data blocks.
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
    bool *free_blocks;      // Amount of available blocks
    SuperBlock *meta_data;  // Meta data of the file system
};

/* File System Functions */

void fs_debug(Disk *disk) {
    // Vaidation Checks
    if (disk == NULL) {
        perror("fs_debug: Error disk is invalid");
        return;
    }
    if (!disk->mounted) { 
        fprintf(stderr, "fs_debug: Error disk is not mounted\n");
        return;
    }

    Block block_buffer;
    SuperBlock superblock;

    // Attempt to copy the content of super block into block_buffer
    if (disk_read(disk, 0, block_buffer.data) < 0) {
        perror("fs_debug: Failed to read SuperBlock from disk");
        return;
    }

    superblock = block_buffer.super; // Copy the initialized superblock into the union

    // super block validation check
    if (superblock.magic_number != MAGIC_NUMBER) {
        fprintf(stderr, 
                "fs_debug: Error Disk magic number (0x%x) is invalid. Expected (0x%x).\n"
                "The disk is either unformatted or corrupted.\n",
                superblock.magic_number, MAGIC_NUMBER);
        return;
    }
    
    // Printing The Super block values (Using %u for consistency)
    printf("SuperBlock:\n");
    printf("\tmagic number is valid\n");
    printf("\t%u blocks\n", superblock.blocks);
    printf("\t%u inode blocks\n", superblock.inode_blocks);
    printf("\t%u inodes\n", superblock.inodes); 

    // --- Scanning Inode Table ---
    Block inode_buffer;
    for (uint32_t i = 1; i <= superblock.inode_blocks; i++) // Iterate over all Inode Blocks (1 to N)
    {
        // Read the current Inode Block
        if (disk_read(disk, i, inode_buffer.data) < 0) {
            perror("fs_debug: Failed to read Inode Block from disk");
            return;
        }

        // Iterate through all Inodes within the current block (0 to 127)
        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            Inode current_node = inode_buffer.inodes[j];
            
            if (current_node.valid) {
                // Calculate and print the global inode index
                uint32_t inode_index = (i - 1) * INODES_PER_BLOCK + j; 
                printf("Inode %u:\n", inode_index);
                printf("\tsize: %u bytes\n", current_node.size);
                
                // Scan and print Direct Pointers
                printf("\tdirect blocks:");
                int count = 0;
                for (uint32_t k = 0; k < POINTERS_PER_NODE; k++) {
                    uint32_t block_num = current_node.direct[k];
                    if (block_num != 0) {
                        printf(" %u", block_num);
                        count++;
                    }
                }
                printf(" (%d total)\n", count); 

                // Scan and print Indirect Pointers
                uint32_t indirect_block_num = current_node.indirect;
                if (indirect_block_num != 0) {
                    printf("\tindirect block: %u\n", indirect_block_num);

                    Block indirect_buffer;
                    // Attempt to read the indirect block from the disk
                    if (disk_read(disk, indirect_block_num, indirect_buffer.data) < 0) {
                        perror("fs_debug: Failed to read indirect block");
                    }
                    else {
                        // read the pointers array inside the indirect block
                        printf("\tindirect pointers:");
                        int indirect_count = 0;

                        for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                            uint32_t data_block_num = indirect_buffer.pointers[k];
                            if (data_block_num != 0) {
                                printf(" %u", data_block_num);
                                indirect_count++;
                            }
                        }
                        printf(" (%d total)\n", indirect_count);
                    }
                }
            }
        }
    }
}

bool fs_format(Disk *disk) 
{
    if (disk == NULL) {
        perror("fs_format: Error disk is invalid");
        return false;
    }
    if (disk->mounted) { 
        fprintf(stderr, "fs_format: Error disk is mounted, aborting to prevent data loss\n");
        return false;
    }

    // Initializing the super block
    SuperBlock superblock;
    superblock.magic_number = MAGIC_NUMBER;
    superblock.blocks = (uint32_t)disk->blocks;

    // Inodes
    double percent_blocks = (double)superblock.blocks * 0.10;   
    superblock.inode_blocks = (uint32_t)ceil(percent_blocks);
    superblock.inodes = superblock.inode_blocks * INODES_PER_BLOCK;
    
    // Capacity check
    if (1 + superblock.inode_blocks > superblock.blocks) {
        fprintf(stderr, "fs_format: Error metadata blocks amount (%u) exceeds disk capacity (%u)\n", 
                1 + superblock.inode_blocks, superblock.blocks);
        return false;
    }

    // Initializing The Block Buffer
    Block block_buffer;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block_buffer.data[i] = 0;
    }

    block_buffer.super = superblock; // Copy the initialized superblock into the union

    // attempt to write the metadata into the magic block
    if (disk_write(disk, 0, block_buffer.data) < 0) {
        perror("fs_format: Failed to write SuperBlock to disk");
        return false;
    }

    // Clean the inode tables
    for (uint32_t i = 1; i <= superblock.inode_blocks; i++) {
        if (disk_write(disk, i, block_buffer.data) < 0) {
            perror("fs_format: Failed to clear inode table blocks");
            return false;
        }
    }
    // success
    return true;
}


bool fs_mount(FileSystem *fs, Disk *disk) {
    if (fs == NULL || disk == NULL) {
        perror("fs_mount: Error fs or disk is invalid (NULL)"); 
        return false;
    }
    if (disk->mounted) { 
        fprintf(stderr, "fs_mount: Error disk is already mounted, cannot mount it\n");
        return false;
    }

    Block block_buffer;
    if (disk_read(disk, 0, block_buffer.data)< 0) {
        perror("fs_mount: Failed to read super block from disk");
        return false;
    }

    SuperBlock superblock = block_buffer.super;
    if (superblock.magic_number != MAGIC_NUMBER) {
        fprintf(stderr, 
                "fs_mount: Error Disk magic number (0x%x) is invalid. Expected (0x%x).\n"
                "The disk is either unformatted or corrupted.\n",
                superblock.magic_number, MAGIC_NUMBER);
        return false;
    }
    if (superblock.blocks != disk->blocks) {
        fprintf(stderr, "fs_mount: Error Super block amount of blocks (%u) mismatch the disk capacity (%zu), aborting...\n",
                superblock.blocks, disk->blocks);
        return false;
    }

    // Allocate memory for the SuperBlock metadata and copy it
    fs->meta_data = (SuperBlock *)malloc(sizeof(SuperBlock));
    if (fs->meta_data == NULL) {
        perror("fs_mount: Failed to allocate memory for SuperBlock metadata");
        return false;
    }

    *(fs->meta_data) = superblock; // Copy the structure contents
    fs->disk = disk;

    uint32_t total_blocks = fs->meta_data->blocks;
    fs->free_blocks = (bool *)calloc(total_blocks, sizeof(bool));

    // Error check if free_blocks is invalid
    if (fs->free_blocks == NULL) {
        perror("fs_mount: Failed to allocate free block bitmap");
        free(fs->meta_data);
        return false;
    }

    // Set the super block (block 0) as allocated
    fs->free_blocks[0] = true;

    // Mark Inode blocks (Blocks 1 To N) as allocated
    uint32_t inode_blocks_end = fs->meta_data->inode_blocks;
    for (uint32_t i = 1; i <= inode_blocks_end; i++) {
        // Check for bounds just in case
        if (i < total_blocks) { 
            // Mark as allocated
            fs->free_blocks[i] = true;
        }
    }

    // Scanning The Inode Tables
    for (uint32_t i = 1; i <= inode_blocks_end; i++) {
        Block inode_buffer;
        // Attempt to copy the inode data on the disk into inode_buffer
        if (disk_read(fs->disk, i, inode_buffer.data) < 0)
        {
            perror("fs_mount: failed to read Inode table");
            free(fs->meta_data);
            free(fs->free_blocks);
            return false;
        }
        // Iterate through all the inodes inside the block
        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++)
        {
            // Check if inode is valid
            if (inode_buffer.inodes[j].valid == true )
                {
                for (uint32_t k = 0; k < POINTERS_PER_NODE; k++)
                {
                    uint32_t block_num = inode_buffer.inodes[j].direct[k];
                    // Check if pointer is non-zero AND within total disk bounds
                    if (block_num != 0 && block_num < fs->meta_data->blocks) {
                        fs->free_blocks[block_num] = true; // Mark as Allocated
                    }
                }
                uint32_t indirect_block_num = inode_buffer.inodes[j].indirect;
                uint32_t total_blocks = fs->meta_data->blocks;
                if (indirect_block_num != 0 && indirect_block_num < total_blocks) {
                    fs->free_blocks[indirect_block_num] = true;

                    Block indirect_buffer;
                    // Attempt to copy the indirect pointer into the indirect_buffer (basically the address)
                    if (disk_read(fs->disk, indirect_block_num, indirect_buffer.data) < 0) {
                        // Handle the read error and CLEAN UP ALL allocated memory
                        perror("fs_mount: Failed to read indirect block during scan");
                        free(fs->meta_data);
                        free(fs->free_blocks);
                        return false;
                    }

                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                            uint32_t data_block_num = indirect_buffer.pointers[k];

                            // Check if the pointer is non-zero AND within total disk bounds
                            if (data_block_num != 0 && data_block_num < total_blocks) {
                                fs->free_blocks[data_block_num] = true; // Mark the data block as allocated
                            }
                    }
                }
                
            }
        }
        
    }
    disk->mounted=true;
    return true;
}


void fs_unmount(FileSystem *fs) {
    // Error Checks
    if (fs == NULL) {
        perror("fs_unmount: Error file system pointer is NULL");
        return;
    }
    
    // Check disk pointer 
    if (fs->disk == NULL) {
        perror("fs_unmount: Warning, disk pointer is NULL but continuing cleanup");
    } else if (!fs->disk->mounted) { 
        fprintf(stderr, "fs_unmount: Warning disk is already unmounted\n");
        // Continue cleanup in case memory was still allocated
    }

    // Memory Cleanup 
    if (fs->meta_data != NULL) {
        free(fs->meta_data);
        fs->meta_data = NULL;
    }
    if (fs->free_blocks != NULL) {
        free(fs->free_blocks);
        fs->free_blocks = NULL;
    }

    if (fs->disk != NULL) {
        // Dettach the disk from the file system
        fs->disk->mounted = false;
        fs->disk = NULL; // Clear the pointer to the disk
    }
}

ssize_t fs_create(FileSystem *fs);
bool fs_remove(FileSystem *fs, size_t inode_number);
ssize_t fs_stat(FileSystem *fs, size_t inode_number);
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset);
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {

}
