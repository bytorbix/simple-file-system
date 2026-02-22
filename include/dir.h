/* Directory Layer */

#ifndef DIR_H
#define DIR_H

#include "fs.h"

/* Directory Functions Prototypes (Declarations) */

ssize_t dir_create(FileSystem *fs);
int     dir_add(FileSystem *fs, size_t dir_inode, const char *name, size_t inode_number);
ssize_t dir_lookup(FileSystem *fs, size_t dir_inode, const char *name);

#endif // DIR_H
