#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "disk.h"

#define BITS_PER_WORD (32)
#define BITS_PER_BITMAP_BLOCK (BLOCK_SIZE*8)

typedef struct Disk Disk;
typedef struct FileSystem FileSystem;

typedef struct Bitmap Bitmap;
struct Bitmap
{
    bool dirty;         // Is the bitmap modified or no
    uint32_t *bits;     // Bitmap array Cache
};

bool format_bitmap(Disk *disk, uint32_t inode_blocks, uint32_t bitmap_blocks);
bool save_bitmap(FileSystem *fs);
bool load_bitmap(FileSystem *fs);

