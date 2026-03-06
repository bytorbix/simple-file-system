#include "fs.h"
#include <stdbool.h>
#include "pfs.h"
#include "utils.h"


bool pfs_format(Disk *disk)
{
    return fs_format(disk);
}

bool pfs_mount(pFileSystem *pfs, Disk *disk) 
{
    pfs->fs = calloc(1, sizeof(FileSystem));
    if (pfs->fs == NULL) return false;

    bool flag = fs_mount(pfs->fs, disk);
    if (flag) 
    {
        Block buffer;
        memset(buffer.data, 0, BLOCK_SIZE);
        size_t pfs_block = pfs->fs->meta_data->inode_blocks + pfs->fs->meta_data->bitmap_blocks + 1;
        if (disk_read(disk, pfs_block, buffer.data) < 0) {
            perror("pfs_mount: Failed to read from disk");
            return false;
        }

        pfs->entries = (ExtensionEntry*)calloc(ENTRIES_PER_BLOCK, sizeof(ExtensionEntry));
        if (pfs->entries == NULL) {
            return false;
        }
        ExtensionEntry *ptr = buffer.data;

        for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++) {
            if (ptr[i].name[0] != '\0') {
                memcpy(&pfs->entries[i], &ptr[i],sizeof(ExtensionEntry));
            }
        }
        return true;
    }
    else 
    {
        free(pfs->fs);
        free(pfs->entries);
        
        return false;
    }
}

ssize_t pfs_create(pFileSystem *pfs, const char *path) 
{
    if (pfs->fs == NULL) return false;
    // get inode
    ssize_t inode_file = fs_create(pfs->fs);
    // check if we managed to get allocated an inode
    if (inode_file != -1) {
        // File has been created and inode is given
        // extract the file components
        char *parentdir_path = extract_parentdir(path);
        char *filename = extract_filename(path);
        // validation checks
        if (parentdir_path == NULL || filename == NULL) 
        {
            free(parentdir_path);
            return -1;
        }
        // retrieve the parent directory inode
        ssize_t inode_parentdir = fs_lookup(pfs->fs, parentdir_path);
        if (inode_parentdir == -1) 
        {
            free(parentdir_path);
            return -1;
        }
        // adding the file entry into the directory
        if (dir_add(pfs->fs, inode_parentdir, filename, inode_file) < 0)
        {
            free(parentdir_path);
            return -1;
        }

        // Extract Extension and add it to the entries 
        char *extension = extract_extension(filename);
        if (extension != NULL) 
        {   
            ExtensionEntry entry;
            strcpy(entry.name, extension);
            add_entry(pfs, &entry);
        }


        // cleanup and return
        free(parentdir_path);
        return inode_file;
    }
    else {
        return -1;
    }
}

ssize_t pfs_write(pFileSystem *pfs, size_t inode_number, char *data, size_t length, size_t offset) 
{
    if (pfs == NULL) {
        perror("pfs_write: Error pfs is invalid");
        return -1;
    }
    ssize_t flag = fs_write(pfs->fs, inode_number, data, length, offset);
    if (flag == -1) {
        return -1;
    }
    if (flag) 
    {
        // Do something related to pfs
    }
    return flag;
}

int add_entry(pFileSystem *pfs, const ExtensionEntry *entry) 
{   
    // validation check
    if (entry->name[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; i < ENTRIES_PER_BLOCK; i++) 
    {
        ExtensionEntry *temp_entry = &pfs->entries[i];
        if (strcmp(temp_entry->name, entry->name) == 0) 
        {
            perror("add_entry: found a duplicate, skipping..");
            return 1;
        }
        if (*temp_entry->name == '\0') 
        {
            memcpy(&pfs->entries[i], entry, ENTRY_SIZE);
            return 0;
        }
    }
    return -1;
}