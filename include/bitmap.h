    #ifndef BITMAP_H
    #define BITMAP_H

    #include <stdbool.h>
    #include <stdint.h>

    typedef struct Disk Disk;
    typedef struct FileSystem FileSystem;

    typedef struct Bitmap Bitmap;
    struct Bitmap
    {
        bool dirty;
        uint32_t *bits;     // Bitmap array Cache
    };

    bool format_bitmap(Disk *disk, uint32_t inode_blocks, uint32_t bitmap_blocks);
    bool save_bitmap(FileSystem *fs);
    bool load_bitmap(FileSystem *fs);

    #endif // BITMAP_H