/* Directory Layer */

#pragma once

#include "fs.h"


typedef struct DirEntry DirEntry; 
struct DirEntry {
    uint32_t inode_number; // UINT32_MAX = empty/deleted slot
    char name[28]; // Dir Name
};

/* Directory Functions Prototypes (Declarations) */

ssize_t dir_create(FileSystem *fs);
int     dir_add(FileSystem *fs, size_t dir_inode, const char *name, size_t inode_number);
ssize_t dir_lookup(FileSystem *fs, size_t dir_inode, const char *name);
ssize_t dir_remove(FileSystem *fs, size_t inode_dir, const char *name);
