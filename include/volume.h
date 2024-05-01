#ifndef VOLUME_H
#define VOLUME_H

#include <sys/types.h>
#include "constants.h"
#include "merkle.h"

typedef enum volume_type
{
    LOCAL,  // Local volume (on the same machine as the file system)
    AWS,    // AWS S3 volume (remote volume) - Not implemented
    GDRIVE, // Google Drive volume (remote volume) - In Progress
    FTP     // FTP SERVER volume (remote volume) - Not implemented
} volume_type;

typedef struct volume_info
{
    char inodes_path[MAX_PATH_LENGTH]; // Path to the file storing the inodes
    char bitmap_path[MAX_PATH_LENGTH]; // Path to the file storing the bitmap
    char volume_path[MAX_PATH_LENGTH]; // Path to the file storing the volume data
    char merkle_path[MAX_PATH_LENGTH]; // Path to the file storing the Merkle tree
    int inodes_count;                  // Number of inodes in the volume
    int blocks_count;                  // Number of data blocks in the volume
    MerkleTree *merkle_tree;           // Pointer to the Merkle tree of this volume
} volume_info_t;

typedef struct superblock
{
    int volume_count;                  // Number of volumes
    int block_size;                    // Size of a block in bytes
    int inode_size;                    // Size of an inode in bytes
    volume_type vtype;                 // Type of volume
    volume_info_t volumes[NUMVOLUMES]; // Array of volume_info_t structures, Maximum defined in constants
} superblock_t;

extern superblock_t sb; // Global superblock for the file system mounted

extern char superblock_path[MAX_PATH_LENGTH]; // Path to the file storing the superblock

// Function prototypes for volume operations
void init_volume(volume_info_t *volume, const char *path, volume_type type, int i);
void load_or_create_superblock(const char *path, superblock_t *sb);
void init_superblock_local(superblock_t *sb);
void create_volume_files_local(int i, superblock_t *sb);
void read_volume_block(int block_index, void *buf);
void read_volume_block_no_check(int block_index, void *buf);
void write_volume_block(int block_index, const void *buf, size_t buf_size);
void load_or_create_remote_superblock(const char *path, superblock_t *sb);

#endif // VOLUME_H
