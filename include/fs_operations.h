#ifndef FS_OPERATIONS_H
#define FS_OPERATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <fuse.h>

#include "bitmap.h"
#include "inode.h"
#include "volume.h"

// Function prototypes
// create a new file
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
// read data from a file
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
//  write data to a file
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// truncate a file
int fs_truncate(const char *path, off_t newsize);
// get file attributes
int fs_getattr(const char *path, struct stat *stbuf);
// open file
int fs_open(const char *path, struct fuse_file_info *fi);
// readdir or ls
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
// mv or rename
int fs_rename(const char *from, const char *to);
//  rm or delete
int fs_unlink(const char *path);

extern const struct fuse_operations fs_operations;

#endif // FS_OPERATIONS_H
