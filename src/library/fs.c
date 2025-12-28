#include "fs.h"
#include "disk.h"
#include "utils.h"
#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <math.h>

/* File System Functions Definitions */




void fs_debug(FileSystem *fs) {
    if (fs->disk == NULL || fs == NULL) {
        perror("fs_debug: Error disk is invalid");
        return;
    }
    if (!(fs->disk->mounted)) { 
        fprintf(stderr, "fs_debug: Error disk is not mounted, aborting...\n");
        return;
    }

    printf("SuperBlock\n");
    SuperBlock *super = fs->meta_data;
    bool magic_number = (super->magic_number == 0xf0f03410);
    printf("\tMagic Number is %s\n", (magic_number) ? "Valid" : "Invalid");
    printf("\tTotal Blocks: %d\n", super->blocks);
    printf("\tInode Blocks: %d\n", super->inode_blocks);
    printf("\tTotal Inodes: %d\n", super->inodes);

    printf("Bitmap\n");
    
}

bool fs_bitmap_to_disk(FileSystem *fs) {
    if (fs->disk == NULL || fs == NULL) {
        perror("fs_debug: Error disk is invalid");
        return false;
    }
    if (fs->bitmap == NULL) {
        perror("fs_bitmap_to_disk: Error bitmap is invalid");
        return false;
    }

    Block buffer;
    // Clean the buffer so unused bits are zero
    memset(buffer.data, 0, BLOCK_SIZE);

    // Calculate the EXACT size of bitmap in bytes
    uint32_t total_blocks = fs->meta_data->blocks;
    uint32_t bitmap_words = (total_blocks + BITS_PER_WORD - 1) / BITS_PER_WORD;
    size_t bitmap_size_bytes = bitmap_words * sizeof(uint32_t);

    memcpy(buffer.data, fs->bitmap, bitmap_size_bytes);

    ssize_t bitmap_block = fs->meta_data->inode_blocks + 1; 
    if (disk_write(fs->disk, bitmap_block, buffer.data) < 0) {
        perror("fs_bitmap_to_disk: Failed to write Bitmap to disk");
        return false;
    }
    return true;
}


