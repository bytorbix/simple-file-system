#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"
#include "utils.h"




int main() {
    Disk *disk = disk_open("mfs_test_disk.img", 100); 
    FileSystem fs = {0}; 
    fs.disk = disk;

    if (fs_mount(&fs, disk)) {
        printf("Disk mounted successfully. Loading existing data...\n");
    } 
    else {
        printf("Mount failed (New disk?). Formatting...\n");
        if (fs_format(disk)) {
            if (!fs_mount(&fs, disk)) {
                fprintf(stderr, "Critical Error: Failed to mount even after formatting.\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Critical Error: Format failed.\n");
            return 1;
        }
    }

    disk_debug(disk);
    fs_debug(&fs);

    size_t *result = fs_allocate(&fs, 5);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate 5 blocks\n");
    } else {
        printf("\nSuccessfully allocated 5 blocks:\n");
        for (size_t i = 0; i < 5; i++) {
            printf("Index [%zu]: Block Number %zu\n", i, result[i]);
        }
        free(result);
    }
    fs_bitmap_to_disk(&fs);


    fs_unmount(&fs); 
    return 0;
}