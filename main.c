    #include "include/disk.h"
    #include "include/fs.h"
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

void check_result(int status_code, char *name) {
    if (status_code == 1) {
        printf("[DEBUG]: %s worked succesfully", name);
    }
    else if (status_code == 0) {
        printf("[DEBUG]: %s failed", name);
    }
    else if (status_code == -1) {
        printf("[ERROR]: an illegal action occured in %s", name);
    }
}

int main() {
    Disk *disk = NULL;
    FileSystem fs = {0}; 
    
    disk = disk_open("mfs_test_disk.img", 100);
    fs.disk = disk;
    fs_format(disk);
    fs_mount(&fs, disk);
    fs_debug(disk);
    
    
}