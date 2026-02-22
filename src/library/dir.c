#include "fs.h"
#include "dir.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


ssize_t dir_create(FileSystem *fs)
{
    // Validation check
    if (fs == NULL || fs->disk == NULL) {
        perror("dir_create: Error fs or disk is invalid (NULL)");
        return -1;
    }
    if (!fs->disk->mounted) {
        fprintf(stderr, "dir_create: Error disk is not mounted, cannot procceed t\n");
        return -1;
    }

    uint32_t *ibitmap = fs->ibitmap;
    int inode_num = -1;
    for (size_t i = 0; i < fs->meta_data->inodes; i++)
    {
        if (!(get_bit(ibitmap, i))) {
            inode_num = i;
            set_bit(ibitmap, i, 1);
            break;
        }
    }

    if (inode_num == -1) return -1;

    // Locate and write the inode
    uint32_t block_idx = 1 + (inode_num / INODES_PER_BLOCK);
    uint32_t offset = inode_num % INODES_PER_BLOCK;

    Block buffer;
    if (disk_read(fs->disk, block_idx, buffer.data) < 0) return -1;

    Inode *target = &buffer.inodes[offset];
    target->valid = INODE_DIR;
    target->size = 0;
    // Zeroing Out pointers
    for (size_t i = 0; i < 5; i++) target->direct[i] = 0;
    target->indirect = 0;

    if (disk_write(fs->disk, block_idx, buffer.data) < 0) return -1;
    return (ssize_t)inode_num;
}

int dir_add(FileSystem *fs, size_t dir_inode, const char *name, size_t inode_number)
{
    // Validation check
    if (fs == NULL || fs->disk == NULL || name == NULL) {
        perror("dir_add: Error fs, disk or name is invalid (NULL)");
        return -1;
    }
    if (!fs->disk->mounted) {
        fprintf(stderr, "dir_add: Error disk is not mounted, cannot procceed t\n");
        return -1;
    }
    if (strlen(name) >= 28) {
        perror("dir_add: Error, name exceeds the possible length (28 Chars)");
        return -1;
    }

    uint32_t inode_block_idx = 1 + (dir_inode / INODES_PER_BLOCK);
    uint32_t inode_offset = dir_inode % INODES_PER_BLOCK;

    Block inode_buf;
    if (disk_read(fs->disk, inode_block_idx, inode_buf.data) < 0) {
        return -1;
    }
    Inode *target = &inode_buf.inodes[inode_offset];

    if (target->valid != INODE_DIR)
    {
        perror("dir_add: Inode given is not a directory.");
        return -1;
    }

    ssize_t available_slot = -1;
    for (size_t i = 0; i < target->size; i += 32)
    {
        DirEntry entry;
        if (fs_read(fs, dir_inode, (char *)&entry, sizeof(DirEntry), i) < 0) {
            return -1;
        }

        if (strcmp(entry.name, name) == 0) // duplicate
        {
            return -1;
        }
        if (entry.inode_number == 0 && available_slot == -1) // save the first empty slot
        {
            available_slot = i;
        }
    }

    size_t write_offset = (available_slot != -1) ? available_slot : target->size;

    DirEntry new_entry;
    memset(&new_entry, 0, sizeof(DirEntry));
    new_entry.inode_number = inode_number;
    strncpy(new_entry.name, name, 28);
    if (fs_write(fs, dir_inode, (char *)&new_entry, sizeof(DirEntry), write_offset) < 0) {
        return -1;
    }
    return 0;
}

ssize_t dir_lookup(FileSystem *fs, size_t dir_inode, const char *name) 
{
    // Validation check
    if (fs == NULL || fs->disk == NULL || name == NULL) {
        perror("dir_lookup: Error fs, disk or name is invalid (NULL)");
        return -1;
    }
    if (!fs->disk->mounted) 
    {
        fprintf(stderr, "dir_lookup: Error disk is not mounted, cannot procceed t\n");
        return -1;
    }
    if (strlen(name) >= 28) {
        perror("dir_lookup: Error, name exceeds the possible length (28 Chars)");
        return -1;
    }

    uint32_t inode_block_idx = 1 + (dir_inode / INODES_PER_BLOCK);
    uint32_t inode_offset = dir_inode % INODES_PER_BLOCK;

    Block inode_buf;
    if (disk_read(fs->disk, inode_block_idx, inode_buf.data) < 0) {
        return -1;
    }
    Inode *target = &inode_buf.inodes[inode_offset];

    if (target->valid != INODE_DIR)
    {
        perror("dir_lookup: Inode given is not a directory.");
        return -1;
    }

    for (size_t i = 0; i < target->size; i += 32)
    {
        DirEntry entry;
        if (fs_read(fs, dir_inode, (char *)&entry, sizeof(DirEntry), i) < 0) {
            return -1;
        }
        if (entry.inode_number != 0 && strcmp(entry.name, name) == 0) {
            return (ssize_t)entry.inode_number;
        }
    }
    return -1;
}