bool fs_load_bitmap(FileSystem *fs) {
    if (fs == NULL || fs->disk == NULL || fs->meta_data == NULL || fs->bitmap == NULL) {
        return false;
    }

    uint32_t total_blocks = fs->meta_data->blocks;
    uint32_t bitmap_words = (total_blocks + 32 - 1) / 32;
    size_t bitmap_bytes = bitmap_words * sizeof(uint32_t);

    ssize_t bitmap_block_num = fs->meta_data->inode_blocks + 1;
    Block buffer;

    if (disk_read(fs->disk, bitmap_block_num, buffer.data) < 0) {
        perror("fs_load_bitmap: Failed to read bitmap block from disk");
        return false;
    }

    memcpy(fs->bitmap, buffer.data, bitmap_bytes);

    return true;
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
    if (1 + superblock.inode_blocks > superblock.blocks+1) {
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
    

    memset(block_buffer.data, 0, BLOCK_SIZE);

    // Clean the inode table
    for (uint32_t i = 1; i <= superblock.inode_blocks+1; i++) {
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

    fs->disk = disk;
    // Allocate memory for the SuperBlock metadata and copy it
    fs->meta_data = (SuperBlock *)malloc(sizeof(SuperBlock));
    if (fs->meta_data == NULL) return false;
    *(fs->meta_data) = superblock; // Copy the structure contents

    uint32_t total_inodes = fs->meta_data->inodes;
    uint32_t total_blocks = fs->meta_data->blocks;
    uint32_t meta_data_blocks = fs->meta_data->inode_blocks + 1 + 1;
    uint32_t inode_blocks_end = fs->meta_data->inode_blocks;
    

    // Bitmap
    ssize_t bitmap_block = fs->meta_data->inode_blocks + 1; // Bitmap always sits right after the inode table
    uint32_t bitmap_words = (total_blocks + BITS_PER_WORD -1)  / BITS_PER_WORD;


    fs->bitmap = (uint32_t *)calloc(bitmap_words, sizeof(uint32_t));
    if (fs->bitmap == NULL) {
        perror("fs_mount: Failed to allocate bitmap");
        free(fs->meta_data);
        return false;
    }

    bool bitmap_loaded_valid = fs_load_bitmap(fs);
    if (bitmap_loaded_valid) {
        if (get_bit(fs->bitmap, 0) == 0) {
            bitmap_loaded_valid = false; 
        }
    }

    // Try to load from disk. 
    if (!bitmap_loaded_valid) 
    {
        memset(fs->bitmap, 0, bitmap_words * sizeof(uint32_t));

        // Mark Metadata blocks as allocated
        for(uint32_t k=0; k < meta_data_blocks; k++) {
            set_bit(fs->bitmap, k, 1);
        }

        // Scan Inodes to mark data blocks
        // Loop through Inode Blocks (starts at block 1)
        for (size_t i = 1; i <= fs->meta_data->inode_blocks; i++)
        {
            Block inode_buffer;
            if (disk_read(fs->disk, i, inode_buffer.data) < 0) {
                free(fs->meta_data);
                free(fs->bitmap);
                return false;
            }

            for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
                // Check Direct Pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    uint32_t block_num = inode_buffer.inodes[j].direct[k];
                    // Check if pointer is non-zero AND within total disk bounds
                    if (block_num != 0 && block_num < fs->meta_data->blocks) { 
                        set_bit(fs->bitmap, block_num, 1); // Mark as Allocated
                    }
                }

                uint32_t indirect_block_num = inode_buffer.inodes[j].indirect;
                uint32_t total_blocks = fs->meta_data->blocks;

                if (indirect_block_num != 0 && indirect_block_num < total_blocks) {
                    set_bit(fs->bitmap, indirect_block_num, 1);

                    Block indirect_buffer;
                    // Attempt to copy the indirect pointer into the indirect_buffer (basically the address)
                    if (disk_read(fs->disk, indirect_block_num, indirect_buffer.data) < 0) {
                        // Handle the read error and CLEAN UP ALL allocated memory
                        perror("fs_mount: Failed to read indirect block during scan");
                        free(fs->meta_data);
                        free(fs->bitmap);
                        free(fs->ibitmap);
                        return false;
                    }

                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        uint32_t data_block_num = indirect_buffer.pointers[k];

                        // Check if the pointer is non-zero AND within total disk bounds
                        if (data_block_num != 0 && data_block_num < total_blocks) {
                            set_bit(fs->bitmap, data_block_num, 1); // Mark the data block as allocated
                        }
                    }
                }
            }    
        }
    }

    // Inodes Bitmap
    // ibitmap in it's initial form iterates through all the inodes and read if valid or not
    uint32_t ibitmap_words = (total_inodes + BITS_PER_WORD - 1) / BITS_PER_WORD;
    uint32_t *ibitmap = calloc(ibitmap_words, sizeof(uint32_t));
    if (ibitmap == NULL) {
        perror("fs_mount: Failed to allocate memory for ibitmap array");
        free(fs->meta_data);
        return false;
    }
    fs->ibitmap = ibitmap;

    uint32_t current_inode_id = 0;
    for (size_t i = 1; i <= fs->meta_data->inode_blocks; i++)
    {
        Block inode_buffer;
        if (disk_read(fs->disk, i, inode_buffer.data) < 0) {
            // Error handling (free memory and return false)
            free(fs->meta_data);
            free(fs->bitmap);
            free(fs->ibitmap);
            return false;
        }

        for (size_t j = 0; j < INODES_PER_BLOCK; j++)
        {
            if (current_inode_id >= total_inodes) break;
            if (inode_buffer.inodes[j].valid) {
                set_bit(fs->ibitmap, current_inode_id, 1);
            }
            current_inode_id++;
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
    if (fs->bitmap != NULL) {
        free(fs->bitmap);
        fs->bitmap = NULL;
    }

    if (fs->ibitmap != NULL) {
        free(fs->ibitmap);
        fs->ibitmap = NULL;
    }

    if (fs->disk != NULL) {
        disk_close(fs->disk); 
        fs->disk = NULL;
    }
}


/**
 * @brief Attempts to find and allocate a contiguous run of blocks using the First Fit algorithm.
 *
 * @param fs Pointer to the FileSystem structure.
 * @param blocks_to_reserve The number of contiguous blocks requested.
 * @return size_t* A pointer to an array of allocated block numbers (indices) on success.
 * The array must be freed by the caller. Returns NULL on failure (disk full or fragmentation).
 */
size_t* fs_allocate(FileSystem *fs, size_t blocks_to_reserve) {
    // Initial Validation Checks
    if (fs == NULL || fs->meta_data == NULL || fs->bitmap == NULL || fs->disk == NULL) { 
        perror("fs_allocate: Error fs, metadata, bitmap, or disk is invalid (NULL)"); 
        return NULL;
    }

    if (!(fs->disk->mounted))
    { 
        fprintf(stderr, "fs_allocate: Error disk is not mounted, cannot access the disk\n");
        return NULL;
    }

    if (blocks_to_reserve == 0) {
        return NULL; 
    }

    // init of the bitmap
    uint32_t *bitmap = fs->bitmap;
    uint32_t total_blocks = fs->meta_data->blocks;
    size_t meta_blocks = 2 + (fs->meta_data->inode_blocks); 

    // Declaring the allocation blocks array
    size_t *allocated_array = calloc(blocks_to_reserve, sizeof(size_t)); 
    if (allocated_array == NULL) {
        perror("fs_allocate: Failed to allocate memory for return array");
        return NULL; 
    }
    
    // temp_count: a temporary count to achieve the a row of blocks in the amount desired
    // start_block_index: we mark the first block index of the row that fits the desired blocks_to_reserve
    size_t temp_count = 0;
    size_t start_block_index = 0;

    // Iteration of the whole blocks on the disk
    for (size_t i = meta_blocks; i < total_blocks; i++)
    {
        // Checking if a block is not allocated (free)
        if (!(get_bit(bitmap, i))) 
        {
            // Marking the first block index
            if (temp_count == 0) {
                start_block_index = i; 
            }
            temp_count++; 

            // Row Found, Proceeding with allocation
            if (temp_count == blocks_to_reserve) {
                // Inner loop for saving the results on the allocation array
                for (size_t j = 0; j < blocks_to_reserve; j++)
                {
                    // Marking the index block to iterate inside the row
                    size_t block_index = start_block_index + j;
                    
                    // Attempt to allocate the block in the bitmap (set the bit to 1)
                    set_bit(bitmap, block_index, 1);
                    allocated_array[j] = block_index;

                }
                // Returning the allocated array
                return allocated_array;
            }
        }
        // Block is allocated
        else { 
            temp_count = 0; // Reset the counter
            
        }
    }

    // Failure, loop finished without finding enough space for the row
    free(allocated_array);
    fprintf(stderr, "fs_allocate: Not enough contiguous space available for %zu blocks (Fragmentation).\n", blocks_to_reserve);
    return NULL;
}


ssize_t fs_create(FileSystem *fs) {
    // Validation check
    if (fs == NULL || fs->disk == NULL) {
        perror("fs_create: Error fs or disk is invalid (NULL)"); 
        return -1;
    }
    if (!fs->disk->mounted) { 
        fprintf(stderr, "fs_create: Error disk is not mounted, cannot procceed t\n");
        return -1;
    }

    int inode_num = -1;
    uint32_t *ibitmap = fs->ibitmap;
    uint32_t total_inodes = fs->meta_data->inodes;

    for (size_t i = 0; i < total_inodes; i++)
    {
        if (!(get_bit(ibitmap, i))) {
            inode_num = i;
            set_bit(ibitmap, i, 1);
            break;
        }
    }

    if (inode_num == -1) return -1;

    uint32_t block_idx = 1 + (inode_num / INODES_PER_BLOCK);
    uint32_t offset = inode_num % INODES_PER_BLOCK;

    Block buffer;
    if (disk_read(fs->disk, block_idx, buffer.data) < 0) return -1;

    Inode *target = &buffer.inodes[offset];
    target->valid = 1;
    target->size = 0;

    // Zeroing Out pointers
    for (size_t i = 0; i < 5; i++) target->direct[i] = 0;
    target->indirect=0;


    if (disk_write(fs->disk, block_idx, buffer.data) < 0) return -1;
    return (ssize_t)inode_num;
}

ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) 
{
    // Validation check
     if (fs == NULL || fs->disk == NULL) {
        perror("fs_write: Error fs or disk is invalid (NULL)"); 
        return -1;
    }
    if (!fs->disk->mounted) { 
        fprintf(stderr, "fs_write: Error disk is not mounted, cannot procceed t\n");
        return -1;
    }
    if (inode_number >= fs->meta_data->inodes) {
        // Inode number is invalid (too high)
        fprintf(stderr, "fs_write: Error inode_number is out of bounds, cannot procceed t\n");
        return -1;
    }

    
    

}

bool fs_remove(FileSystem *fs, size_t inode_number) {return false;}
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {return -1;}
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {return -1;}
