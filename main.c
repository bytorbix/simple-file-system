#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

int main() {
    // Format and mount
    Disk *disk = disk_open("mfs_test_disk.img", 100);
    fs_format(disk);

    FileSystem fs = {0};
    fs_mount(&fs, disk);
    printf("Formatted and mounted.\n");
    fs_debug(&fs);

    // Create two files
    ssize_t file_a = fs_create(&fs);
    ssize_t file_b = fs_create(&fs);
    printf("\nCreated file A (inode %zd), file B (inode %zd)\n", file_a, file_b);

    // Write small data to file A
    char *msg = "Hello world!";
    ssize_t written = fs_write(&fs, file_a, msg, strlen(msg), 0);
    printf("File A: wrote %zd bytes\n", written);

    // Append to file A
    char *extra = " Goodbye!";
    written = fs_write(&fs, file_a, extra, strlen(extra), strlen(msg));
    printf("File A: appended %zd bytes\n", written);

    // Write multi-block data to file B (spans direct + indirect blocks)
    size_t big_size = BLOCK_SIZE * 6;
    char *big_buf = malloc(big_size);
    memset(big_buf, 'X', big_size);
    written = fs_write(&fs, file_b, big_buf, big_size, 0);
    printf("File B: wrote %zd bytes (direct + indirect)\n", written);

    // Stat
    printf("\nFile A size: %zd bytes\n", fs_stat(&fs, file_a));
    printf("File B size: %zd bytes\n", fs_stat(&fs, file_b));

    // Read file A
    char buf[256] = {0};
    ssize_t rd = fs_read(&fs, file_a, buf, sizeof(buf), 0);
    printf("\nFile A content: \"%.*s\"\n", (int)rd, buf);

    // Read from file B indirect region
    char buf2[32] = {0};
    rd = fs_read(&fs, file_b, buf2, sizeof(buf2), BLOCK_SIZE * 5);
    printf("File B indirect read: all 'X'? %s\n",
           (rd > 0 && buf2[0] == 'X' && buf2[31] == 'X') ? "YES" : "NO");

    // Remove file B
    fs_remove(&fs, file_b);
    printf("\nRemoved file B. Stat: %zd (expected -1)\n", fs_stat(&fs, file_b));

    // Reuse freed inode
    ssize_t file_c = fs_create(&fs);
    printf("Created file C (inode %zd) - reused inode %zd\n", file_c, file_b);

    // Unmount and remount to verify persistence
    fs_unmount(&fs);
    printf("\nUnmounted. Remounting...\n");

    Disk *disk2 = disk_open("mfs_test_disk.img", 100);
    FileSystem fs2 = {0};
    fs_mount(&fs2, disk2);

    memset(buf, 0, sizeof(buf));
    rd = fs_read(&fs2, file_a, buf, sizeof(buf), 0);
    printf("File A after remount: \"%.*s\"\n", (int)rd, buf);

    fs_unmount(&fs2);
    free(big_buf);
    printf("\nDone!\n");
    return 0;
}
