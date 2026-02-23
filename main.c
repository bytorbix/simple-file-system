#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "disk.h"
#include "fs.h"
#include "dir.h"
#include "utils.h"

void print_passed(const char* message) { printf("[OK]:   %s\n", message); }
void print_failed(const char* message) { printf("[FAIL]: %s\n", message); }

void bitmap_stats(FileSystem *fs)
{
    uint32_t total = fs->meta_data->blocks;
    uint32_t used = 0;
    for (uint32_t i = 0; i < total; i++)
        if (get_bit(fs->bitmap->bits, i)) used++;

    printf("bitmap: %u/%u blocks used, %u free (%u bitmap block(s))\n",
           used, total, total - used, fs->meta_data->bitmap_blocks);
}

void ls(FileSystem *fs, ssize_t dir_inode) // Demo ls command for test case
{
    // Validation check
    if (fs == NULL || fs->disk == NULL) {
        perror("ls: Error fs or disk is invalid (NULL)");
        return;
    }
    if (!fs->disk->mounted)
    {
        fprintf(stderr, "ls: Error disk is not mounted, cannot procceed t\n");
        return;
    }

    // Read the directory inode and confirm it's a directory
    uint32_t inode_block_idx = 1 + (dir_inode / INODES_PER_BLOCK);
    uint32_t inode_offset = dir_inode % INODES_PER_BLOCK;

    Block inode_buf;
    if (disk_read(fs->disk, inode_block_idx, inode_buf.data) < 0) return;

    Inode *target = &inode_buf.inodes[inode_offset]; // our dir inode

    if (target->valid != INODE_DIR)
    {
        perror("ls: Inode given is not a directory.");
        return;
    }
    int count = 0;
    for (size_t i = 0; i < target->size; i+=32) 
    {
        DirEntry entry;
        if (fs_read(fs, dir_inode, (char *)&entry, sizeof(DirEntry), i) < 0) return;
        if ((entry.inode_number != UINT32_MAX)) 
        {
            size_t file_size = fs_stat(fs, (size_t)entry.inode_number);
            printf("File: %d (Inode %d) - %s (Size %ld)\n", count, entry.inode_number, entry.name, file_size);
            count++;
        }
    }
}

void cat(FileSystem *fs, ssize_t inode_file) 
{
    // Validation check
    if (fs == NULL || fs->disk == NULL) {
        perror("cat: Error fs or disk is invalid (NULL)");
        return;
    }
    if (!fs->disk->mounted)
    {
        fprintf(stderr, "cat: Error disk is not mounted, cannot procceed t\n");
        return;
    }

    size_t file_size = fs_stat(fs, inode_file);
    if (file_size <= 0) return;

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        perror("cat: malloc failed");
        return;
    }
    if (fs_read(fs, inode_file, buffer, file_size, 0) < 0) 
    { 
        free(buffer);
        return; 
    }
    buffer[file_size] = '\0';

    int word_count = 0;
    for (size_t i = 0; i < file_size; i++) {
        putchar(buffer[i]);

        if (isspace(buffer[i])) {
            word_count++;

            if (word_count >= 30) {
                putchar('\n');
                word_count = 0;

                while (i + 1 < file_size && isspace(buffer[i + 1])) { // skip extra spaces
                    i++;
                }
            }
        }
    }
    printf("\n");
    free(buffer);
}


int main() {
    Disk *disk = disk_open("mfs_test_disk.img", 10000);
    fs_format(disk);

    FileSystem fs = {0};
    fs_mount(&fs, disk);


    // Allocate inodes
    ssize_t inode_file1 = fs_create(&fs);
    ssize_t inode_file2 = fs_create(&fs);
    ssize_t inode_file3 = fs_create(&fs);

    // Allocate a directory
    ssize_t inode_dir1 = dir_create(&fs);
    ssize_t inode_sub_dir1 = dir_create(&fs);
    ssize_t inode_sub_dir2 = dir_create(&fs);

    // Adding directories
    if (dir_add(&fs, 0, "dir1", inode_dir1) < 0) {
        print_failed("dir_add test_case1 failed");
    }
    if (dir_add(&fs, inode_dir1, "dir2", inode_sub_dir1) < 0) {
        print_failed("dir_add test_case2 failed");
    }
    if (dir_add(&fs, inode_sub_dir1, "dir3", inode_sub_dir2) < 0) {
        print_failed("dir_add test_case3 failed");
    }

    // Adding the files 
    if (dir_add(&fs, inode_sub_dir2, "file1", inode_file1) < 0) {
        print_failed("dir_add test_case4 failed");
    }

    if (dir_add(&fs, inode_sub_dir2, "file2", inode_file2) < 0) {
        print_failed("dir_add test_case5 failed");
    }

    if (dir_add(&fs, inode_sub_dir2, "file3", inode_file3) < 0) {
        print_failed("dir_add test_case6 failed");
    }

    // Writing data into one of the files
    char *text = "Hello World!";
    if (fs_write(&fs, inode_file2, text, strlen(text), 0) < 0) return -1;

    // Lookup the sub directories that holds the files
    ssize_t desired_dir  = fs_lookup(&fs, "/dir1/dir2/dir3");
    
    // List files in the directory
    ls(&fs, desired_dir);

    // print the file with content
    cat(&fs, inode_file2);

    bitmap_stats(&fs);

    // Double Indirect Test (5MB file, exceeds single indirect ~4MB range)
    size_t large_size = 5 * 1024 * 1024;
    char *write_buf = malloc(large_size);
    char *read_buf  = malloc(large_size);
    memset(write_buf, 0xAB, large_size);

    ssize_t inode_large = fs_create(&fs);
    fs_write(&fs, inode_large, write_buf, large_size, 0);
    fs_read(&fs, inode_large, read_buf, large_size, 0);

    if (memcmp(write_buf, read_buf, large_size) == 0)
        print_passed("double indirect: 5MB write/read verified");
    else
        print_failed("double indirect: data mismatch");

    bitmap_stats(&fs);
    fs_remove(&fs, inode_large);
    bitmap_stats(&fs);

    free(write_buf);
    free(read_buf);

    // Close and exit
    fs_unmount(&fs);
    return 0;
}

