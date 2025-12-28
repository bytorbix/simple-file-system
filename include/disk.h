#ifndef DISK_H
#define DISK_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h> 

// Size of each block
#define BLOCK_SIZE 4096

// Structure of the disk
typedef struct Disk Disk;
struct Disk {
    int     fd;         // File Descriptor
    size_t  blocks;     // Maximum Capacity of blocks
    size_t  reads;      // Reading operations count
    size_t  writes;     // Writing operations count
    bool    mounted;    // Disk mounted status
};

/* Disk Functions Prototypes (Declarations) */

void disk_debug(Disk *disk);
Disk * disk_open(const char *path, size_t blocks);
void disk_close(Disk *disk);
ssize_t disk_write(Disk *disk, size_t block, char *data);
ssize_t disk_read(Disk *disk, size_t block, char *data);
#endif  