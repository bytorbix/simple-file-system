#pragma once

#include "fs.h"
#include <stdbool.h>

#define ENTRY_SIZE (32)
#define ENTRIES_PER_BLOCK (BLOCK_SIZE / ENTRY_SIZE)

typedef struct ExtensionEntry ExtensionEntry;
struct ExtensionEntry {
    char name[28]; // Ext name
    float average; // a variable holding the average file tendancy to grow from it's original size
};


typedef struct pFileSystem pFileSystem;
struct pFileSystem {
    FileSystem *fs;
    ExtensionEntry *entries;
    bool dirty;
};


bool pfs_mount(pFileSystem *pfs, Disk *disk);
bool pfs_format(Disk *disk);
bool pfs_unmount(pFileSystem *pfs);
ssize_t pfs_create(pFileSystem *pfs, const char *path);
ssize_t pfs_write(pFileSystem *pfs, size_t inode_number, char *data, size_t length, size_t offset);
int add_entry(pFileSystem *pfs, const ExtensionEntry *entry);