#ifndef CONSTANTS_H
#define CONSTANTS_H

// Constants for the filesystem
#define BLOCK_SIZE 4096
#define INODE_SIZE 512
#define INODES_PER_VOLUME 32768
#define DATA_BLOCKS_PER_VOLUME 32768
#define MAX_CHILDREN 128
#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 256
#define MAX_TYPE_LENGTH 20
#define MAX_DATABLOCKS 64
#define ENCRYPTION_KEY_SIZE 32 // AES-256
#define AES_GCM_NONCE_SIZE 12
#define AES_GCM_TAG_SIZE 16

// Utility macro to get the minimum of two values
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif // CONSTANTS_H
