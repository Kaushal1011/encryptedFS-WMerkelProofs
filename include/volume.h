#ifndef VOLUME_H
#define VOLUME_H

#include <sys/types.h>
#include "constants.h"
#include "merkle.h"

typedef enum volume_type
{
    LOCAL,
    FTP,
    GDRIVE
} volume_type;

typedef struct volume_info
{
    char inodes_path[MAX_PATH_LENGTH];
    char bitmap_path[MAX_PATH_LENGTH];
    char volume_path[MAX_PATH_LENGTH];
    char merkle_path[MAX_PATH_LENGTH];
    int inodes_count;
    int blocks_count;
    volume_type type;
    MerkleTree *merkle_tree; // Pointer to the Merkle tree of this volume
} volume_info_t;

typedef struct superblock
{
    int volume_count;
    int block_size;
    int inode_size;
    volume_info_t volumes[10]; // Assume a maximum of 10 volumes
} superblock_t;

// Function prototypes for volume operations
void init_volume(volume_info_t *volume, const char *path, volume_type type, int i);
void load_or_create_superblock(const char *path, superblock_t *sb);
void init_superblock_local(superblock_t *sb);

#endif // VOLUME_H
