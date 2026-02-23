#include <stdio.h>
#include "fs.h"
#include <string.h>

bool format_bitmap(Disk *disk, uint32_t inode_blocks, uint32_t bitmap_blocks) 
{
    // Validation checks
    if (disk == NULL) {
        perror("format_bitmap: Error disk is invalid");
        return false;
    }
    if (disk->mounted) {
        perror("format_bitmap: Error disk is mounted, cannot proceed with format");
        return false;
    }
    // Capacity Check
    if (inode_blocks > disk->blocks || bitmap_blocks > disk->blocks) {
        perror("format_bitmap: invalid inode/bitmap blocks given");
        return false;
    }

    // Formatting the bitmap blocks
    for(size_t i = (inode_blocks+1); i <= (bitmap_blocks+inode_blocks); i++) 
    {
        Block buffer;
        memset(buffer.data, 0, BLOCK_SIZE);
        if (disk_write(disk, i, buffer.data) < 0) {
            perror("format_bitmap: writing to disk failed.");
            return false;
        }
    }
    return true;
}

bool save_bitmap(FileSystem *fs) 
{
   if (fs == NULL || fs->disk == NULL || fs->meta_data == NULL || fs->bitmap == NULL) {
        perror("save_bitmap: Error invalid fs");
        return false;
    }
    // Bitmap blocks live right after the inode table (inode_blocks+1+i)
    // writes to disk from the buffer byte after byte (using char casting) up to bitmap_blocks
    for (uint32_t i = 0; i < fs->meta_data->bitmap_blocks; i++) {
        if (disk_write(fs->disk, fs->meta_data->inode_blocks+1+i, (char *)fs->bitmap->bits+i*BLOCK_SIZE) < 0) {
            perror("save_bitmap: Failed to write bitmap block to disk");
            return false;
        }
    }
    return true;
}


bool load_bitmap(FileSystem *fs)
{
    if (fs == NULL || fs->disk == NULL || fs->meta_data == NULL || fs->bitmap == NULL) {
        perror("load_bitmap: Error invalid fs");
        return false;
    }

    // Bitmap blocks live right after the inode table (inode_blocks+1+i)
    // reads the bitmap block from the disk one by one and copies it into the bitmap buffer one byte at a time (we cast the bitmap to char to be able to do that)
    for (uint32_t i = 0; i < fs->meta_data->bitmap_blocks; i++) {
        if (disk_read(fs->disk, fs->meta_data->inode_blocks+1+i, (char *)fs->bitmap->bits+i*BLOCK_SIZE) < 0) {
            perror("load_bitmap: Failed to read bitmap block from disk");
            return false;
        }
    }
    return true;
}
