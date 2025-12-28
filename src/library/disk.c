
#include "disk.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h> 

/* Disk Functions Definitions */

void disk_debug(Disk *disk) {
    if (disk == NULL) {
        return;
    }

    printf("--------Disk Metadata--------\n");
    printf("Disk is %s\n", (disk->mounted) ? "Mounted" : "Not Mounted");
    if (disk->mounted) {
        printf("Sum of write operations on the disk: %ld\n", disk->writes);
        printf("Sum of read operations on the disk: %ld\n", disk->reads);
    }
}

/* Opens the file and returns the pointer to the virtual disk*/
Disk * disk_open(const char *path, size_t blocks) {

    // Attempt to open the file and receive the proccess id (fd)
    // if the file does not exist, a new one will be created.
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("disk_open: failed to open/create disk image");
        return NULL;
    }

    // free up space and initiate the disk object and the pointer
    Disk *disk = (Disk *)calloc(1, sizeof(Disk));
    if (disk == NULL) {
        close(fd); 
        return NULL;
    }

    // calc size and resize if needed
    off_t total_size = (off_t)blocks * BLOCK_SIZE;
    if (ftruncate(fd, total_size) < 0) {
        perror("disk_open: failed to resize disk image");
        close(fd);
        free(disk); 
        return NULL;
    }

    // final construction 
    disk->fd      = fd;
    disk->blocks  = blocks;

    return disk;
}

 void disk_close(Disk *disk) {
    // error checks
    if (disk == NULL) {
        return;
    }
    if (disk->fd >= 0) {
        if (close(disk->fd) < 0) {
            perror("disk_close: Error closing the disk");
        }
    }
    // unmount the disk and free it from the memory
    disk->mounted = false;
    disk->fd = -1; 
    free(disk);
}

ssize_t disk_write(Disk *disk, size_t block, char *data) {
    if (disk == NULL) {
        perror("disk_write: disk is invalid");
        return -1;
    }
    if (block >= disk->blocks) {
        perror("disk_write: block size is invalid");
        return -1;
    }

    // position the offset in the file at desired block location
    off_t offset = (off_t)block * BLOCK_SIZE;
    if (lseek(disk->fd, offset, SEEK_SET) < 0) {
        perror("disk_write: lseek has failed");
        return -1;
    }

    // attempt to write the data into the disk
    ssize_t written_bytes = write(disk->fd, data, BLOCK_SIZE);
    if (written_bytes != BLOCK_SIZE) {
        if (written_bytes < 0) {
            perror("disk_write: write system call failed");
        } else {
            fprintf(stderr, "disk_write: short write (%zd bytes written instead of %d)\n", written_bytes, BLOCK_SIZE);
        }
        return -1;
    }
    // success
    // write operation count is incremented
    disk->writes++;
    return written_bytes;
}

ssize_t disk_read(Disk *disk, size_t block, char *data) {
    if (disk == NULL) {
        perror("disk_read: disk is invalid (NULL pointer)");
        return -1;
    }
    if (block >= disk->blocks) {
        fprintf(stderr, "disk_read: block number %zu out of bounds (max %zu)\n", 
                block, disk->blocks - 1);
        return -1;
    }
    off_t offset = (off_t)block * BLOCK_SIZE;

    if (lseek(disk->fd, offset, SEEK_SET) < 0) {
        perror("disk_read: lseek has failed");
        return -1;
    }

    // attempt to read the data from the disk into the buffer data
    ssize_t bytes_read = read(disk->fd, data, BLOCK_SIZE);
    if (bytes_read != BLOCK_SIZE) {
        if (bytes_read < 0) {
            perror("disk_read: system read call failed");
        } else if (bytes_read == 0) {
            fprintf(stderr, "disk_read: read failed, unexpectedly hit End-of-File (EOF)\n");
        } else { 
            fprintf(stderr, "disk_read: short read (%zd bytes read instead of %d)\n", 
                    bytes_read, BLOCK_SIZE);
        }
        return -1; 
    }

    // increment the read operations and return the buffer
    disk->reads++;
    return bytes_read; 
}