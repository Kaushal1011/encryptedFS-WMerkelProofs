#ifndef CONSTANTS_H
#define CONSTANTS_H

// Constants for the filesystem
#define BLOCK_SIZE 4096 // 4KB block size
#define INODE_SIZE sizeof(inode)
#define INODES_PER_VOLUME 4      // maximum inodes that can be stored in a volume, minimum is 3
#define DATA_BLOCKS_PER_VOLUME 3 // minimum is 2
#define MAX_CHILDREN 1024        // maximum children a directory can have
#define MAX_PATH_LENGTH 256      // maximum length of a path
#define MAX_NAME_LENGTH 256      // maximum length of a name
#define MAX_TYPE_LENGTH 20       // maximum length of a type
#define MAX_DATABLOCKS 64        // maximum data blocks that can be stored in an inode
#define NUMVOLUMES 10
// Utility macro to get the minimum of two values
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif // CONSTANTS_H
